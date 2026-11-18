using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media.Imaging;
using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Nodes;
namespace GpuPro.Windows;
public sealed partial class MainWindow {
 Process? worker;CancellationTokenSource? workerCancel;
 readonly ComboBox gpuChoice=new(){MinWidth=240,HorizontalAlignment=HorizontalAlignment.Stretch,DisplayMemberPath="Name"};
 List<GpuChoice> inventory=new();
 JsonObject? selectedReport;
 static string KindName(string? kind)=>kind switch{"suite"=>"Teste completo","graphics"=>"Cenas 3D / jogos","compute"=>"Memória e computação","video"=>"Codecs · vídeo e imagem","bottleneck"=>"Gargalo",_=>kind??"teste"};
 object? reportView;Pivot? benchTabs;bool benchTabOpened;
 static readonly string[] benchKinds={"suite","graphics","compute","video","bottleneck"};
 readonly Dictionary<string,Border> resultCards=new();readonly Dictionary<string,StackPanel> historyLists=new();readonly Dictionary<string,JsonObject> shownReports=new();TextBlock? workerStatus;ProgressBar? workerProgress;Image? previewImage;
 record GpuChoice(string Id,string Name,string Vendor,string DeviceId);
 async Task RefreshGpuInventory(){try{var result=await Task.Run(Native.Inventory);inventory=result.EnumerateArray().Select(g=>new GpuChoice(Native.Text(g,"id"),Native.Text(g,"name"),Native.Text(g,"vendor"),Native.Text(g,"deviceId"))).ToList();gpuChoice.ItemsSource=inventory;if(inventory.Count>0){gpuChoice.SelectedIndex=0;foreach(var k in benchKinds)_ = RefreshHistory(k);}else Record("Nenhuma GPU física identificada pelo DXGI.",true);Record("Inventário: "+inventory.Count+" GPU(s) · "+string.Join(", ",inventory.Select(g=>g.Name)));}catch(Exception e){Record("Inventário de GPUs: "+e.Message,true);}}
 GpuChoice ActiveGpu=>gpuChoice.SelectedItem as GpuChoice??throw new InvalidOperationException("Selecione uma GPU.");
 string DeviceKey=>ActiveGpu.Vendor+"/"+ActiveGpu.DeviceId+"/"+ActiveGpu.Id;
 JsonObject PublicGpu()=>new(){["name"]=ActiveGpu.Name,["vendor"]=ActiveGpu.Vendor,["deviceId"]=ActiveGpu.DeviceId};
 async Task<JsonObject> Data(JsonObject request){request["database"]=Path.Combine(dataRoot,"results.sqlite");var start=new ProcessStartInfo(Path.Combine(AppContext.BaseDirectory,"gpu-data-worker.exe")){UseShellExecute=false,RedirectStandardInput=true,StandardInputEncoding=new System.Text.UTF8Encoding(false),RedirectStandardOutput=true,StandardOutputEncoding=System.Text.Encoding.UTF8,RedirectStandardError=true,CreateNoWindow=true};using var p=Process.Start(start)??throw new IOException("Não foi possível iniciar o banco de resultados.");var error=p.StandardError.ReadToEndAsync();var output=p.StandardOutput.ReadToEndAsync();await p.StandardInput.WriteAsync(request.ToJsonString());p.StandardInput.Close();await p.WaitForExitAsync();var response=JsonNode.Parse(await output)?.AsObject()??throw new IOException("Resposta inválida do banco.");if(response["ok"]?.GetValue<bool>()!=true)throw new IOException(response["error"]?.ToString()??await error);return response;}
 void BuildBenchmarkPage(){
  var tabs=new Pivot();foreach(var kind in benchKinds){var content=new StackPanel{Spacing=14};var api=new ComboBox{Header="API",ItemsSource=new[]{"Vulkan","OpenGL"},SelectedIndex=0,Width=190};var resolution=new ComboBox{Header="Resolução",ItemsSource=new[]{"1280 × 720","1920 × 1080","2560 × 1440","3840 × 2160","7680 × 4320"},SelectedIndex=1,Width=200};var duration=new ComboBox{Header="Duração",ItemsSource=new[]{"30 segundos","60 segundos"},SelectedIndex=0,Width=170};var controls=new StackPanel{Orientation=Orientation.Horizontal,Spacing=12};if(kind=="graphics")controls.Children.Add(api);if(kind is "graphics" or "bottleneck")controls.Children.Add(resolution);if(kind=="graphics")controls.Children.Add(duration);if(kind=="video"){var useVideo=new CheckBox{Content="Vídeo · H.264, HEVC e AV1 (FFmpeg)",IsChecked=videoParts.Contains("video")};var useImages=new CheckBox{Content="Imagem · JPEG, PNG e WebP",IsChecked=videoParts.Contains("images")};void Parts(){var list=new List<string>();if(useVideo.IsChecked==true)list.Add("video");if(useImages.IsChecked==true)list.Add("images");videoParts=list.Count>0?string.Join(",",list):"images";}useVideo.Click+=(s,e)=>Parts();useImages.Click+=(s,e)=>Parts();controls.Children.Add(useVideo);controls.Children.Add(useImages);}if(controls.Children.Count>0)content.Children.Add(controls);
   content.Children.Add(Text(kind=="suite"?"Todos os testes em sequência, como um benchmark completo: cenas 3D (Vulkan 1920×1080, 30 s), memória e computação da GPU, codecs de vídeo e de imagem e processador. Leva cerca de 5 a 8 minutos e gera uma pontuação total com a pontuação de cada categoria. Cada teste também pode ser feito sozinho nas outras abas.":kind=="graphics"?"Três cenas 3D procedurais com aquecimento de 2 segundos. Mostra a pontuação 3D e o FPS mínimo, médio e máximo.":kind=="compute"?"Memória da GPU (leitura, escrita, cópia e transferências CPU ↔ GPU pelo PCIe) e computação (FP32, FP64 e inteiros), com pontuação de cada parte.":kind=="video"?"Codificação e decodificação de vídeo por hardware em 720p, 1080p, 1440p e 4K, e de imagens JPEG, PNG e WebP pelo processador, com velocidade, FPS e pontuação. Vídeo requer FFmpeg (Atualizações › Instalar dependências).":"Duas fases Vulkan (referência 1280×720 e a resolução alvo) com carga fixa de CPU. O resultado diz, em porcentagem, quanto a GPU, a CPU e a memória perdem de desempenho nesta carga. A memória só perde quando a VRAM transborda para a RAM do sistema. Estimativa de carga sintética; não é a perda medida em um jogo específico."));
   content.Children.Add(Button(kind=="suite"?"Iniciar teste completo":"Iniciar teste",async()=>{try{if(kind=="suite"){await RunSuite();return;}var dimensions=new[]{(1280,720),(1920,1080),(2560,1440),(3840,2160),(7680,4320)};var size=dimensions[Math.Max(0,resolution.SelectedIndex)];await RunGpuTest(kind,api.SelectedIndex==1?"OpenGL":"Vulkan",size.Item1,size.Item2,duration.SelectedIndex==1?60:30);}catch(Exception e){TestError(e);}}));var card=Section("Resultado · "+KindName(kind),Text("Nenhum resultado exibido. Rode o teste ou escolha um dos últimos resultados abaixo."));resultCards[kind]=card;content.Children.Add(card);
   var tabActions=new StackPanel{Orientation=Orientation.Horizontal,Spacing=12};tabActions.Children.Add(Button("Exportar JSON",async()=>{if(shownReports.TryGetValue(kind,out var r)){selectedReport=r;reportCard=card;}await ExportReport();}));tabActions.Children.Add(Button("Exportar PNG / JPEG",async()=>{if(shownReports.TryGetValue(kind,out var r)){selectedReport=r;reportCard=card;}await ExportReportImage();}));content.Children.Add(tabActions);
   var history=new StackPanel{Spacing=6};historyLists[kind]=history;content.Children.Add(Section("Últimos resultados · "+KindName(kind),history));
   tabs.Items.Add(new PivotItem{Header=KindName(kind),Content=content});}
  benchTabs=tabs;body.Children.Add(tabs);workerStatus=null;workerProgress=new ProgressBar{Minimum=0,Maximum=100};body.Children.Add(workerProgress);body.Children.Add(Button("Cancelar teste",()=>workerCancel?.Cancel()));previewImage=new Image{MaxHeight=360,Stretch=Microsoft.UI.Xaml.Media.Stretch.Uniform};body.Children.Add(previewImage);foreach(var kind in benchKinds){if(shownReports.TryGetValue(kind,out var shown))ShowReport(shown,false);_ = RefreshHistory(kind);}if(selectedReport?["kind"]?.ToString() is string last&&Array.IndexOf(benchKinds,last)>=0)tabs.SelectedIndex=Array.IndexOf(benchKinds,last);var args=Environment.GetCommandLineArgs();if(!benchTabOpened&&Array.IndexOf(args,"--aba") is int at and >0&&at+1<args.Length&&int.TryParse(args[at+1],out var tab)&&tab>=0&&tab<benchKinds.Length){tabs.SelectedIndex=tab;benchTabOpened=true;}
 }
 void TestError(Exception e)=>Record(e.Message,true);
 async Task<JsonObject> ExecuteWorker(string name,IEnumerable<string> args,string? imagePath,CancellationToken cancel){var start=new ProcessStartInfo(Path.Combine(AppContext.BaseDirectory,name)){UseShellExecute=false,RedirectStandardOutput=true,StandardOutputEncoding=System.Text.Encoding.UTF8,RedirectStandardError=true,CreateNoWindow=true,WorkingDirectory=AppContext.BaseDirectory};foreach(var arg in args)start.ArgumentList.Add(arg);var ffmpegPath=Path.Combine(dataRoot,"ffmpeg-path.txt");if(File.Exists(ffmpegPath)){var folder=File.ReadAllText(ffmpegPath).Trim();if(Directory.Exists(folder))start.Environment["PATH"]=folder+Path.PathSeparator+start.Environment["PATH"];}
  using var p=Process.Start(start)??throw new IOException("Executor não iniciou.");worker=p;using var registration=cancel.Register(()=>{try{if(!p.HasExited)p.Kill(true);}catch{}});var errors=p.StandardError.ReadToEndAsync();JsonObject? result=null;string? line;while((line=await p.StandardOutput.ReadLineAsync())!=null){cancel.ThrowIfCancellationRequested();if(line.StartsWith("RESULT "))result=JsonNode.Parse(line[7..])?.AsObject();else if(line.StartsWith("PROGRESS ")){var progressText=line[9..];if(progressText.StartsWith("{")&&JsonNode.Parse(progressText) is JsonObject progressInfo){var pct=progressInfo["progress"]?.GetValue<double>();if(workerStatus!=null)workerStatus.Text="Teste em execução"+(pct.HasValue?$" · {pct.Value:0}%":"");}else if(workerStatus!=null)workerStatus.Text="Teste em execução · "+progressText;if(line[9..].StartsWith("{")&&JsonNode.Parse(line[9..]) is JsonObject progress&&workerProgress!=null)workerProgress.Value=progress["progress"]?.GetValue<double>()??0;}else if(line.StartsWith("FRAME ")&&imagePath!=null&&previewImage!=null){try{using var input=new FileStream(imagePath,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete);var bitmap=new BitmapImage();await bitmap.SetSourceAsync(input.AsRandomAccessStream());previewImage.Source=bitmap;}catch(IOException){}}}await p.WaitForExitAsync();cancel.ThrowIfCancellationRequested();if(result==null)throw new IOException("Executor sem resultado: "+(await errors).TakeLast(3000).Aggregate("",(a,c)=>a+c));return result;
 }
 async Task RunGpuTest(string kind,string api,int width,int height,int seconds){if(worker!=null||benchmark)throw new InvalidOperationException("Aguarde ou cancele o teste em andamento.");var gpu=ActiveGpu;gpuChoice.IsEnabled=false;Record("Teste "+KindName(kind)+" iniciado · "+gpu.Name+(kind is "graphics" or "bottleneck"?" · "+(kind=="bottleneck"?"Vulkan":api)+" · "+width+"×"+height:""));workerCancel=new();try{Directory.CreateDirectory(Path.Combine(dataRoot,"images"));string id=Guid.NewGuid().ToString(),image=Path.Combine(dataRoot,"images",id+".jpg");JsonObject result;
   async Task<JsonObject> Graphics(int w,int h,bool compute=false,int cpu=0){var args=new List<string>{"--pci",gpu.Id,"--api",compute?"Vulkan":api,"--width",w.ToString(),"--height",h.ToString(),"--seconds",seconds.ToString(),"--cpu-iterations",cpu.ToString()};if(compute)args.Add("--compute");else{args.AddRange(new[]{"--live-preview",image,"--preview",Path.Combine(dataRoot,"images",id+".png")});}return await ExecuteWorker("gpu-bench-worker.exe",args,image,workerCancel.Token);}
   if(kind=="video")result=await ExecuteWorker("gpu-video-worker.exe",new[]{"--pci",gpu.Id,"--parts",videoParts},null,workerCancel.Token);else if(kind=="bottleneck"){api="Vulkan";var low=await Graphics(1280,720,false,300000);if(low["valid"]?.GetValue<bool>()!=true)throw new IOException(low["error"]?.ToString());memoryWatch=new MemoryWatch(snapshot);MemoryWatch watched;JsonObject high;try{high=await Graphics(width,height,false,300000);}finally{watched=memoryWatch!;memoryWatch=null;}if(high["valid"]?.GetValue<bool>()!=true)throw new IOException(high["error"]?.ToString());result=BalanceReport(low,high,watched);}else result=await Graphics(width,height,kind=="compute");
   result["id"]=id;result["gpu"]=PublicGpu();result["platform"]="Windows";result["timestamp"]=DateTime.UtcNow.ToString("O");result["kind"]=kind;await Data(new(){["operation"]="save",["device"]=DeviceKey,["record"]=result.DeepClone()});selectedReport=result;ShowReport(result);_ = RefreshHistory(kind);Record("Teste "+KindName(kind)+(result["valid"]?.GetValue<bool>()==true?" concluído":" com cobertura parcial")+" · "+gpu.Name+(result["summary"] is JsonNode summary?" · "+summary.ToString().Replace('\n',' '):result["points"] is JsonNode points?" · "+Points(points).ToString("N0")+" pontos":result["score"] is JsonNode score?" · score "+score+" "+result["scoreUnit"]:"")+" · registro: "+result.ToJsonString());if(workerProgress!=null)workerProgress.Value=100;
  }catch(OperationCanceledException){if(workerStatus!=null)workerStatus.Text="Teste cancelado. Nenhum score válido foi registrado.";Record("Teste "+KindName(kind)+" cancelado pelo usuário.");}finally{worker=null;workerCancel.Dispose();workerCancel=null;gpuChoice.IsEnabled=true;}}
 // Gargalo em linguagem direta: quanto cada componente deixa de render nesta carga.
 // GPU e CPU vêm do modelo de sobreposição de tempos por quadro (core compartilhado com Linux).
 // Memória só recebe porcentagem quando a VRAM transborda para a RAM do sistema durante a
 // fase alvo; a perda é o excesso de tempo de GPU sobre o esperado pela escala de resolução.
 JsonObject BalanceReport(JsonObject low,JsonObject high,MemoryWatch memory){
  double Mean(JsonObject o,string key)=>o["sceneResults"]!.AsArray().Average(row=>row![key]!.GetValue<double>());
  var r=Native.gpp_balance(Mean(high,"cpuWorkMs"),Mean(high,"gpuMeanMs"));
  int width=high["width"]!.GetValue<int>(),height=high["height"]!.GetValue<int>();
  double pixelRatio=width*(double)height/(1280*720.0),expectedGpuMs=Mean(low,"gpuMeanMs")*pixelRatio,gpuMs=Mean(high,"gpuMeanMs");
  const double GiB=1073741824;double growth=Math.Max(0,memory.SharedPeak-memory.SharedBase);
  bool dedicated=memory.VramTotal>0,spill=dedicated&&memory.VramPeak>=memory.VramTotal*.95&&growth>=256*1048576.0;
  double? memoryLoss=!dedicated?null:spill&&gpuMs>expectedGpuMs?100*(1-expectedGpuMs/gpuMs):0;
  bool ramFull=memory.RamTotal>0&&memory.RamPeak>=memory.RamTotal*.95;
  string Pct(double v)=>v.ToString("0")+"%";
  var lines=new JsonArray();
  void Line(string part,double? loss,string detail)=>lines.Add(new JsonObject{["part"]=part,["lossPct"]=loss,["title"]=loss.HasValue?$"{part} perdendo {Pct(loss.Value)} de desempenho":$"{part} · não se aplica",["detail"]=detail});
  Line("GPU",r.GpuLoss,r.GpuLoss<1?"A CPU acompanha a GPU: nada da GPU fica parado nesta carga.":$"A GPU conseguiria {r.GpuPotential:0} FPS, mas a CPU só prepara {r.Model:0} FPS. {r.GpuLost:0} FPS da GPU ficam sem uso.");
  Line("CPU",r.CpuLoss,r.CpuLoss<1?"A GPU acompanha a CPU: nada da CPU fica parado nesta carga.":$"A CPU conseguiria preparar {r.CpuPotential:0} FPS, mas a GPU só desenha {r.Model:0} FPS. {r.CpuLost:0} FPS da CPU ficam sem uso.");
  string ram=memory.RamTotal>0?$"RAM pico {memory.RamPeak/GiB:0.0} de {memory.RamTotal/GiB:0.0} GiB"+(ramFull?" · RAM do sistema no limite, o Windows pode usar o arquivo de paginação":""):"RAM sem leitura";
  Line("Memória",memoryLoss,!dedicated?"GPU sem VRAM dedicada informada; usa a RAM do sistema. "+ram+".":spill?$"A VRAM encheu ({memory.VramPeak/GiB:0.0} de {memory.VramTotal/GiB:0.0} GiB) e {growth/GiB:0.0} GiB transbordaram para a RAM do sistema; cada quadro ficou {Pct(memoryLoss??0)} mais lento que o esperado para {width}×{height}. {ram}.":$"Sem transbordo: VRAM pico {memory.VramPeak/GiB:0.0} de {memory.VramTotal/GiB:0.0} GiB. {ram}.");
  var parts=new[]{("GPU",r.GpuLoss),("CPU",r.CpuLoss),("Memória",memoryLoss??0)};var worst=parts.MaxBy(p=>p.Item2);
  string verdict=worst.Item2<5?"Equilibrado: nenhum componente perde mais de 5% nesta carga.":worst.Item1 switch{"GPU"=>"Gargalo principal: CPU. A GPU espera pela CPU.","CPU"=>"Gargalo principal: GPU. A CPU espera pela GPU.",_=>"Gargalo principal: memória de vídeo. A VRAM não comporta esta resolução."};
  string summary=string.Join("\n",lines.Select(l=>l!["title"]!.ToString()))+"\n"+verdict;
  return new(){["valid"]=r.Valid!=0,["kind"]="bottleneck",["method"]="resolution-cpu-memory-balance-v4",["api"]="Vulkan",["width"]=width,["height"]=height,["gpuLossFromCpuPct"]=r.GpuLoss,["cpuLossFromGpuPct"]=r.CpuLoss,["memoryLossPct"]=memoryLoss,["gpuLostFps"]=r.GpuLost,["cpuLostFps"]=r.CpuLost,["modelFps"]=r.Model,["memory"]=new JsonObject{["vramPeakBytes"]=memory.VramPeak,["vramTotalBytes"]=memory.VramTotal,["sharedGrowthBytes"]=growth,["ramPeakBytes"]=memory.RamPeak,["ramTotalBytes"]=memory.RamTotal,["spill"]=spill,["samples"]=memory.Samples,["expectedGpuMs"]=expectedGpuMs,["measuredGpuMs"]=gpuMs},["lines"]=lines,["verdict"]=verdict,["comparison"]=new JsonArray(low.DeepClone(),high.DeepClone()),["summary"]=summary,["note"]=$"Carga sintética do GPU-PRO: referência 1280×720 e alvo {width}×{height} com carga fixa de CPU. Não é a perda medida em um jogo específico."};
 }
 MemoryWatch? memoryWatch;
 internal sealed class MemoryWatch {
  internal double VramPeak,VramTotal,SharedBase,SharedPeak,RamPeak,RamTotal;internal int Samples;
  internal MemoryWatch(JsonElement start){if(start.ValueKind!=JsonValueKind.Object)return;var gpu=start.GetProperty("gpu");SharedBase=SharedPeak=Native.Number(gpu,"sharedUsed")??0;Add(start);}
  internal void Add(JsonElement s){if(s.ValueKind!=JsonValueKind.Object)return;var gpu=s.GetProperty("gpu");Samples++;VramPeak=Math.Max(VramPeak,Native.Number(gpu,"vram")??0);VramTotal=Math.Max(VramTotal,Native.Number(gpu,"vramTotal")??0);SharedPeak=Math.Max(SharedPeak,Native.Number(gpu,"sharedUsed")??0);RamPeak=Math.Max(RamPeak,Native.Number(s,"ramUsed")??0);RamTotal=Math.Max(RamTotal,Native.Number(s,"ramTotal")??0);}
 }
 UIElement BottleneckView(JsonObject result,JsonArray lines){
  var panel=new StackPanel{Spacing=12};var grid=new Grid{ColumnSpacing=12};int column=0;
  foreach(var row in lines){var line=row!.AsObject();grid.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});var loss=line["lossPct"]?.GetValue<double>();
   var card=new NeonCard(line["part"]+" perdendo");card.Value.Text=loss.HasValue?loss.Value.ToString("0")+"%":"N/A";card.Value.FontSize=34;card.Reading(loss,"usage",theme);
   if(card.Children[0] is Border b&&b.Child is StackPanel inner)inner.Children.Add(new TextBlock{Text=line["detail"]?.ToString(),FontSize=12,TextWrapping=TextWrapping.Wrap,Opacity=.85});
   Grid.SetColumn(card,column++);grid.Children.Add(card);}
  panel.Children.Add(grid);panel.Children.Add(Text(result["verdict"]?.ToString()??"",18));panel.Children.Add(Text(result["note"]?.ToString()??"",12));return panel;
 }
 // Mostra o resultado no quadro da aba do próprio teste e, se pedido, abre essa aba.
 void ShowReport(JsonObject result,bool focus=true){
  RepairText(result);string kind=result["kind"]?.ToString()??"graphics";if(!resultCards.ContainsKey(kind))kind="graphics";
  shownReports[kind]=result;selectedReport=result;
  if(resultCards.TryGetValue(kind,out var card)&&card.Child is StackPanel report){reportCard=card;report.Children.Clear();
   report.Children.Add(Text("GPU-PRO · "+KindName(kind),24));report.Children.Add(Text(result["gpu"]?["name"]+" · "+result["api"]+" · "+result["width"]+" × "+result["height"]));
   if(result["lines"] is JsonArray lines)report.Children.Add(BottleneckView(result,lines));else report.Children.Add(ResultView(result));
   report.Children.Add(Text("Data: "+LocalTime(result["timestamp"])+" · Plataforma: Windows",12));}
  if(focus&&benchTabs!=null&&Array.IndexOf(benchKinds,kind) is int index and >=0)benchTabs.SelectedIndex=index;
  if(focus&&workerStatus!=null)workerStatus.Text=result["summary"]?.ToString()??(result["valid"]?.GetValue<bool>()==true?"Teste concluído e salvo.":result["error"]?.ToString()??"Teste com cobertura parcial. Confira o resultado.");
 }
 static string LocalTime(JsonNode? stamp)=>DateTime.TryParse(stamp?.ToString(),null,System.Globalization.DateTimeStyles.RoundtripKind,out var t)?t.ToLocalTime().ToString("dd/MM/yyyy HH:mm"):stamp?.ToString()??"";
 // Teste 1 (mais recente), 2 e 3 de cada categoria, com data, hora e resultado; um clique abre no quadro da aba.
 async Task RefreshHistory(string kind){
  if(!historyLists.TryGetValue(kind,out var list))return;
  if(gpuChoice.SelectedItem==null){list.Children.Clear();list.Children.Add(Text("Aguardando a identificação da GPU…",12));return;}
  try{var response=await Data(new(){["operation"]="recent",["device"]=DeviceKey,["kind"]=kind});var records=response["records"]!.AsArray();
   if(!historyLists.TryGetValue(kind,out var current)||current!=list)return;list.Children.Clear();int n=0;
   foreach(var row in records){if(row is not JsonObject record||n>=3)continue;n++;RepairText(record);
    string outcome=record["points"] is JsonNode p?Points(p).ToString("N0")+" pontos":record["verdict"]?.ToString() is {Length:>0} v?v:record["valid"]?.GetValue<bool>()==true?"concluído":"incompleto";
    list.Children.Add(Button($"Teste {n}"+(n==1?" · mais recente":"")+$" · {LocalTime(record["timestamp"])} · {outcome}",()=>ShowReport(record)));}
   if(n==0)list.Children.Add(Text("Nenhum teste registrado para esta GPU nesta categoria.",12));
   else if(!shownReports.ContainsKey(kind)&&records[0] is JsonObject latest)ShowReport(latest,false);}
  catch(Exception e){list.Children.Clear();list.Children.Add(Text("Histórico indisponível: "+e.Message,12));}
 }
 // Resultados salvos antes da 0.8.1 têm acentos embaralhados (lidos na página de código do console);
 // desfaz a conversão dupla quando o texto tem esse padrão.
 static readonly System.Text.RegularExpressions.Regex mojibake=new("[Â-ô][-¿ŒœŠšŸŽžƒˆ˜–—‘-„†-•…‰‹›€™]+");
 static string Repair(string text){
  if(!mojibake.IsMatch(text))return text;
  System.Text.Encoding.RegisterProvider(System.Text.CodePagesEncodingProvider.Instance);
  var latin=System.Text.Encoding.GetEncoding(1252);
  // Converte só os trechos embaralhados (UTF-8 lido como Windows-1252), até três camadas.
  for(int round=0;round<3&&mojibake.IsMatch(text);round++)
   text=mojibake.Replace(text,m=>{var fixedText=System.Text.Encoding.UTF8.GetString(latin.GetBytes(m.Value));return fixedText.Contains('�')?m.Value:fixedText;});
  return text;
 }
 static void RepairText(JsonNode? node){
  switch(node){
   case JsonObject o:foreach(var key in o.Select(p=>p.Key).ToList()){if(o[key] is JsonValue v&&v.TryGetValue<string>(out var t)){var fixedText=Repair(t);if(fixedText!=t)o[key]=fixedText;}else RepairText(o[key]);}break;
   case JsonArray a:for(int i=0;i<a.Count;i++){if(a[i] is JsonValue v&&v.TryGetValue<string>(out var t)){var fixedText=Repair(t);if(fixedText!=t)a[i]=fixedText;}else RepairText(a[i]);}break;
  }
 }
 async Task ExportReport(){try{if(selectedReport==null)throw new InvalidOperationException("Selecione um resultado.");var picker=new global::Windows.Storage.Pickers.FileSavePicker{SuggestedFileName="GPU-PRO-resultado"};WinRT.Interop.InitializeWithWindow.Initialize(picker,hwnd);picker.FileTypeChoices.Add("JSON",new List<string>{".json"});var file=await picker.PickSaveFileAsync();if(file!=null)File.WriteAllText(file.Path,selectedReport.ToJsonString(new JsonSerializerOptions{WriteIndented=true}));}catch(Exception e){TestError(e);}}
}
