using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text.Json;
using Microsoft.UI.Composition.SystemBackdrops;
using WinRT;
namespace GpuPro.Windows;
// GamerMonitor & Overlay: o usuário escolhe, por grupo (FPS, GPU, CPU, Memória, Disco), cada
// leitura exibida no HUD, e se o HUD fica sobre o jogo ou na área de trabalho. As escolhas
// ficam em %LOCALAPPDATA%\GPU-PRO\hud.json.
public sealed partial class MainWindow {
 internal sealed class HudConfig {
  public string Mode{get;set;}="game";public int Anchor{get;set;}public double Width{get;set;}=420;public double Font{get;set;}=14;
  public bool OnlyGame{get;set;}public bool Graph{get;set;}=true;public string Disk{get;set;}="all";
  // Aparência no padrão do overlay Linux: formato, fundo translúcido com desfoque, cores e deslocamento.
  public string Layout{get;set;}="list";public int Opacity{get;set;}=55;public double OffsetX{get;set;}=24;public double OffsetY{get;set;}=24;public int Monitor{get;set;}
  public string TextColor{get;set;}="#EBEBEB";public string TitleColor{get;set;}="auto";public string GraphColor{get;set;}="auto";public double GraphHeight{get;set;}=90;
  public HashSet<string> Fields{get;set;}=new(){"fps","fpsLow","frametime","usage","temp","clock","power","vram","cpuUsage","ramUsed","ramPct","diskRead","diskWrite","diskTemp"};
  static string File=>Path.Combine(dataRoot,"hud.json");
  internal static HudConfig Load(){try{if(System.IO.File.Exists(File))return JsonSerializer.Deserialize<HudConfig>(System.IO.File.ReadAllText(File))??new();}catch{}return new();}
  internal void Save(){try{Directory.CreateDirectory(dataRoot);System.IO.File.WriteAllText(File,JsonSerializer.Serialize(this,new JsonSerializerOptions{WriteIndented=true}));}catch{}}
 }
 readonly HudConfig hudConfig=HudConfig.Load();
 record HudField(string Group,string Key,string Label);
 static readonly HudField[] hudCatalog={
  new("FPS","fps","FPS atual"),new("FPS","fpsLow","1% low"),new("FPS","fpsAvg","FPS médio"),new("FPS","fpsMax","FPS máximo"),new("FPS","frametime","Frametime (ms)"),
  new("GPU","gpuName","Nome da GPU"),new("GPU","usage","Uso"),new("GPU","temp","Temperatura"),new("GPU","hotspot","Hotspot"),new("GPU","memoryTemp","Temperatura VRAM"),new("GPU","clock","Clock GPU"),new("GPU","memClock","Clock memória"),new("GPU","power","Consumo"),new("GPU","fan","Ventoinha"),new("GPU","voltage","Tensão"),new("GPU","encoder","Encoder"),new("GPU","decoder","Decoder"),new("GPU","vram","VRAM usada"),new("GPU","vramPct","VRAM %"),
  new("CPU","cpuName","Nome da CPU"),new("CPU","cpuUsage","Uso total"),new("CPU","cpuTemp","Temperatura"),new("CPU","cpuClock","Clock médio"),new("CPU","cpuBusiest","Núcleo mais carregado"),new("CPU","cpuCores","Uso por núcleo"),
  new("Memória","ramUsed","RAM usada"),new("Memória","ramPct","RAM %"),new("Memória","ramTotal","RAM total"),new("Memória","commit","Memória confirmada"),new("Memória","gpuShared","Compartilhada com a GPU"),
  new("Disco","diskRead","Leitura"),new("Disco","diskWrite","Escrita"),new("Disco","diskActive","Tempo ativo"),new("Disco","diskTemp","Temperatura")};
 ComboBox monitoredProcess=new(){Header="Jogo / aplicativo",DisplayMemberPath="Label",MinWidth=280};
 HistoryPlot? hudPlot;Process? capture;
 [DllImport("user32.dll")]static extern nint GetForegroundWindow();
 [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(nint window,out uint pid);
 record ProcessChoice(int Pid,string Label);
 record DiskChoice(string Id,string Label);
 void BuildOverlaySettings(){
  var c=hudConfig;
  // Página compacta: os mesmos controles em linhas densas e os quatro grupos de leitura lado a lado.
  StackPanel Row(params UIElement[] items){var row=new StackPanel{Orientation=Orientation.Horizontal,Spacing=10};foreach(var i in items){if(i is FrameworkElement f)f.VerticalAlignment=VerticalAlignment.Bottom;row.Children.Add(i);}return row;}
  var toggle=Button(hud==null?"Exibir HUD":"Ocultar HUD",()=>{ToggleHud();BuildPage();});
  var mode=new ComboBox{Header="Onde o HUD aparece",ItemsSource=new[]{"No jogo · sempre no topo (clique atravessa)","Área de trabalho · atrás das janelas"},SelectedIndex=c.Mode=="desktop"?1:0,MinWidth=300};
  mode.SelectionChanged+=(s,e)=>{c.Mode=mode.SelectedIndex==1?"desktop":"game";c.Save();Record("HUD: modo "+(c.Mode=="desktop"?"área de trabalho":"jogo"));PositionHud();};
  var onlyGame=new CheckBox{Content="Só com o jogo em primeiro plano (modo jogo)",IsChecked=c.OnlyGame};onlyGame.Click+=(s,e)=>{c.OnlyGame=onlyGame.IsChecked==true;c.Save();};
  var layout=new ComboBox{Header="Formato",ItemsSource=new[]{"Lista vertical","Duas colunas","Faixa compacta"},SelectedIndex=c.Layout=="columns"?1:c.Layout=="compact"?2:0,MinWidth=150};layout.SelectionChanged+=(s,e)=>{c.Layout=layout.SelectedIndex switch{1=>"columns",2=>"compact",_=>"list"};c.Save();RebuildHud();};
  var anchor=new ComboBox{Header="Posição",ItemsSource=new[]{"Superior esquerdo","Superior centralizado","Superior direito","Inferior esquerdo","Inferior direito","Centro"},SelectedIndex=Math.Clamp(c.Anchor,0,5),MinWidth=170};anchor.SelectionChanged+=(s,e)=>{c.Anchor=anchor.SelectedIndex;c.Save();PositionHud();};
  var displays=Microsoft.UI.Windowing.DisplayArea.FindAll();var monitorNames=new List<string>();for(int i=0;i<displays.Count;i++){var b=displays[i].OuterBounds;monitorNames.Add($"Monitor {i+1} · {b.Width}×{b.Height}"+(displays[i].IsPrimary?" · principal":""));}
  var monitor=new ComboBox{Header="Monitor",ItemsSource=monitorNames,SelectedIndex=Math.Clamp(c.Monitor,0,Math.Max(0,monitorNames.Count-1)),MinWidth=200};monitor.SelectionChanged+=(s,e)=>{c.Monitor=Math.Max(0,monitor.SelectedIndex);c.Save();PositionHud();};
  NumberBox Number(string header,double value,double min,double max,Action<double> set){var n=new NumberBox{Header=header,Value=value,Minimum=min,Maximum=max,Width=112,SpinButtonPlacementMode=NumberBoxSpinButtonPlacementMode.Compact};n.ValueChanged+=(s,e)=>{if(double.IsFinite(n.Value)){set(n.Value);c.Save();PositionHud();}};return n;}
  var hudSection=Section("HUD",
   Row(toggle,mode,onlyGame),
   Row(layout,anchor,monitor,ColorChoice("Fonte",()=>c.TextColor,v=>c.TextColor=v,false),ColorChoice("Título",()=>c.TitleColor,v=>c.TitleColor=v,true),ColorChoice("Gráfico",()=>c.GraphColor,v=>c.GraphColor=v,true)),
   Row(Number("Deslocamento X",c.OffsetX,0,4000,v=>c.OffsetX=v),Number("Deslocamento Y",c.OffsetY,0,4000,v=>c.OffsetY=v),Number("Largura (px)",c.Width,220,1600,v=>c.Width=v),Number("Fonte (px)",c.Font,9,40,v=>c.Font=v),Number("Fundo (%)",c.Opacity,0,100,v=>{c.Opacity=(int)v;ApplyHudBackdrop();}),Number("Altura gráfico",c.GraphHeight,50,300,v=>c.GraphHeight=v)),
   Text("Mudanças em tempo real. Fundo translúcido com desfoque; o HUD não captura cliques. Deslocamento X/Y = margem a partir do canto. Tela cheia exclusiva pode cobrir o HUD; use tela cheia sem bordas.",11));
  body.Children.Add(Tighten(hudSection));
  var graph=new CheckBox{Content="Gráfico de frametime",IsChecked=c.Graph};graph.Click+=(s,e)=>{c.Graph=graph.IsChecked==true;c.Save();PositionHud();};
  csv.MinWidth=260;monitoredProcess.MinWidth=240;
  body.Children.Add(Tighten(Section("FPS do jogo",
   Row(monitoredProcess,Button("Atualizar processos",RefreshProcesses),Button("Capturar FPS · PresentMon",async()=>await CaptureFrames()),Button("Parar captura",StopCapture)),
   Row(csv,Button("Selecionar CSV",async()=>await PickCsv())),
   Choices("FPS",graph),
   Text("Sem fonte de quadros (PresentMon ou CSV com frametime/fps), FPS fica N/D. 1% low após 100 amostras; janela de até 240.",11))));
  var disks=new List<DiskChoice>{new("all","Todos os discos")};if(snapshot.ValueKind==JsonValueKind.Object&&snapshot.TryGetProperty("disks",out var list))foreach(var d in list.EnumerateArray())disks.Add(new(Native.Text(d,"index"),"Disco "+Native.Text(d,"index")+" · "+Native.Text(d,"volumes")+" · "+Native.Text(d,"model")));
  var disk=new ComboBox{Header="Disco exibido",ItemsSource=disks,DisplayMemberPath="Label",MinWidth=280,SelectedIndex=Math.Max(0,disks.FindIndex(d=>d.Id==c.Disk))};disk.SelectionChanged+=(s,e)=>{if(disk.SelectedItem is DiskChoice d){c.Disk=d.Id;c.Save();}};
  var groups=new Grid{ColumnSpacing=12,RowSpacing=12};groups.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});groups.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});groups.RowDefinitions.Add(new(){Height=GridLength.Auto});groups.RowDefinitions.Add(new(){Height=GridLength.Auto});
  var cells=new UIElement[]{Tighten(Section("GPU",Choices("GPU"))),Tighten(Section("CPU",Choices("CPU"),Text("Temperatura de CPU só quando o Windows expõe o sensor.",11))),Tighten(Section("Memória",Choices("Memória"))),Tighten(Section("Disco",disk,Choices("Disco"),Text("Leitura/escrita em tempo real. Temperatura quando a unidade expõe o sensor (SSD NVMe/SATA).",11)))};
  for(int i=0;i<cells.Length;i++){Grid.SetRow((FrameworkElement)cells[i],i/2);Grid.SetColumn((FrameworkElement)cells[i],i%2);groups.Children.Add(cells[i]);}
  body.Children.Add(groups);
  if(monitoredProcess.ItemsSource==null)RefreshProcesses();
 }
 static Border Tighten(Border section){if(section.Child is StackPanel inner){inner.Margin=new Thickness(12,10,12,10);inner.Spacing=8;}return section;}
 UIElement Choices(string group,params UIElement[] extra){
  var grid=new VariableSizedWrapGrid{Orientation=Orientation.Horizontal,ItemWidth=200,ItemHeight=30};
  foreach(var field in hudCatalog.Where(f=>f.Group==group)){var box=new CheckBox{Content=field.Label,IsChecked=hudConfig.Fields.Contains(field.Key)};box.Click+=(s,e)=>{if(box.IsChecked==true)hudConfig.Fields.Add(field.Key);else hudConfig.Fields.Remove(field.Key);hudConfig.Save();PositionHud();};grid.Children.Add(box);}
  foreach(var e in extra)grid.Children.Add(e);
  return grid;
 }
 void RefreshProcesses(){var values=new List<ProcessChoice>();foreach(var process in Process.GetProcesses()){using(process)try{if(process.Id!=Environment.ProcessId&&process.MainWindowHandle!=0)values.Add(new(process.Id,process.ProcessName+" · PID "+process.Id));}catch{}}monitoredProcess.ItemsSource=values.OrderBy(p=>p.Label).ToList();}
 async Task CaptureFrames(){try{if(capture!=null&&!capture.HasExited)throw new InvalidOperationException("Uma captura já está ativa. Pare-a antes de iniciar outra.");var selected=monitoredProcess.SelectedItem as ProcessChoice??throw new InvalidOperationException("Selecione o jogo em execução.");var executable=FindTool("PresentMon.exe")??throw new IOException("PresentMon Console não encontrado. Use Instalar dependências na aba Atualizações.");Directory.CreateDirectory(Path.Combine(dataRoot,"frametimes"));var path=Path.Combine(dataRoot,"frametimes",Guid.NewGuid()+".csv");var start=new ProcessStartInfo(executable){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};foreach(var arg in new[]{"--process_id",selected.Pid.ToString(),"--output_file",path,"--no_console_stats","--v1_metrics","--session_name","GPU-PRO-"+Environment.ProcessId,"--terminate_on_proc_exit","--timed","3600","--terminate_after_timed"})start.ArgumentList.Add(arg);capture=Process.Start(start)??throw new IOException("PresentMon não iniciou.");csv.Text=path;var running=capture;var output=running.StandardOutput.ReadToEndAsync();var error=running.StandardError.ReadToEndAsync();Record("Captura de FPS iniciada para PID "+selected.Pid);await running.WaitForExitAsync();var message=await error;_ = await output;if(running.ExitCode!=0)TestError(new IOException("PresentMon não confirmou a captura: "+message+". Verifique as permissões de coleta de desempenho do Windows."));}catch(Exception e){TestError(e);}}
 void StopCapture(){if(capture==null)return;try{if(!capture.HasExited)capture.Kill(true);}catch{}capture.Dispose();capture=null;Record("Captura de FPS encerrada.");}
 // ---- Conteúdo do HUD ----
 static string N(double? v,string fmt,string unit)=>v.HasValue?v.Value.ToString(fmt)+unit:"N/D";
 const double GiB=1073741824;
 static global::Windows.UI.Color ParseColor(string hex,global::Windows.UI.Color fallback){try{hex=hex.TrimStart('#');if(hex.Length==6)return global::Windows.UI.Color.FromArgb(255,Convert.ToByte(hex[..2],16),Convert.ToByte(hex[2..4],16),Convert.ToByte(hex[4..6],16));}catch{}return fallback;}
 static string Hex(global::Windows.UI.Color c)=>$"#{c.R:X2}{c.G:X2}{c.B:X2}";
 global::Windows.UI.Color HudText=>ParseColor(hudConfig.TextColor,global::Windows.UI.Color.FromArgb(255,235,235,235));
 global::Windows.UI.Color HudTitle=>hudConfig.TitleColor=="auto"?theme:ParseColor(hudConfig.TitleColor,theme);
 global::Windows.UI.Color HudGraph=>hudConfig.GraphColor=="auto"?theme:ParseColor(hudConfig.GraphColor,theme);
 // Seletor de cor com opção "automática" (cor do fabricante da GPU, como no Linux).
 UIElement ColorChoice(string label,Func<string> get,Action<string> set,bool allowAuto){
  var swatch=new Border{Width=18,Height=18,CornerRadius=new CornerRadius(4),BorderBrush=Brush(255,90,90,96),BorderThickness=new Thickness(1)};
  void Paint(){var v=get();swatch.Background=new SolidColorBrush(v=="auto"?theme:ParseColor(v,theme));}Paint();
  var picker=new ColorPicker{IsAlphaEnabled=false,IsMoreButtonVisible=false,IsColorSliderVisible=true,IsHexInputVisible=true,Color=get()=="auto"?theme:ParseColor(get(),theme)};
  picker.ColorChanged+=(s,e)=>{set(Hex(e.NewColor));hudConfig.Save();Paint();RebuildHud();};
  var panel=new StackPanel{Spacing=8};panel.Children.Add(picker);
  if(allowAuto){var auto=new Button{Content="Automática · cor da GPU"};auto.Click+=(s,e)=>{set("auto");hudConfig.Save();Paint();RebuildHud();};panel.Children.Add(auto);}
  var content=new StackPanel{Orientation=Orientation.Horizontal,Spacing=8};content.Children.Add(swatch);content.Children.Add(new TextBlock{Text=label});
  return new DropDownButton{Content=content,Flyout=new Flyout{Content=panel}};
 }
 List<(string Label,string Value)> HudItems(){
  var items=new List<(string,string)>();if(snapshot.ValueKind!=JsonValueKind.Object)return items;var on=hudConfig.Fields;var gpu=snapshot.GetProperty("gpu");
  void Add(string key,string label,string value){if(on.Contains(key))items.Add((label,value));}
  bool any=frames.Count>0;var times=Native.LastTimes;
  Add("fps","FPS",any?frames.Current.ToString("0"):"N/D");Add("fpsLow","1% low",frames.LowAvailable!=0?frames.Low.ToString("0"):"N/D");Add("fpsAvg","FPS médio",any?frames.Mean.ToString("0"):"N/D");Add("fpsMax","FPS máximo",any?frames.Maximum.ToString("0"):"N/D");Add("frametime","Frametime",times.Length>0?times[^1].ToString("0.0")+" ms":"N/D");
  double? G(string k)=>Native.Number(gpu,k);double? vram=G("vram"),vramTotal=G("vramTotal");
  Add("usage","GPU uso",N(G("usage"),"0"," %"));Add("temp","GPU temp",N(G("temp"),"0"," °C"));Add("hotspot","Hotspot",N(G("hotspot"),"0"," °C"));Add("memoryTemp","VRAM temp",N(G("memoryTemp"),"0"," °C"));
  Add("clock","GPU clock",N(G("clock"),"0"," MHz"));Add("memClock","Mem clock",N(G("memClock"),"0"," MHz"));Add("power","GPU consumo",N(G("power"),"0"," W"));Add("fan","Ventoinha",N(G("fan"),"0"," %"));
  Add("voltage","Tensão",N(G("voltage"),"0"," mV"));Add("encoder","Encoder",N(G("encoder"),"0"," %"));Add("decoder","Decoder",N(G("decoder"),"0"," %"));
  Add("vram","VRAM",N(vram/GiB,"0.0"," GiB"));Add("vramPct","VRAM %",N(vram.HasValue&&vramTotal>0?100*vram/vramTotal:null,"0"," %"));
  var cores=snapshot.TryGetProperty("cores",out var list)?list.EnumerateArray().ToArray():Array.Empty<JsonElement>();
  Add("cpuUsage","CPU uso",N(Native.Number(snapshot,"cpuUsage"),"0"," %"));Add("cpuTemp","CPU temp",N(Native.Number(snapshot,"cpuTemp"),"0"," °C"));
  var mhz=cores.Select(x=>Native.Number(x,"mhz")).Where(x=>x.HasValue).Select(x=>x!.Value).ToArray();Add("cpuClock","CPU clock",N(mhz.Length>0?mhz.Average():null,"0"," MHz"));
  Add("cpuBusiest","Núcleo pico",cores.Length>0?N(cores.Max(x=>Native.Number(x,"usage")??0),"0"," %"):"N/D");Add("cpuCores","Núcleos",cores.Length>0?string.Join(" ",cores.Select(x=>N(Native.Number(x,"usage"),"0","")))+" %":"N/D");
  double? ramUsed=Native.Number(snapshot,"ramUsed"),ramTotal=Native.Number(snapshot,"ramTotal");
  Add("ramUsed","RAM",N(ramUsed/GiB,"0.0"," GiB"));Add("ramPct","RAM %",N(ramUsed.HasValue&&ramTotal>0?100*ramUsed/ramTotal:null,"0"," %"));Add("ramTotal","RAM total",N(ramTotal/GiB,"0.0"," GiB"));
  Add("commit","Commit",N(Native.Number(snapshot,"commitUsed")/GiB,"0.0","")+" / "+N(Native.Number(snapshot,"commitTotal")/GiB,"0.0"," GiB"));Add("gpuShared","GPU compart.",N(G("sharedUsed")/GiB,"0.0"," GiB"));
  if(snapshot.TryGetProperty("disks",out var disks)){var all=disks.EnumerateArray().ToArray();var chosen=hudConfig.Disk=="all"?all:all.Where(d=>Native.Text(d,"index")==hudConfig.Disk).ToArray();
   double Sum(string k)=>chosen.Sum(d=>Native.Number(d,k)??0);string Rate(double bytes)=>bytes>=1048576?(bytes/1048576).ToString("0.0")+" MB/s":(bytes/1024).ToString("0")+" KB/s";
   var temps=chosen.Select(x=>Native.Number(x,"temp")).Where(x=>x.HasValue).Select(x=>x!.Value.ToString("0")).ToArray();
   Add("diskRead","Disco leitura",Rate(Sum("readBytes")));Add("diskWrite","Disco escrita",Rate(Sum("writeBytes")));Add("diskActive","Disco ativo",N(chosen.Length>0?chosen.Max(x=>Native.Number(x,"active")??0):null,"0"," %"));Add("diskTemp","Disco temp",temps.Length>0?string.Join(" / ",temps)+" °C":"N/D");}
  return items;
 }
 Grid? hudBody;DesktopAcrylicController? hudAcrylic;SystemBackdropConfiguration? hudBackdrop;
 void ToggleHud(){
  if(hud!=null){hud.Close();return;}
  hud=new Window{Title="GPU-PRO HUD"};hudBody=new Grid();hudText=new TextBlock();hudPlot=new HistoryPlot("Frametime · ms"){Background=null,Margin=new Thickness(6,0,6,6)};
  var root=new StackPanel();root.Children.Add(hudBody);root.Children.Add(hudPlot);hud.Content=new Border{Child=root,BorderThickness=new Thickness(1)};
  if(hud.AppWindow.Presenter is Microsoft.UI.Windowing.OverlappedPresenter p){p.SetBorderAndTitleBar(false,false);p.IsResizable=false;}
  hud.AppWindow.IsShownInSwitchers=false;hud.Activate();var h=WinRT.Interop.WindowNative.GetWindowHandle(hud);SetWindowLongPtr(h,-20,GetWindowLongPtr(h,-20)|(nint)(0x20|0x80|0x08000000));
  ApplyHudBackdrop();hud.Closed+=(s,e)=>{hudAcrylic?.Dispose();hudAcrylic=null;hud=null;hudText=null;hudPlot=null;hudBody=null;Record("HUD ocultado.");};
  Record("HUD exibido · "+(hudConfig.Mode=="desktop"?"área de trabalho":"jogo"));RebuildHud();
 }
 // Fundo translúcido com desfoque (acrylic do Windows); "Fundo (%)" controla a intensidade da cor por trás.
 void ApplyHudBackdrop(){
  if(hud==null)return;double alpha=hudConfig.Opacity/100.0;
  if(DesktopAcrylicController.IsSupported()){if(hudAcrylic==null){hudBackdrop=new(){IsInputActive=true,Theme=SystemBackdropTheme.Dark};hudAcrylic=new();hudAcrylic.AddSystemBackdropTarget(hud.As<Microsoft.UI.Composition.ICompositionSupportsSystemBackdrop>());hudAcrylic.SetSystemBackdropConfiguration(hudBackdrop);}
   hudAcrylic.TintColor=global::Windows.UI.Color.FromArgb(255,9,9,10);hudAcrylic.TintOpacity=(float)(alpha*.9);hudAcrylic.LuminosityOpacity=(float)(.05+alpha*.35);hudAcrylic.FallbackColor=global::Windows.UI.Color.FromArgb((byte)(alpha*255),9,9,10);}
  else if(hud.Content is Border b)b.Background=Brush((byte)(alpha*255),9,9,10);
  int corner=hudConfig.Layout=="compact"?3:2;DwmSetWindowAttribute(WinRT.Interop.WindowNative.GetWindowHandle(hud),33,ref corner,4);
 }
 void RebuildHud(){if(hud!=null){ApplyHudBackdrop();PositionHud();}}
 void PositionHud(){
  if(hud==null||hudBody==null)return;var c=hudConfig;var items=HudItems();var text=new SolidColorBrush(HudText);var dim=new SolidColorBrush(global::Windows.UI.Color.FromArgb(170,HudText.R,HudText.G,HudText.B));
  hudBody.Children.Clear();hudBody.RowDefinitions.Clear();hudBody.ColumnDefinitions.Clear();hudBody.Margin=new Thickness(14,10,14,c.Graph?4:10);
  if(hud.Content is Border frame)frame.BorderBrush=new SolidColorBrush(global::Windows.UI.Color.FromArgb(110,HudTitle.R,HudTitle.G,HudTitle.B));
  var header=new StackPanel{Spacing=0,Margin=new Thickness(0,0,0,6)};
  if(c.Fields.Contains("gpuName"))header.Children.Add(new TextBlock{Text=Native.Text(snapshot.ValueKind==JsonValueKind.Object?snapshot.GetProperty("gpu"):default,"name"),FontSize=c.Font,FontWeight=global::Microsoft.UI.Text.FontWeights.SemiBold,Foreground=new SolidColorBrush(HudTitle),TextTrimming=TextTrimming.CharacterEllipsis});
  if(c.Fields.Contains("cpuName"))header.Children.Add(new TextBlock{Text=Native.Text(snapshot,"cpuName"),FontSize=c.Font*.85,Foreground=dim,TextTrimming=TextTrimming.CharacterEllipsis});
  if(monitoredProcess.SelectedItem is ProcessChoice tracked)header.Children.Add(new TextBlock{Text=tracked.Label,FontSize=c.Font*.8,Foreground=dim,TextTrimming=TextTrimming.CharacterEllipsis});
  hudBody.RowDefinitions.Add(new(){Height=GridLength.Auto});hudBody.Children.Add(header);
  if(items.Count==0)items.Add(("HUD","nenhuma leitura selecionada"));
  if(c.Layout=="compact"){
   var line=new TextBlock{TextWrapping=TextWrapping.Wrap,FontSize=c.Font};foreach(var (label,value) in items){line.Inlines.Add(new Microsoft.UI.Xaml.Documents.Run{Text=label+" ",Foreground=dim});line.Inlines.Add(new Microsoft.UI.Xaml.Documents.Run{Text=value+"    ",Foreground=text,FontWeight=global::Microsoft.UI.Text.FontWeights.SemiBold});}
   hudBody.RowDefinitions.Add(new(){Height=GridLength.Auto});Grid.SetRow(line,1);hudBody.Children.Add(line);
  }else{
   int columns=c.Layout=="columns"?2:1;var table=new Grid{ColumnSpacing=10,RowSpacing=2};for(int i=0;i<columns;i++){table.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});table.ColumnDefinitions.Add(new(){Width=GridLength.Auto});}
   for(int i=0;i<items.Count;i++){int row=i/columns,col=(i%columns)*2;if(i%columns==0)table.RowDefinitions.Add(new(){Height=GridLength.Auto});
    var l=new TextBlock{Text=items[i].Label,FontSize=c.Font,Foreground=dim,TextTrimming=TextTrimming.CharacterEllipsis};var v=new TextBlock{Text=items[i].Value,FontSize=c.Font,Foreground=text,FontWeight=global::Microsoft.UI.Text.FontWeights.SemiBold,HorizontalAlignment=HorizontalAlignment.Right,Margin=new Thickness(0,0,col==0&&columns>1?14:0,0)};
    Grid.SetRow(l,row);Grid.SetColumn(l,col);Grid.SetRow(v,row);Grid.SetColumn(v,col+1);table.Children.Add(l);table.Children.Add(v);}
   hudBody.RowDefinitions.Add(new(){Height=GridLength.Auto});Grid.SetRow(table,1);hudBody.Children.Add(table);
  }
  if(header.Children.Count==0)header.Visibility=Visibility.Collapsed;
  if(hudPlot!=null){hudPlot.Visibility=c.Graph?Visibility.Visible:Visibility.Collapsed;hudPlot.Height=c.GraphHeight;hudPlot.Update(Native.LastTimes,HudGraph);}
  hudBody.Measure(new global::Windows.Foundation.Size(c.Width-28,double.PositiveInfinity));double scale=hud.Content?.XamlRoot?.RasterizationScale??1;
  int w=(int)(c.Width*scale),h=(int)((hudBody.DesiredSize.Height+(c.Graph?c.GraphHeight+18:20))*scale);hud.AppWindow.Resize(new global::Windows.Graphics.SizeInt32(w,h));
  var displays=Microsoft.UI.Windowing.DisplayArea.FindAll();var area=(c.Monitor<displays.Count?displays[c.Monitor]:Microsoft.UI.Windowing.DisplayArea.Primary).WorkArea;
  int ox=(int)(c.OffsetX*scale),oy=(int)(c.OffsetY*scale),x=area.X+ox,y=area.Y+oy;
  switch(c.Anchor){case 1:x=area.X+(area.Width-w)/2;break;case 2:x=area.X+area.Width-w-ox;break;case 3:y=area.Y+area.Height-h-oy;break;case 4:x=area.X+area.Width-w-ox;y=area.Y+area.Height-h-oy;break;case 5:x=area.X+(area.Width-w)/2+ox;y=area.Y+(area.Height-h)/2+oy;break;}
  x=Math.Clamp(x,area.X,area.X+Math.Max(0,area.Width-w));y=Math.Clamp(y,area.Y,area.Y+Math.Max(0,area.Height-h));var handle=WinRT.Interop.WindowNative.GetWindowHandle(hud);
  if(c.Mode=="desktop"){if(hud.AppWindow.Presenter is Microsoft.UI.Windowing.OverlappedPresenter p)p.IsAlwaysOnTop=false;hud.AppWindow.Show(false);SetWindowPos(handle,(nint)1,x,y,w,h,0x10);return;}
  if(hud.AppWindow.Presenter is Microsoft.UI.Windowing.OverlappedPresenter top)top.IsAlwaysOnTop=true;
  bool visible=true;if(c.OnlyGame&&monitoredProcess.SelectedItem is ProcessChoice target){GetWindowThreadProcessId(GetForegroundWindow(),out var pid);visible=pid==target.Pid;}
  if(visible){hud.AppWindow.Show(false);SetWindowPos(handle,(nint)(-1),x,y,w,h,0x10);}else hud.AppWindow.Hide();
 }
}
