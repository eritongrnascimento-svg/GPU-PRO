using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Composition.SystemBackdrops;
using System.Runtime.InteropServices;
using System.Text.Json;
using WinRT;
namespace GpuPro.Windows;
public sealed partial class MainWindow:Window {
 readonly DispatcherTimer timer=new(){Interval=TimeSpan.FromSeconds(1)};
 readonly StackPanel body=new(){Spacing=18,Margin=new Thickness(28)};
 readonly NavigationView nav=new(){PaneDisplayMode=NavigationViewPaneDisplayMode.Left,IsBackButtonVisible=NavigationViewBackButtonVisible.Collapsed,IsSettingsVisible=false,OpenPaneLength=240};
 readonly TextBlock status=new(){Text="GPU-PRO · build local multiplataforma",Margin=new Thickness(20,8,20,8),FontSize=12};
 readonly Dictionary<string,TextBlock> values=new();readonly Dictionary<string,NeonCard> metricCards=new();readonly List<NeonCard> coreCards=new();readonly List<TextBlock> coreValues=new();
 readonly List<double> gpuHistory=new(),cpuHistory=new();HistoryPlot? gpuPlot,cpuPlot,framePlot;
 readonly TextBlock gpuIdentity=new(){FontSize=14,VerticalAlignment=VerticalAlignment.Center,Text="Aguardando inventário de dispositivos"};
 Grid? coresGrid;JsonElement snapshot;bool polling,benchmark;int current;Window? hud;TextBlock? hudText;
 TextBox csv=new(){PlaceholderText="CSV ativo com frametime (ms) ou fps",MinWidth=320};
 Native.Frames frames;readonly FrameFeed frameFeed=new();int fpsPid;string fpsLabel="";DesktopAcrylicController? acrylic;SystemBackdropConfiguration? backdrop;readonly nint hwnd;
 ComboBox benchMode=new();NumberBox seconds=new(){Value=3,Minimum=1,Maximum=30,SmallChange=1,Width=120};CheckBox multi=new(){Content="Todos os processadores lógicos",IsChecked=true};
 TextBlock benchResult=new(){Text="Selecione um teste. Nenhuma carga é iniciada automaticamente.",TextWrapping=TextWrapping.Wrap};
 readonly NumberBox cpuMs=new(){Header="Tempo CPU por quadro (ms)",Value=16.7,Minimum=.01,Maximum=10000,Width=230};
 readonly NumberBox gpuMs=new(){Header="Tempo GPU por quadro (ms)",Value=8.3,Minimum=.01,Maximum=10000,Width=230};
 readonly TextBlock balanceText=new(){TextWrapping=TextWrapping.Wrap,FontSize=18};
 [DllImport("dwmapi.dll")]static extern int DwmExtendFrameIntoClientArea(nint h,ref Margins m);
 [DllImport("dwmapi.dll")]static extern int DwmSetWindowAttribute(nint h,int attr,ref int value,int size);
 [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")]static extern nint GetWindowLongPtr(nint h,int index);
 [DllImport("user32.dll",EntryPoint="SetWindowLongPtrW")]static extern nint SetWindowLongPtr(nint h,int index,nint value);
 [DllImport("user32.dll")]static extern bool SetWindowPos(nint h,nint after,int x,int y,int w,int height,uint flags);
 [StructLayout(LayoutKind.Sequential)]struct Margins{public int Left,Right,Top,Bottom;}
 static SolidColorBrush Brush(byte a,byte r,byte g,byte b)=>new(global::Windows.UI.Color.FromArgb(a,r,g,b));
 static TextBlock Text(string text,int size=14)=>new(){Text=text,FontSize=size,TextWrapping=TextWrapping.Wrap};
 static Button Button(string label,Action action){var b=new Button{Content=label,Padding=new Thickness(18,10,18,10),Background=Accent.Fill,Foreground=Brush(255,9,9,9),CornerRadius=new CornerRadius(4),FontWeight=global::Microsoft.UI.Text.FontWeights.SemiBold};b.Click+=(s,e)=>action();return b;}
 public MainWindow(){
  InitializeComponent();hwnd=WinRT.Interop.WindowNative.GetWindowHandle(this);Title="GPU-PRO";
  AppWindow.Resize(new global::Windows.Graphics.SizeInt32(1320,960));CenterOnScreen();AppWindow.SetIcon(Path.Combine(AppContext.BaseDirectory,"Assets","gpu-pro.ico"));
  Root.RequestedTheme=ElementTheme.Dark;Root.RowDefinitions.Add(new(){Height=new GridLength(48)});Root.RowDefinitions.Add(new(){Height=new GridLength(1,GridUnitType.Star)});Root.RowDefinitions.Add(new(){Height=GridLength.Auto});
  var header=new Grid{Background=Brush(40,0,0,0),Padding=new Thickness(18,8,160,8)};var brand=new StackPanel{Orientation=Orientation.Horizontal,Spacing=10,VerticalAlignment=VerticalAlignment.Center};brand.Children.Add(new Image{Source=new Microsoft.UI.Xaml.Media.Imaging.BitmapImage(new Uri(Path.Combine(AppContext.BaseDirectory,"Assets","gpu-pro.png"))),Width=24,Height=24});brand.Children.Add(new TextBlock{Text="GPU-PRO",FontSize=18,VerticalAlignment=VerticalAlignment.Center,Foreground=Accent.Fill,FontWeight=global::Microsoft.UI.Text.FontWeights.SemiBold});header.Children.Add(brand);Root.Children.Add(header);ExtendsContentIntoTitleBar=true;SetTitleBar(header);
  AppWindow.TitleBar.ButtonBackgroundColor=Colors.Transparent;AppWindow.TitleBar.ButtonInactiveBackgroundColor=Colors.Transparent;
  Root.Background=Brush(160,10,10,14);Grid.SetRow(nav,1);Root.Children.Add(nav);Grid.SetRow(status,2);Root.Children.Add(status);
  nav.IsPaneToggleButtonVisible=false;nav.PaneTitle="GPU-PRO";
  foreach(var item in new[]{(0,"GPU · Monitor e especificações"),(1,"CPU · Monitor e cálculos"),(6,"Configurações 3D & Filtros"),(2,"GamerMonitor & Overlay"),(7,"Monitores & Telas"),(8,"Overclock · Manual"),(9,"Benchmark"),(5,"Feedback & Diagnóstico"),(10,"Notificações & Logs"),(4,"Atualizações"),(12,"Sobre")})nav.MenuItems.Add(new NavigationViewItem{Content=item.Item2,Tag=item.Item1,FontSize=12});
  var workspace=new Grid();workspace.RowDefinitions.Add(new(){Height=GridLength.Auto});workspace.RowDefinitions.Add(new(){Height=new GridLength(1,GridUnitType.Star)});var deviceBar=new Grid{Margin=new Thickness(24,14,24,0),ColumnSpacing=16};deviceBar.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});deviceBar.ColumnDefinitions.Add(new(){Width=GridLength.Auto});deviceBar.Children.Add(gpuChoice);gpuChoice.SelectionChanged+=async(s,e)=>{if(gpuChoice.SelectedItem is GpuChoice choice){if(Native.gpp_select_gpu(choice.Id)!=0){Record("GPU não está mais disponível: "+choice.Name,true);return;}Record("GPU selecionada: "+choice.Name);gpuHistory.Clear();selectedReport=null;specs=default;videoCodecs=null;await Poll();if(specGrid!=null)await LoadSpecs();}};var notificationsButton=Button("Notificações",()=>SelectPage(10));Grid.SetColumn(notificationsButton,1);deviceBar.Children.Add(notificationsButton);workspace.Children.Add(deviceBar);pageScroll=new ScrollViewer{Content=body,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled};Grid.SetRow(pageScroll,1);workspace.Children.Add(pageScroll);nav.Content=workspace;nav.SelectionChanged+=async(s,e)=>{if(e.SelectedItem is not NavigationViewItem item)return;int tag=(int)item.Tag;if(tag==current&&body.Children.Count>0)return;if(tag==8&&!await ConfirmOverclockTerms()){SelectPage(current);return;}current=tag;BuildPage();};
  InitializeTray();SetAcrylic();Root.SizeChanged+=(s,e)=>ArrangeCores();Closed+=(s,e)=>{timer.Stop();workerCancel?.Cancel();Native.gpp_cancel_benchmark();hud?.Close();acrylic?.Dispose();};
  timer.Tick+=async(s,e)=>await Poll();timer.Start();ShowPaneVerse();var startArgs=Environment.GetCommandLineArgs();int startTag=Array.IndexOf(startArgs,"--pagina") is int at and >0&&at+1<startArgs.Length&&int.TryParse(startArgs[at+1],out var tag)?tag:-1;nav.SelectedItem=nav.MenuItems.OfType<NavigationViewItem>().FirstOrDefault(i=>i.Tag is int t&&t==startTag)??nav.MenuItems[0];_ = RefreshGpuInventory();
 }
 void SetAcrylic(){
  if(DesktopAcrylicController.IsSupported()){backdrop=new(){IsInputActive=true,Theme=SystemBackdropTheme.Dark};acrylic=new(){TintColor=global::Windows.UI.Color.FromArgb(255,8,10,18),TintOpacity=.70f,LuminosityOpacity=.28f,FallbackColor=global::Windows.UI.Color.FromArgb(255,12,14,22)};acrylic.AddSystemBackdropTarget(this.As<Microsoft.UI.Composition.ICompositionSupportsSystemBackdrop>());acrylic.SetSystemBackdropConfiguration(backdrop);Activated+=(s,e)=>{if(backdrop!=null)backdrop.IsInputActive=e.WindowActivationState!=WindowActivationState.Deactivated;};}
  var m=new Margins{Left=-1,Right=-1,Top=-1,Bottom=-1};DwmExtendFrameIntoClientArea(hwnd,ref m);int dark=1;DwmSetWindowAttribute(hwnd,20,ref dark,4);
 }
 global::Windows.UI.Color theme=global::Windows.UI.Color.FromArgb(255,118,185,0);
 NeonCard Card(string label,string key){var card=new NeonCard(label);values[key]=card.Value;metricCards[key]=card;return card;}
 ScrollViewer? pageScroll;
 // Cards de métricas que se ajustam à janela (FitPanel); share = fração da altura visível.
 FitPanel Cards(params (string Label,string Key)[] entries)=>Cards(1,entries);
 // Grade fixa original (3 colunas, altura natural) para as páginas cujo design já estava pronto.
 // Cards pequenos em uma faixa (GamerMonitor e Overclock compactos).
 Grid CompactCards(int columns,params (string Label,string Key)[] entries){var grid=new Grid{ColumnSpacing=8,RowSpacing=8};for(int n=0;n<columns;n++)grid.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});for(int i=0;i<entries.Length;i++){if(i%columns==0)grid.RowDefinitions.Add(new(){Height=GridLength.Auto});var card=new NeonCard(entries[i].Label,true){MinHeight=66};values[entries[i].Key]=card.Value;metricCards[entries[i].Key]=card;Grid.SetColumn(card,i%columns);Grid.SetRow(card,i/columns);grid.Children.Add(card);}return grid;}
 Grid CardsGrid(params (string Label,string Key)[] entries){var grid=new Grid{ColumnSpacing=14,RowSpacing=14};for(int n=0;n<3;n++)grid.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});for(int i=0;i<entries.Length;i++){if(i%3==0)grid.RowDefinitions.Add(new(){Height=GridLength.Auto});var c=Card(entries[i].Label,entries[i].Key);Grid.SetColumn(c,i%3);Grid.SetRow(c,i/3);grid.Children.Add(c);}return grid;}
 FitPanel Cards(double share,params (string Label,string Key)[] entries){var panel=Fit(share);foreach(var e in entries)panel.Children.Add(Card(e.Label,e.Key));return panel;}
 FitPanel Fit(double share,double reserve=60)=>new FitPanel{Viewport=pageScroll,Share=share,Reserve=reserve,Gap=12};
 static void Detach(UIElement element){if(element is FrameworkElement view){if(view.Parent is Panel panel)panel.Children.Remove(element);else if(view.Parent is Border border)border.Child=null;else if(view.Parent is ContentControl host)host.Content=null;}}
 // Controles que guardam estado entre páginas são recriados a cada montagem, copiando o estado.
 // Reinserir a mesma instância enquanto o WinUI ainda a considera filha da página anterior
 // fecha o app (COMException 0x800F1000 em Microsoft.UI.Xaml.dll ao trocar de aba).
 void RecreateSharedControls(){
  csv=new TextBox{PlaceholderText=csv.PlaceholderText,Text=csv.Text,MinWidth=csv.MinWidth};
  fpsStatus=new TextBlock{TextWrapping=TextWrapping.Wrap,FontSize=12};
  monitoredProcess=new ComboBox{Header="Jogo / aplicativo",DisplayMemberPath="Label",MinWidth=monitoredProcess.MinWidth,ItemsSource=monitoredProcess.ItemsSource,SelectedItem=monitoredProcess.SelectedItem};
  int mode=Math.Max(0,benchMode.SelectedIndex);benchMode=new ComboBox{Tag=mode};
  seconds=new NumberBox{Value=seconds.Value,Minimum=1,Maximum=30,SmallChange=1,Width=120};
  multi=new CheckBox{Content="Todos os processadores lógicos",IsChecked=multi.IsChecked};
  benchResult=new TextBlock{Text=benchResult.Text,TextWrapping=TextWrapping.Wrap};
  profileName=new TextBox{Header="Nome do perfil",Text=profileName.Text};
  profileExe=new TextBox{Header="Executável do jogo",Text=profileExe.Text};
  profileArgs=new TextBox{Header="Argumentos",Text=profileArgs.Text};
  profileOverlay=new CheckBox{Content="Exibir HUD ao iniciar",IsChecked=profileOverlay.IsChecked};
 }
 void BuildPage(){
  RecreateSharedControls();foreach(var element in new UIElement[]{cpuMs,gpuMs,balanceText,profileState})Detach(element);
  body.Children.Clear();values.Clear();metricCards.Clear();coreCards.Clear();coreValues.Clear();coresGrid=null;specGrid=null;stabilityChart=null;stabilityVerdict=null;gpuPlot=null;cpuPlot=null;framePlot=null;body.Children.Add(Text(nav.MenuItems.Cast<NavigationViewItem>().FirstOrDefault(i=>(int)i.Tag==current)?.Content.ToString()??"GPU-PRO",26));
  if(current==0){
   var tabs=new Pivot();var monitor=new PivotItem{Header="Monitor de recursos"};var content=new StackPanel{Spacing=16};
   content.Children.Add(Cards(("Uso GPU","usage"),("Temperatura","temp"),("Hotspot","hotspot"),("Temperatura VRAM","memoryTemp"),("VRAM usada","vram"),("VRAM total","vramTotal"),("Consumo","power"),("Clock GPU","clock"),("Clock memória","memClock"),("Ventoinha","fan"),("Ventoinha RPM","fanRpm"),("Encoder","encoder"),("Decoder","decoder"),("Uso CPU","cpuUsage"),("Temperatura CPU","cpuTemp"),("Tensão GPU","voltage"),("FPS atual","fps"),("1% low","fpsLow"),("FPS médio","fpsAvg"),("FPS máximo","fpsMax")));
   content.Children.Add(Text("Leituras do driver da GPU selecionada. N/D indica sensor indisponível; uso do sistema não equivale ao uso exclusivo de um jogo.",12));gpuPlot=new HistoryPlot("Uso GPU · %");content.Children.Add(gpuPlot);monitor.Content=content;tabs.Items.Add(monitor);
   tabs.Items.Add(new PivotItem{Header="Especificações",Content=SpecsPanel()});body.Children.Add(tabs);
  }
  if(current==1){
   body.Children.Add(CardsGrid(("Processador","cpuName"),("Uso CPU","cpuUsage"),("Processadores lógicos","logicalProcessors"),("Núcleos físicos","physicalCores"),("Memória usada","ramUsed"),("Memória total","ramTotal")));
   body.Children.Add(Text("Processadores lógicos · a cor acompanha o uso durante jogos e testes",18));coresGrid=new Grid{ColumnSpacing=10,RowSpacing=10};body.Children.Add(coresGrid);
   benchMode.Items.Clear();foreach(var item in new[]{"Mistura de inteiros","Cálculo FP64 (raiz e seno)","Contagem de primos"})benchMode.Items.Add(item);benchMode.SelectedIndex=benchMode.Tag is int keep?keep:0;
   var row=new StackPanel{Orientation=Orientation.Horizontal,Spacing=12};row.Children.Add(benchMode);row.Children.Add(seconds);row.Children.Add(Button("Iniciar teste CPU",async()=>await RunBench()));row.Children.Add(Button("Cancelar",()=>Native.gpp_cancel_benchmark()));body.Children.Add(row);body.Children.Add(multi);body.Children.Add(benchResult);cpuPlot=new HistoryPlot("Uso CPU · %");body.Children.Add(cpuPlot);
   body.Children.Add(Text("Carga sintética local em C++ compartilhado com Linux. Resultado em operações do algoritmo por segundo, sem score universal. Temperatura CPU depende de sensores que o Windows exponha; não é inferida da temperatura da GPU."));
  }
  if(current==2){
   var live=new Grid{ColumnSpacing=8,RowSpacing=8};var entries=new[]{("FPS atual","fps"),("1% low","fpsLow"),("FPS médio","fpsAvg"),("FPS máximo","fpsMax"),("Uso GPU","usage"),("Temperatura GPU","temp"),("Uso CPU","cpuUsage"),("Consumo GPU","power"),("VRAM usada","vram"),("RAM usada","ramUsed")};
   for(int i=0;i<5;i++)live.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});for(int i=0;i<entries.Length;i++){if(i%5==0)live.RowDefinitions.Add(new(){Height=GridLength.Auto});var card=new NeonCard(entries[i].Item1,true){MinHeight=66};values[entries[i].Item2]=card.Value;metricCards[entries[i].Item2]=card;Grid.SetRow(card,i/5);Grid.SetColumn(card,i%5);live.Children.Add(card);}
   body.Children.Add(live);framePlot=new HistoryPlot("Frametime · ms"){Height=110};body.Children.Add(framePlot);
   BuildOverlaySettings();
  }
  if(current==3){body.Children.Add(Text("Modelo de sobreposição ideal CPU/GPU. Informe tempos medidos por quadro para explorar o limite estimado. Os valores iniciais são uma simulação; não são medição do seu hardware."));var row=new StackPanel{Orientation=Orientation.Horizontal,Spacing=16};row.Children.Add(cpuMs);row.Children.Add(gpuMs);row.Children.Add(Button("Calcular estimativa",UpdateBalance));body.Children.Add(row);body.Children.Add(balanceText);UpdateBalance();}
  if(current==4){BuildDependencyControls();BuildTrayControls();body.Children.Add(Button("Baixar versão atual",()=>Open("https://github.com/eritongrnascimento-svg/GPU-PRO/releases/latest")));body.Children.Add(Button("Pacotes anteriores",()=>Open("https://github.com/eritongrnascimento-svg/GPU-PRO/blob/main/docs/VERSOES-ANTERIORES.md")));body.Children.Add(Text("Esta é uma compilação local de desenvolvimento para Windows. Não há publicação automática ou alteração de drivers."));}
  if(current==5){BuildFeedbackActions();body.Children.Add(Section("Dados brutos do sistema",Text("Leitura completa reportada pelo sistema e pelo driver, para anexar a um relatório. Sensores sem suporte ficam ausentes."),Button("Exportar dados brutos · JSON",async()=>{try{if(snapshot.ValueKind!=JsonValueKind.Object)throw new InvalidOperationException("Aguardando a primeira leitura dos sensores.");var data=System.Text.Json.Nodes.JsonNode.Parse(snapshot.GetRawText())!;if(specs.ValueKind==JsonValueKind.Object)data["specs"]=System.Text.Json.Nodes.JsonNode.Parse(specs.GetRawText());await SaveJsonFile(data,"GPU-PRO-diagnostico");Record("Diagnóstico exportado.");}catch(Exception e){TestError(e);}})));}
  BuildOtherPages();Render();
 }
 async Task Poll(){if(polling)return;polling=true;try{snapshot=await Task.Run(Native.Snapshot);memoryWatch?.Add(snapshot);CollectStability();ReadGameFrames();UpdateFpsStatus();var gpuReading=Native.Number(snapshot.GetProperty("gpu"),"usage");if(gpuReading.HasValue){gpuHistory.Add(gpuReading.Value);if(gpuHistory.Count>240)gpuHistory.RemoveAt(0);}var cpuReading=Native.Number(snapshot,"cpuUsage");if(cpuReading.HasValue){cpuHistory.Add(cpuReading.Value);if(cpuHistory.Count>240)cpuHistory.RemoveAt(0);}Render();status.Text=Native.Text(snapshot.GetProperty("gpu"),"name")+" · "+Native.Text(snapshot,"cpuName")+" · "+DateTime.Now.ToLongTimeString();}catch(Exception e){Record("Falha de leitura dos sensores: "+e.Message,true);AppendLogFile("  detalhe: "+e.ToString().Replace(Environment.NewLine," | "));}finally{polling=false;}}
 void Set(string key,string text){if(values.TryGetValue(key,out var t))t.Text=text;if(metricCards.TryGetValue(key,out var c)&&text!="N/D")c.Reading(0,key,theme);}
 void Reading(string key,JsonElement source,string? sourceKey=null){if(metricCards.TryGetValue(key,out var c))c.Reading(Native.Number(source,sourceKey??key),key,theme);}
 string Metric(JsonElement e,string key,string suffix="",double divisor=1){var n=Native.Number(e,key);return n.HasValue?(n.Value/divisor).ToString("0.0")+suffix:"N/D";}
 void Render(){
  if(snapshot.ValueKind!=JsonValueKind.Object)return;var gpu=snapshot.GetProperty("gpu");var name=Native.Text(gpu,"name");theme=Accent.ForGpu((gpuChoice.SelectedItem as GpuChoice)?.Vendor??Native.Text(gpu,"vendor"),name);Accent.Set(theme);gpuIdentity.Text="GPU ativa   ·   "+name;Set("gpuName",Native.Text(gpu,"name"));Set("cpuName",Native.Text(snapshot,"cpuName"));Set("logicalProcessors",Native.Text(snapshot,"logicalProcessors"));Set("physicalCores",Native.Text(snapshot,"physicalCores"));
  foreach(var key in new[]{"usage","fan"})Set(key,Metric(gpu,key," %"));Set("temp",Metric(gpu,"temp"," °C"));Set("power",Metric(gpu,"power"," W"));Set("clock",Metric(gpu,"clock"," MHz"));Set("memClock",Metric(gpu,"memClock"," MHz"));Set("vram",Metric(gpu,"vram"," GiB",1073741824));Set("vramTotal",Metric(gpu,"vramTotal"," GiB",1073741824));Set("cpuUsage",Metric(snapshot,"cpuUsage"," %"));Set("ramUsed",Metric(snapshot,"ramUsed"," GiB",1073741824));Set("ramTotal",Metric(snapshot,"ramTotal"," GiB",1073741824));
  Set("fps",frames.Count>0?frames.Current.ToString("0.0"):"N/D");Set("fpsLow",frames.LowAvailable!=0?frames.Low.ToString("0.0"):"N/D");Set("fpsAvg",frames.Count>0?frames.Mean.ToString("0.0"):"N/D");Set("fpsMax",frames.Count>0?frames.Maximum.ToString("0.0"):"N/D");
  Set("driver",Native.Text(gpu,"driverVersion"));foreach(var pair in new[]{("specVramTotal","vramTotal"," GiB",1073741824d),("specClock","clock"," MHz",1d),("specMemClock","memClock"," MHz",1d),("specPower","power"," W",1d),("specTemp","temp"," °C",1d),("specFan","fan"," %",1d)})Set(pair.Item1,Metric(gpu,pair.Item2,pair.Item3,pair.Item4));
  foreach(var pair in new[]{("hotspot"," °C"),("memoryTemp"," °C"),("fanRpm"," RPM"),("encoder"," %"),("decoder"," %"),("voltage"," mV")})Set(pair.Item1,Metric(gpu,pair.Item1,pair.Item2));
  foreach(var key in new[]{"usage","temp","hotspot","memoryTemp","power","clock","memClock","fan","fanRpm","encoder","decoder","voltage","vram","vramTotal"})Reading(key,gpu);Reading("cpuUsage",snapshot);Reading("cpuTemp",snapshot);
  if(current==1&&coresGrid!=null){var cores=snapshot.GetProperty("cores").EnumerateArray().ToArray();if(coreCards.Count!=cores.Length){coreCards.Clear();coreValues.Clear();coresGrid.Children.Clear();for(int i=0;i<cores.Length;i++){var c=new NeonCard("CPU "+Native.Text(cores[i],"name")+" · lógico",true);coreValues.Add(c.Value);coreCards.Add(c);coresGrid.Children.Add(c);}ArrangeCores();}for(int i=0;i<cores.Length;i++){var n=Native.Number(cores[i],"usage")??0;coreValues[i].Text=n.ToString("0.0")+" % · "+Metric(cores[i],"mhz"," MHz");coreCards[i].Reading(Native.Number(cores[i],"usage"),"cpuUsage",theme);}}
  RenderSpecs();PositionHud();gpuPlot?.Update(gpuHistory,theme);cpuPlot?.Update(cpuHistory,theme);framePlot?.Update(Native.LastTimes,theme);
 }
 void ArrangeCores(){if(coresGrid==null)return;int cols=Math.Max(1,(int)Math.Max(180,Root.ActualWidth-320)/195);coresGrid.ColumnDefinitions.Clear();coresGrid.RowDefinitions.Clear();for(int i=0;i<cols;i++)coresGrid.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});for(int i=0;i<coreCards.Count;i++){if(i%cols==0)coresGrid.RowDefinitions.Add(new(){Height=GridLength.Auto});Grid.SetColumn(coreCards[i],i%cols);Grid.SetRow(coreCards[i],i/cols);}}
 async Task RunBench(){if(benchmark||worker!=null)return;benchmark=true;try{Record("Teste CPU "+benchMode.SelectedItem+" iniciado");int mode=benchMode.SelectedIndex,threads=multi.IsChecked==true?Environment.ProcessorCount:1,ms=(int)Math.Clamp(seconds.Value,1,30)*1000;var r=await Task.Run(()=>Native.gpp_run_cpu(mode,threads,ms));if(r.Cancelled==0)benchResult.Text="Resultado: "+r.PerSecond.ToString("N0")+" operações/s · "+r.Threads+" threads · "+r.Seconds.ToString("0.0")+" s";Record("Teste CPU "+benchMode.SelectedItem+(r.Cancelled!=0?" cancelado":" concluído")+" · "+r.PerSecond.ToString("N0")+" operações/s · "+r.Threads+" threads · "+r.Seconds.ToString("0.00")+" s · checksum "+r.Checksum.ToString("G6"));}catch(Exception e){Record("Teste CPU: "+e.Message,true);}finally{benchmark=false;}}
 void UpdateBalance(){var r=Native.gpp_balance(cpuMs.Value,gpuMs.Value);balanceText.Text=r.Valid==0?"Tempos inválidos":$"Estimativa desta carga: {r.Model:0.0} FPS\nGPU limitada pela CPU: {r.GpuLoss:0.0}% · {r.GpuLost:0.0} FPS abaixo do potencial da GPU ({r.GpuPotential:0.0} FPS).\nCPU limitada pela GPU: {r.CpuLoss:0.0}% · {r.CpuLost:0.0} FPS abaixo do potencial de entrega da CPU ({r.CpuPotential:0.0} FPS).\nNão representa perda universal em jogos ou uma promessa de ganho após trocar uma peça.";}
 // FPS: CSV escolhido manualmente tem prioridade; senão o contador próprio do GPU-PRO, com o jogo
 // escolhido na lista ou detectado automaticamente.
 void ReadGameFrames(){
  Native.FeedTimes=Array.Empty<double>();fpsPid=0;fpsLabel="";
  if(!string.IsNullOrWhiteSpace(csv.Text)){frames=Native.ReadFrames(csv.Text);if(frames.Count>0){fpsLabel=Path.GetFileName(csv.Text);return;}}
  if(!frameFeed.Connected)frameFeed.StartSession(false);
  int pid=monitoredProcess.SelectedItem is ProcessChoice chosen&&chosen.Pid>0?chosen.Pid:0;
  if(pid==0){GetWindowThreadProcessId(GetForegroundWindow(),out var fg);pid=frameFeed.Pick(fg);}
  var times=pid>0?frameFeed.Times(pid):Array.Empty<double>();
  if(times.Length==0){frames=default;return;}
  Native.FeedTimes=times;frames=Native.gpp_frame_stats(times,times.Length);fpsPid=pid;fpsLabel=frameFeed.Describe(pid);
 }
 async Task PickCsv(){var picker=new global::Windows.Storage.Pickers.FileOpenPicker();WinRT.Interop.InitializeWithWindow.Initialize(picker,hwnd);picker.FileTypeFilter.Add(".csv");var file=await picker.PickSingleFileAsync();if(file!=null)csv.Text=file.Path;}
 static void Open(string url)=>System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(url){UseShellExecute=true});
}
