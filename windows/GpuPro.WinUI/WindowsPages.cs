using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System.Diagnostics;
using System.Text.Json;
using System.Runtime.InteropServices;
namespace GpuPro.Windows;
public sealed partial class MainWindow {
 static readonly string dataRoot=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"GPU-PRO");
 // Central única: toda mensagem de operação passa por Record(); as que exigem atenção
 // também viram notificação. O log de cada dia é gravado em %LOCALAPPDATA%\GPU-PRO\logs.
 readonly List<string> notices=new();
 readonly List<string> operations=new();
 static readonly string logFolder=Path.Combine(dataRoot,"logs");
 static string LogFile=>Path.Combine(logFolder,"GPU-PRO-"+DateTime.Now.ToString("yyyy-MM-dd")+".log");
 string? lastRecord;int centralTab;TextBlock? logView;TextBlock? logCount;string logSearch="",logCategory="Todas";
 static readonly string[] logCategories={"Todas","Testes","Gargalo","Overclock","HUD & FPS","Sistema & GPU","Perfis","Dependências","Avisos e erros"};
 // Categoria deduzida do texto, para o filtro do Log de operações.
 static string LogCategory(string text,bool notify){
  if(notify||text.StartsWith("Falha")||text.Contains("ERRO"))return "Avisos e erros";
  if(text.Contains("Gargalo"))return "Gargalo";if(text.StartsWith("Teste"))return "Testes";
  if(text.StartsWith("Ajuste")||text.StartsWith("Overclock"))return "Overclock";
  if(text.StartsWith("HUD")||text.Contains("Captura de FPS"))return "HUD & FPS";
  if(text.StartsWith("Perfil")||text.StartsWith("Jogo iniciado"))return "Perfis";
  if(text.StartsWith("winget")||text.StartsWith("Instalação")||text.Contains("FFmpeg")||text.Contains("dependência"))return "Dependências";
  return "Sistema & GPU";
 }
 IEnumerable<string> FilteredLog()=>operations.Where(l=>(logCategory=="Todas"||l.Contains("["+logCategory+"]"))&&(logSearch.Length==0||l.Contains(logSearch,StringComparison.CurrentCultureIgnoreCase)));
 void RefreshLogView(){if(logView==null)return;var rows=FilteredLog().ToList();logView.Text=rows.Count==0?(operations.Count==0?"Nenhuma operação registrada nesta sessão.":"Nenhum registro corresponde ao filtro."):string.Join('\n',rows);if(logCount!=null)logCount.Text=rows.Count+" de "+operations.Count+" registros";}
 readonly TextBox profileName=new(){Header="Nome do perfil",Text="Meu jogo"};
 readonly TextBox profileExe=new(){Header="Executável do jogo"};
 readonly TextBox profileArgs=new(){Header="Argumentos"};
 readonly CheckBox profileOverlay=new(){Content="Exibir HUD ao iniciar",IsChecked=true};
 readonly TextBlock profileState=new(){TextWrapping=TextWrapping.Wrap};
 internal static void AppendLogFile(string line){try{Directory.CreateDirectory(logFolder);File.AppendAllText(LogFile,line+Environment.NewLine);}catch{}}
 void Record(string text,bool notify=false){
  if(string.IsNullOrWhiteSpace(text))return;
  if(text==lastRecord&&!notify)return;lastRecord=text;
  var line=DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss")+" · ["+LogCategory(text,notify)+"] · "+text;
  operations.Add(line);if(operations.Count>2000)operations.RemoveAt(0);AppendLogFile(line);
  if(notify){notices.Insert(0,line);if(notices.Count>200)notices.RemoveAt(notices.Count-1);}
  if(current==10){if(notify&&centralTab==0)BuildPage();else RefreshLogView();}
 }
 void BuildCentral(){
  var tabs=new Pivot();
  var noticeBody=new StackPanel{Spacing=12};noticeBody.Children.Add(Button("Limpar notificações",()=>{notices.Clear();Record("Notificações limpas.");BuildPage();}));
  foreach(var note in notices)noticeBody.Children.Add(Section("Notificação",Text(note)));
  if(notices.Count==0)noticeBody.Children.Add(Text("Nenhuma notificação. Falhas e avisos que pedem atenção aparecem aqui e também ficam registrados em Logs."));
  tabs.Items.Add(new PivotItem{Header="Notificações ("+notices.Count+")",Content=noticeBody});
  var logBody=new StackPanel{Spacing=12};var actions=new StackPanel{Orientation=Orientation.Horizontal,Spacing=12};
  actions.Children.Add(Button("Exportar logs",async()=>await ExportLogs()));actions.Children.Add(Button("Abrir pasta de logs",()=>{Directory.CreateDirectory(logFolder);Open(logFolder);}));logBody.Children.Add(actions);
  logBody.Children.Add(Text("Todas as operações desta sessão: leituras, testes, ajustes, perfis, dependências, importações e erros. Cópia diária em "+logFolder+".",12));
  var search=new TextBox{Header="Buscar no log",PlaceholderText="ex.: gargalo, 1920×1080, overclock, RTX, falha",Text=logSearch,MinWidth=360};search.TextChanged+=(s,e)=>{logSearch=search.Text.Trim();RefreshLogView();};
  var category=new ComboBox{Header="Categoria",ItemsSource=logCategories,SelectedItem=logCategory,MinWidth=200};category.SelectionChanged+=(s,e)=>{logCategory=category.SelectedItem as string??"Todas";RefreshLogView();};
  logCount=new TextBlock{VerticalAlignment=VerticalAlignment.Bottom,Margin=new Thickness(0,0,0,6),FontSize=12};
  var filters=new StackPanel{Orientation=Orientation.Horizontal,Spacing=12};filters.Children.Add(search);filters.Children.Add(category);filters.Children.Add(logCount);logBody.Children.Add(filters);
  logView=new TextBlock{IsTextSelectionEnabled=true,TextWrapping=TextWrapping.Wrap,FontSize=12,FontFamily=new FontFamily("Consolas"),Margin=new Thickness(14)};logBody.Children.Add(new Border{Child=new ScrollViewer{Content=logView,MaxHeight=560,MinHeight=320},Background=Brush(190,12,12,16),BorderBrush=Brush(255,41,41,45),BorderThickness=new Thickness(1),CornerRadius=new CornerRadius(8)});
  RefreshLogView();tabs.Items.Add(new PivotItem{Header="Logs de operações ("+operations.Count+")",Content=logBody});
  tabs.SelectedIndex=centralTab;tabs.SelectionChanged+=(s,e)=>centralTab=tabs.SelectedIndex;body.Children.Add(tabs);
 }
 Border Section(string title,params UIElement[] children){var stack=new StackPanel{Spacing=12,Margin=new Thickness(16)};stack.Children.Add(new TextBlock{Text=title,FontSize=14,Foreground=new SolidColorBrush(theme),FontWeight=global::Microsoft.UI.Text.FontWeights.SemiBold});foreach(var child in children)stack.Children.Add(child);return new Border{Child=stack,Background=Brush(190,20,20,24),BorderBrush=Brush(255,41,41,45),BorderThickness=new Thickness(1),CornerRadius=new CornerRadius(8)};}
 void BuildOtherPages(){
  if(current==6)BuildThreeDPage();
  if(current==7)BuildDisplaysPage();
  if(current==8){body.Children.Add(CardsGrid(("Clock GPU atual","clock"),("Clock memória atual","memClock"),("Consumo atual","power"),("Temperatura GPU","temp"),("Ventoinha atual","fan"),("Tensão GPU","voltage")));var tuning=new StackPanel{Spacing=14};body.Children.Add(tuning);_ = BuildTuning(tuning);}
  if(current==9)BuildBenchmarkPage();
  if(current==10)BuildCentral();
 }
 void SelectPage(int tag){nav.SelectedItem=nav.MenuItems.Cast<NavigationViewItem>().First(i=>(int)i.Tag==tag);}
 async Task SelectExecutable(){var picker=new global::Windows.Storage.Pickers.FileOpenPicker();WinRT.Interop.InitializeWithWindow.Initialize(picker,hwnd);picker.FileTypeFilter.Add(".exe");var f=await picker.PickSingleFileAsync();if(f!=null)profileExe.Text=f.Path;}
 void SaveProfile(){try{Directory.CreateDirectory(Path.Combine(dataRoot,"profiles"));var path=Path.Combine(dataRoot,"profiles",Guid.NewGuid()+".json");File.WriteAllText(path,JsonSerializer.Serialize(new GameProfile(profileName.Text,profileExe.Text,profileArgs.Text,profileOverlay.IsChecked==true),new JsonSerializerOptions{WriteIndented=true}));Record("Perfil gravado: "+profileName.Text);}catch(Exception e){Record("Perfil: "+e.Message,true);}}
 async Task LoadProfile(){try{var picker=new global::Windows.Storage.Pickers.FileOpenPicker();WinRT.Interop.InitializeWithWindow.Initialize(picker,hwnd);picker.FileTypeFilter.Add(".json");var f=await picker.PickSingleFileAsync();if(f==null)return;var p=JsonSerializer.Deserialize<GameProfile>(File.ReadAllText(f.Path))??throw new InvalidDataException("Perfil inválido");profileName.Text=p.Name;profileExe.Text=p.Executable;profileArgs.Text=p.Arguments;profileOverlay.IsChecked=p.Overlay;Record("Perfil carregado: "+p.Name);}catch(Exception e){Record("Perfil: "+e.Message,true);}}
 void LaunchProfile(){try{if(!File.Exists(profileExe.Text)||!Path.GetExtension(profileExe.Text).Equals(".exe",StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Selecione um executável Windows existente.");Process.Start(new ProcessStartInfo(profileExe.Text){Arguments=profileArgs.Text,WorkingDirectory=Path.GetDirectoryName(profileExe.Text),UseShellExecute=false});if(profileOverlay.IsChecked==true&&hud==null)ToggleHud();Record("Jogo iniciado: "+profileName.Text);}catch(Exception e){Record("Perfil: "+e.Message,true);}}
 async Task ExportLogs(){try{var picker=new global::Windows.Storage.Pickers.FileSavePicker{SuggestedFileName="GPU-PRO"};WinRT.Interop.InitializeWithWindow.Initialize(picker,hwnd);picker.FileTypeChoices.Add("Log",new List<string>{".log"});var f=await picker.PickSaveFileAsync();if(f!=null){File.WriteAllLines(f.Path,operations);Record("Logs exportados: "+f.Path);}}catch(Exception e){Record("Falha ao exportar logs: "+e.Message,true);}}
 record GameProfile(string Name,string Executable,string Arguments,bool Overlay);
}
internal static class WindowsDisplays {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct Device{public uint cb;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string Name;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string Description;public uint Flags;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string Id;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string Key;}
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct Mode{[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string DeviceName;public ushort Spec,Driver,Size,Extra;public uint Fields;public int X,Y;public uint Orientation,FixedOutput;public short Color,Duplex,YResolution,TT,Collate;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string Form;public ushort LogPixels;public uint Bits,Width,Height,Flags,Frequency,IcmMethod,IcmIntent,Media,Dither,Reserved1,Reserved2,PanningWidth,PanningHeight;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool EnumDisplayDevices(string? device,uint n,ref Device value,uint flags);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool EnumDisplaySettings(string device,int n,ref Mode value);
 internal record Display(string Name,uint Width,uint Height,uint Hz);
 internal static List<Display> Read(){var result=new List<Display>();for(uint n=0;n<64;n++){var d=new Device{cb=(uint)Marshal.SizeOf<Device>()};if(!EnumDisplayDevices(null,n,ref d,0))break;if((d.Flags&1)==0)continue;var m=new Mode{Size=(ushort)Marshal.SizeOf<Mode>()};if(EnumDisplaySettings(d.Name,-1,ref m))result.Add(new(d.Description+" · "+d.Name,m.Width,m.Height,m.Frequency));}return result;}
}
