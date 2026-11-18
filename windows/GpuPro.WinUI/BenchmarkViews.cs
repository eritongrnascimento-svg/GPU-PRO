using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Text.Json.Nodes;
namespace GpuPro.Windows;

// Teste completo (sequência com pontuação geral, estilo AnTuTu) e resultados simplificados por categoria.
public sealed partial class MainWindow {
 string videoParts="video,images";
 // Aceita qualquer número do JSON: valores lidos de um resultado (JsonElement) e valores criados aqui
 // (long, int, double). Antes só double era lido, e a soma do Teste completo dava 0.
 static long Points(JsonNode? n){
  if(n is not JsonValue v)return 0;
  if(v.TryGetValue<long>(out var l))return l;if(v.TryGetValue<int>(out var i))return i;
  if(v.TryGetValue<double>(out var d)&&double.IsFinite(d))return (long)Math.Round(d);
  if(v.TryGetValue<System.Text.Json.JsonElement>(out var e)&&e.ValueKind==System.Text.Json.JsonValueKind.Number&&e.TryGetDouble(out var x))return (long)Math.Round(x);
  return 0;
 }
 static string Fmt(double v,string unit)=>v>=100?$"{v:N0} {unit}":v>=10?$"{v:N1} {unit}":$"{v:N2} {unit}";
 static readonly Dictionary<string,string> codecNames=new(){["h264"]="H.264",["hevc"]="HEVC",["av1"]="AV1",["jpeg"]="JPEG",["png"]="PNG",["webp"]="WebP"};

 // CPU: as três cargas do núcleo compartilhado, 5 s cada, em todos os processadores lógicos.
 // Pontuação = média geométrica das operações por segundo ÷ 100 000.
 static JsonObject CpuSuite(CancellationToken cancel){
  var rows=new JsonArray();double log=0;int count=0;var names=new[]{"Mistura de inteiros","Cálculo FP64 (raiz e seno)","Contagem de primos"};
  for(int mode=0;mode<3;mode++){cancel.ThrowIfCancellationRequested();var r=Native.gpp_run_cpu(mode,Environment.ProcessorCount,5000);if(r.Cancelled!=0)throw new OperationCanceledException();rows.Add(new JsonObject{["metric"]=names[mode],["value"]=r.PerSecond,["unit"]="op/s",["threads"]=r.Threads});if(r.PerSecond>0){log+=Math.Log(r.PerSecond);count++;}}
  long points=count>0?(long)Math.Round(Math.Exp(log/count)/10000):0;
  return new JsonObject{["kind"]="cpu",["valid"]=count==3,["points"]=points,["cpuResults"]=rows,["methodText"]="Três cargas do processador (inteiros, ponto flutuante FP64 e primos), 5 s cada, em todas as threads. Pontuação = média das operações por segundo ÷ 10 000."};
 }

 async Task RunSuite(){
  if(worker!=null||benchmark)throw new InvalidOperationException("Aguarde ou cancele o teste em andamento.");
  var gpu=ActiveGpu;gpuChoice.IsEnabled=false;workerCancel=new();var token=workerCancel.Token;
  Record("Teste completo iniciado · "+gpu.Name);
  try{
   Directory.CreateDirectory(Path.Combine(dataRoot,"images"));string id=Guid.NewGuid().ToString(),image=Path.Combine(dataRoot,"images",id+".jpg");
   void Stage(int n,string text){if(workerStatus!=null)workerStatus.Text=$"Teste completo · etapa {n} de 4 · {text}";Record($"Teste completo · etapa {n} de 4 · {text}");}
   async Task<JsonObject> Save(JsonObject r,string kind){r["id"]=Guid.NewGuid().ToString();r["gpu"]=PublicGpu();r["platform"]="Windows";r["timestamp"]=DateTime.UtcNow.ToString("O");r["kind"]=kind;await Data(new(){["operation"]="save",["device"]=DeviceKey,["record"]=r.DeepClone()});ShowReport(r,kind=="suite");_ = RefreshHistory(kind);return r;}
   Stage(1,"Cenas 3D · Vulkan 1920×1080 · 30 s");
   var graphics=await Save(await ExecuteWorker("gpu-bench-worker.exe",new[]{"--pci",gpu.Id,"--api","Vulkan","--width","1920","--height","1080","--seconds","30","--cpu-iterations","0","--live-preview",image,"--preview",Path.Combine(dataRoot,"images",id+".png")},image,token),"graphics");
   Stage(2,"Memória e computação da GPU");
   var compute=await Save(await ExecuteWorker("gpu-bench-worker.exe",new[]{"--pci",gpu.Id,"--api","Vulkan","--width","1920","--height","1080","--seconds","30","--cpu-iterations","0","--compute"},null,token),"compute");
   Stage(3,"Codecs de vídeo e imagem");
   var codecs=await Save(await ExecuteWorker("gpu-video-worker.exe",new[]{"--pci",gpu.Id,"--parts","video,images"},null,token),"video");
   Stage(4,"Processador (CPU)");
   benchmark=true;JsonObject cpu;try{cpu=await Task.Run(()=>CpuSuite(token));}finally{benchmark=false;}
   var categories=new JsonArray{
    new JsonObject{["name"]="GPU · 3D",["points"]=Points(graphics["points"]),["detail"]=graphics["fpsAvg"] is JsonNode f?$"{f.GetValue<double>():0} FPS médio":graphics["error"]?.ToString()},
    new JsonObject{["name"]="Memória da GPU",["points"]=Points(compute["memoryPoints"]),["detail"]="banda, cópia e PCIe"},
    new JsonObject{["name"]="Computação da GPU",["points"]=Points(compute["computePoints"]),["detail"]="FP32, FP64 e inteiros"},
    new JsonObject{["name"]="Codecs de vídeo",["points"]=Points(codecs["videoPoints"]),["detail"]="H.264, HEVC e AV1"},
    new JsonObject{["name"]="Codecs de imagem",["points"]=Points(codecs["imagePoints"]),["detail"]="JPEG, PNG e WebP"},
    new JsonObject{["name"]="Processador",["points"]=Points(cpu["points"]),["detail"]=Environment.ProcessorCount+" threads"}};
   long total=categories.Sum(c=>Points(c!["points"]));
   var suite=new JsonObject{["valid"]=true,["method"]="suite-v1",["api"]="Vulkan",["width"]=1920,["height"]=1080,["points"]=total,["categories"]=categories,
    ["methodText"]="Sequência fixa: cenas 3D (Vulkan, 1920×1080, 30 s), memória e computação da GPU, codecs de vídeo (4 resoluções) e de imagem, e processador. A pontuação total é a soma das categorias; cada categoria mostra o próprio método ao abrir o resultado individual.",
    ["parts"]=new JsonObject{["graphics"]=graphics.DeepClone(),["compute"]=compute.DeepClone(),["video"]=codecs.DeepClone(),["cpu"]=cpu.DeepClone()}};
   suite=await Save(suite,"suite");if(workerProgress!=null)workerProgress.Value=100;
   Record($"Teste completo concluído · {gpu.Name} · {total:N0} pontos · "+string.Join(" · ",categories.Select(c=>c!["name"]+" "+Points(c["points"]).ToString("N0"))));
  }catch(OperationCanceledException){if(workerStatus!=null)workerStatus.Text="Teste completo cancelado. Nenhuma pontuação geral foi registrada.";Record("Teste completo cancelado pelo usuário.");}
  finally{worker=null;workerCancel?.Dispose();workerCancel=null;gpuChoice.IsEnabled=true;benchmark=false;}
 }

 // Cartão com a pontuação e os números principais de cada tipo de resultado.
 UIElement ResultView(JsonObject r){
  var panel=new StackPanel{Spacing=12};string kind=r["kind"]?.ToString()??"";
  UIElement Big(string label,long points,string? sub=null){var card=new NeonCard(label);card.Value.Text=points.ToString("N0");card.Value.FontSize=34;if(sub!=null&&card.Children[0] is Border b&&b.Child is StackPanel inner)inner.Children.Add(new TextBlock{Text=sub,FontSize=12,Opacity=.85,TextWrapping=TextWrapping.Wrap});return card;}
  Grid Cards(params UIElement[] items){var g=new Grid{ColumnSpacing=12,RowSpacing=12};int columns=Math.Min(items.Length,4);for(int i=0;i<columns;i++)g.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});for(int i=0;i<(items.Length+columns-1)/columns;i++)g.RowDefinitions.Add(new(){Height=GridLength.Auto});for(int i=0;i<items.Length;i++){Grid.SetColumn((FrameworkElement)items[i],i%columns);Grid.SetRow((FrameworkElement)items[i],i/columns);g.Children.Add(items[i]);}return g;}
  UIElement Stat(string label,string value){var card=new NeonCard(label);card.Value.Text=value;card.Value.FontSize=24;return card;}
  UIElement Table(IEnumerable<(string A,string B,string C)> rows,string ha,string hb,string hc){var g=new Grid{ColumnSpacing=16,RowSpacing=4};foreach(var w in new[]{2.2,1.2,1.4})g.ColumnDefinitions.Add(new(){Width=new GridLength(w,GridUnitType.Star)});int row=0;void Add(string a,string b,string c,bool head){g.RowDefinitions.Add(new(){Height=GridLength.Auto});var cells=new[]{a,b,c};for(int i=0;i<3;i++){var t=new TextBlock{Text=cells[i],FontSize=13,Opacity=head?.7:1,FontWeight=head?Microsoft.UI.Text.FontWeights.SemiBold:Microsoft.UI.Text.FontWeights.Normal,TextTrimming=TextTrimming.CharacterEllipsis};Grid.SetRow(t,row);Grid.SetColumn(t,i);g.Children.Add(t);}row++;}Add(ha,hb,hc,true);foreach(var x in rows)Add(x.A,x.B,x.C,false);return g;}
  switch(kind){
   case "suite":
    panel.Children.Add(Big("Pontuação total",Points(r["points"]),"Soma das categorias"));
    panel.Children.Add(Cards((r["categories"] as JsonArray??new()).Select(c=>Big(c!["name"]!.ToString(),Points(c["points"]),c["detail"]?.ToString())).ToArray()));break;
   case "graphics" when r["points"]!=null:
    panel.Children.Add(Cards(Big("Pontuação 3D",Points(r["points"])),Stat("FPS mínimo",$"{r["fpsMin"]?.GetValue<double>():0}"),Stat("FPS médio",$"{r["fpsAvg"]?.GetValue<double>():0}"),Stat("FPS máximo",$"{r["fpsMax"]?.GetValue<double>():0}")));
    panel.Children.Add(Text($"1% low {r["onePercentLow"]?.GetValue<double>():0} FPS · {r["api"]} · {r["width"]}×{r["height"]}",13));break;
   case "compute" when r["memoryPoints"]!=null:{
    panel.Children.Add(Cards(Big("Pontuação de memória",Points(r["memoryPoints"]),"banda, cópia e transferências"),Big("Pontuação de computação",Points(r["computePoints"]),"FLOPS e inteiros")));
    var rows=(r["computeResults"] as JsonArray??new()).Select(x=>x!.AsObject()).Select(x=>(x["metric"]!.ToString(),x["status"]?.ToString()=="passed"?Fmt(x["value"]!.GetValue<double>(),x["unit"]!.ToString()):"não suportado",x["unit"]?.ToString()=="GB/s"?"memória":"computação"));
    panel.Children.Add(Table(rows,"Medida","Resultado","Grupo"));break;}
   case "video" when r["points"]!=null:{
    panel.Children.Add(Cards(Big("Pontuação de vídeo",Points(r["videoPoints"]),"codificação e decodificação por hardware"),Big("Pontuação de imagem",Points(r["imagePoints"]),"JPEG, PNG e WebP pelo processador")));
    string Name(JsonObject x)=>codecNames.GetValueOrDefault(x["codec"]?.ToString()??x["format"]?.ToString()??"",x["codec"]?.ToString()??x["format"]?.ToString()??"")+(x["mode"]?.ToString()=="decode"?" · decodificar":" · codificar")+" · "+x["width"]+"×"+x["height"];
    string Value(JsonObject x,string unit)=>x["status"]?.ToString() switch{"passed"=>$"{x["fps"]!.GetValue<double>():N0} {unit}",_=>x["status"]?.ToString()=="unsupported"?"não suportado":"falhou"};
    var videoRows=(r["codecResults"] as JsonArray??new()).Select(x=>x!.AsObject()).Select(x=>(Name(x),Value(x,"FPS"),x["mpixPerSecond"] is JsonNode m?Fmt(m.GetValue<double>(),"MPix/s"):""));
    var imageRows=(r["imageResults"] as JsonArray??new()).Select(x=>x!.AsObject()).Where(x=>x["mode"]!=null).Select(x=>(Name(x),Value(x,"img/s"),x["mpixPerSecond"] is JsonNode m?Fmt(m.GetValue<double>(),"MPix/s"):""));
    if(videoRows.Any())panel.Children.Add(Table(videoRows,"Vídeo","Velocidade","Megapixels/s"));
    if(imageRows.Any())panel.Children.Add(Table(imageRows,"Imagem","Velocidade","Megapixels/s"));break;}
   default:return Text(r["summary"]?.ToString()??("Método: "+r["method"]+" · Score: "+r["score"]+" "+r["scoreUnit"]),18);
  }
  if(r["methodText"] is JsonNode method)panel.Children.Add(Text("Método: "+method,12));
  if(r["error"] is JsonNode error)panel.Children.Add(Text(error.ToString(),12));
  return panel;
 }
}
