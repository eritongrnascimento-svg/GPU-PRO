using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.Win32;
using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Nodes;
namespace GpuPro.Windows;
// Configurações 3D & Filtros: o que a GPU suporta (Direct3D 12, Vulkan e geração da arquitetura),
// controles reais do driver NVIDIA (perfil global, via gpu-3d-worker elevado) e opções gráficas do
// Windows. Tecnologias que só o jogo ativa (FSR, XeSS, DLSS sem override) são indicadas como tal.
public sealed partial class MainWindow {
 JsonObject? driver3d;readonly Dictionary<string,ComboBox> driverChoices=new();NumberBox? frameLimit;
 // Opções por configuração do driver: rótulo e valor ("default" = padrão do driver).
 static readonly (string Key,string Label,(string Text,string Value)[] Options)[] driverSettings={
  ("dlssSrOverride","DLSS · Super Resolution (override)",new[]{("Padrão do driver","default"),("Desativado","0"),("Ativado","1")}),
  ("dlssSrMode","DLSS · modo",new[]{("Padrão do driver","default"),("Definido pelo jogo","3"),("DLAA","4"),("Qualidade","2"),("Equilibrado","1"),("Desempenho","0"),("Ultra desempenho","5")}),
  ("dlssSrPreset","DLSS · modelo (preset)",new[]{("Padrão do driver","default"),("Do jogo","0"),("Mais recente","16777215"),("Preset J","10"),("Preset K","11"),("Preset E","5"),("Preset F","6")}),
  ("dlssRrOverride","DLSS · Ray Reconstruction (override)",new[]{("Padrão do driver","default"),("Desativado","0"),("Ativado","1")}),
  ("dlssFgOverride","DLSS · Frame Generation (override)",new[]{("Padrão do driver","default"),("Desativado","0"),("Ativado","1")}),
  ("dlssMultiFrame","DLSS · Multi Frame Generation",new[]{("Padrão do driver","default"),("Desativado","0"),("2x","1"),("3x","2"),("4x","3")}),
  ("imageScaling","NVIDIA Image Scaling (upscale + nitidez)",new[]{("Padrão do driver","default"),("Desativado","0"),("Ativado","1")}),
  ("fxaa","FXAA (antisserrilhamento rápido)",new[]{("Padrão do driver","default"),("Desativado","0"),("Ativado","1")}),
  ("vsync","Sincronização vertical",new[]{("Padrão do driver","default"),("Do aplicativo","1620202130"),("Desativada","138504007"),("Ativada","1199655232"),("Rápida","411601032")}),
  ("maxPrerendered","Quadros pré-renderizados (latência)",new[]{("Padrão do driver","default"),("Do aplicativo","0"),("1 · menor latência","1"),("2","2"),("3","3")}),
  ("powerMode","Modo de energia",new[]{("Padrão do driver","default"),("Energia ideal","5"),("Adaptativo","0"),("Desempenho máximo","1")}),
  ("anisoLevel","Filtragem anisotrópica",new[]{("Padrão do driver","default"),("Do aplicativo","app"),("2x","2"),("4x","4"),("8x","8"),("16x","16")}),
  ("textureQuality","Qualidade de filtragem de textura",new[]{("Padrão do driver","default"),("Alta qualidade","4294967286"),("Qualidade","0"),("Desempenho","10"),("Alto desempenho","20")}),
  ("vrrApp","G-SYNC / VRR por aplicativo",new[]{("Padrão do driver","default"),("Permitir","0"),("Forçar desligado","1"),("Não permitir","2"),("Frequência fixa","4")})};
 void BuildThreeDPage(){
  var features=new StackPanel{Spacing=8};body.Children.Add(Tighten(Section("Recursos suportados por esta GPU",features)));
  var nvidia=new StackPanel{Spacing=8};body.Children.Add(Tighten(Section("NVIDIA · configurações 3D do driver (perfil global)",nvidia)));
  body.Children.Add(Tighten(Section("Windows · gráficos e jogos",WindowsGraphics())));
  body.Children.Add(Tighten(Section("Perfil de jogo",Row4(profileName,profileExe,Button("Selecionar executável",async()=>await SelectExecutable())),Row4(profileArgs,profileOverlay,GpuPreference()),Row4(Button("Gravar perfil",SaveProfile),Button("Carregar perfil",async()=>await LoadProfile()),Button("Iniciar jogo com perfil",LaunchProfile)))));
  _=FillThreeD(features,nvidia);
 }
 async Task FillThreeD(StackPanel features,StackPanel nvidia){
  JsonElement f=default;try{f=await Task.Run(Native.Features);}catch(Exception e){Record("Recursos 3D: "+e.Message,true);}
  if(specs.ValueKind!=JsonValueKind.Object)try{specs=await Task.Run(Native.Specs);}catch{}
  string vendor=SpecText(specs,"vendor",""),name=SpecText(specs,"name","");int arch=(int)(Native.Number(specs,"architectureId")??0);bool rtx=vendor=="NVIDIA"&&arch>=6;
  int Tier(string k)=>(int)(Native.Number(f,k)??0);int rt=Tier("raytracingTier");
  var rows=new List<(string,string,bool)>{
   ("Ray tracing · DirectX",rt>=10?$"DXR {rt/10}.{rt%10}":"Não suportado",rt>=10),
   ("Ray tracing · Vulkan",SpecFlag(specs,"rayTracingPipeline")?"VK_KHR_ray_tracing_pipeline":"Não exposto",SpecFlag(specs,"rayTracingPipeline")),
   ("DirectX 12 Ultimate",SpecText(f,"featureLevel","")=="12_2"?"Sim · nível 12_2":"Não ("+SpecText(f,"featureLevel","N/D")+")",SpecText(f,"featureLevel","")=="12_2"),
   ("Mesh shaders",Tier("meshShaderTier")>=10?"Suportado":"Não suportado",Tier("meshShaderTier")>=10),
   ("Sombreamento de taxa variável",Tier("vrsTier")>0?"VRS tier "+Tier("vrsTier"):"Não suportado",Tier("vrsTier")>0),
   ("Shader model",SpecText(f,"shaderModel","N/D"),f.ValueKind==JsonValueKind.Object),
   ("DLSS Super Resolution / DLAA",rtx?"Suportado · override no driver abaixo":"Requer NVIDIA RTX",rtx),
   ("DLSS Ray Reconstruction",rtx?"Suportado nos jogos compatíveis":"Requer NVIDIA RTX",rtx),
   ("DLSS Frame Generation",vendor=="NVIDIA"&&arch>=8?"Suportado (RTX 40+)":"Requer RTX série 40 ou mais nova",vendor=="NVIDIA"&&arch>=8),
   ("DLSS Multi Frame Generation",vendor=="NVIDIA"&&arch>=10?"Suportado (RTX 50)":"Requer RTX série 50",vendor=="NVIDIA"&&arch>=10),
   ("NVIDIA Image Scaling · nitidez",vendor=="NVIDIA"?"Suportado · controle no driver abaixo":"Somente NVIDIA",vendor=="NVIDIA"),
   ("AMD FSR 1 / 2 / 3",SpecText(f,"featureLevel","").StartsWith("12")||SpecFlag(specs,"vulkanApi")?"Disponível nos jogos compatíveis":"Requer DirectX 12/Vulkan",true),
   ("AMD FSR 4",vendor=="AMD"&&name.Contains("RX 9")?"Suportado (RDNA 4)":"Requer Radeon RX 9000",vendor=="AMD"&&name.Contains("RX 9")),
   ("AMD RSR / RIS · nitidez",vendor=="AMD"?"Suportado pelo driver AMD":"Somente AMD Radeon",vendor=="AMD"),
   ("Intel XeSS",vendor=="Intel"&&name.Contains("Arc")?"XMX (Arc)":"DP4a nos jogos compatíveis",true)};
  var grid=new Grid{ColumnSpacing=8,RowSpacing=8};for(int i=0;i<3;i++)grid.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});
  for(int i=0;i<rows.Count;i++){if(i%3==0)grid.RowDefinitions.Add(new(){Height=GridLength.Auto});var card=new NeonCard(rows[i].Item1,true){MinHeight=62};card.Value.Text=rows[i].Item2;card.Value.FontSize=14;card.Reading(rows[i].Item3?0:null,"static",theme);Grid.SetRow(card,i/3);Grid.SetColumn(card,i%3);grid.Children.Add(card);}
  features.Children.Add(grid);features.Children.Add(Text("FSR, XeSS e DLSS sem override são ativados dentro de cada jogo; o override do driver substitui o DLSS que o jogo já usa.",11));
  if(vendor!="NVIDIA"){nvidia.Children.Add(Text(vendor.Length>0?$"A GPU selecionada é {vendor}. Os controles do driver {vendor} ainda não estão disponíveis nesta versão; os recursos suportados aparecem acima.":"Selecione uma GPU.",12));return;}
  try{driver3d=await Task.Run(()=>RunDriverWorker("--read"));}catch(Exception e){Record("Driver NVIDIA: "+e.Message,true);}
  if(driver3d?["available"]?.GetValue<bool>()!=true){nvidia.Children.Add(Text("O driver NVIDIA não respondeu ao NVAPI.",12));return;}
  var settings=driver3d["settings"]!.AsObject();driverChoices.Clear();var wrap=new VariableSizedWrapGrid{Orientation=Orientation.Horizontal,ItemWidth=300,ItemHeight=66};
  foreach(var (key,label,options) in driverSettings){
   if(key=="dlssFgOverride"&&arch<8||key=="dlssMultiFrame"&&arch<10||key.StartsWith("dlss")&&!rtx)continue;
   var combo=new ComboBox{Header=label,ItemsSource=options.Select(o=>o.Text).ToList(),Width=280};var currentValue=settings[key=="anisoLevel"?"anisoSelector":key]?["value"];
   string? cur=currentValue==null?"default":key=="anisoLevel"?(currentValue.GetValue<uint>()==0?"app":settings["anisoLevel"]?["value"]?.GetValue<uint>().ToString()):currentValue.GetValue<uint>().ToString();
   combo.SelectedIndex=Math.Max(0,Array.FindIndex(options,o=>o.Value==cur));combo.Tag=combo.SelectedIndex;driverChoices[key]=combo;wrap.Children.Add(combo);}
  frameLimit=new NumberBox{Header="Limite de FPS (0 = desligado)",Value=settings["frameLimit"]?["value"]?.GetValue<uint>()??0,Minimum=0,Maximum=1023,Width=280,SpinButtonPlacementMode=NumberBoxSpinButtonPlacementMode.Compact};frameLimit.Tag=frameLimit.Value;wrap.Children.Add(frameLimit);
  nvidia.Children.Add(wrap);
  nvidia.Children.Add(Row4(Button("Aplicar no driver",async()=>await ApplyDriver3D(false)),Button("Restaurar padrões do driver",async()=>await ApplyDriver3D(true))));
  nvidia.Children.Add(Text("Valores gravados no perfil global do driver NVIDIA (valem para todos os jogos sem perfil próprio). O Windows pede autorização de administrador; o app confirma lendo de volta.",11));
 }
 static JsonObject RunDriverWorker(string args){var start=new ProcessStartInfo(Path.Combine(AppContext.BaseDirectory,"gpu-3d-worker.exe"),args){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true};using var p=Process.Start(start)??throw new IOException("gpu-3d-worker não iniciou");var text=p.StandardOutput.ReadToEnd();p.WaitForExit();return JsonNode.Parse(text)?.AsObject()??throw new IOException("Resposta inválida do gpu-3d-worker");}
 async Task ApplyDriver3D(bool restore){
  var args=new List<string>();
  foreach(var (key,label,options) in driverSettings){if(!driverChoices.TryGetValue(key,out var combo))continue;int idx=combo.SelectedIndex;if(!restore&&idx==(int)combo.Tag)continue;var value=restore?"default":options[Math.Max(0,idx)].Value;
   if(key=="anisoLevel"){if(value=="default")args.AddRange(new[]{"anisoSelector=default","anisoLevel=default"});else if(value=="app")args.Add("anisoSelector=0");else args.AddRange(new[]{"anisoSelector=1","anisoLevel="+value});}
   else args.Add(key+"="+value);}
  if(frameLimit!=null&&(restore||frameLimit.Value!=(double)frameLimit.Tag))args.Add("frameLimit="+(restore?"default":((int)frameLimit.Value).ToString()));
  if(args.Count==0){Record("Driver NVIDIA: nenhuma alteração para aplicar.");return;}
  var confirm=new ContentDialog{XamlRoot=Root.XamlRoot,Title=restore?"Restaurar padrões do driver NVIDIA":"Aplicar no driver NVIDIA",Content=Text((restore?"Todas as configurações listadas voltarão ao padrão do driver.":"Serão gravadas "+args.Count+" configuração(ões) no perfil global.")+" O Windows pedirá autorização de administrador."),PrimaryButtonText="Continuar",CloseButtonText="Cancelar",DefaultButton=ContentDialogButton.Close};
  if(await confirm.ShowAsync()!=ContentDialogResult.Primary)return;
  try{using var p=Process.Start(new ProcessStartInfo(Path.Combine(AppContext.BaseDirectory,"gpu-3d-worker.exe"),"--apply "+string.Join(' ',args)){UseShellExecute=true,Verb="runas",WindowStyle=ProcessWindowStyle.Hidden})??throw new IOException("gpu-3d-worker não iniciou");await p.WaitForExitAsync();
   Record(p.ExitCode==0?"Driver NVIDIA: configurações 3D aplicadas e confirmadas · "+string.Join(", ",args):$"Driver NVIDIA: alteração não confirmada (código {p.ExitCode}) · "+string.Join(", ",args),p.ExitCode!=0);}
  catch(System.ComponentModel.Win32Exception){Record("Driver NVIDIA: autorização de administrador recusada; nada foi alterado.");}
  catch(Exception e){Record("Driver NVIDIA: "+e.Message,true);}
  BuildPage();
 }
 // ---- Windows: DirectX (Auto HDR, VRR, jogos em janela), Modo de Jogo e HAGS ----
 const string DirectXKey=@"Software\Microsoft\DirectX\UserGpuPreferences";
 static Dictionary<string,string> DirectXGlobal(){var map=new Dictionary<string,string>();var raw=Registry.CurrentUser.OpenSubKey(DirectXKey)?.GetValue("DirectXUserGlobalSettings") as string??"";foreach(var part in raw.Split(';',StringSplitOptions.RemoveEmptyEntries)){var kv=part.Split('=',2);if(kv.Length==2)map[kv[0]]=kv[1];}return map;}
 void SetDirectXGlobal(string key,bool on,string label){var map=DirectXGlobal();map[key]=on?"1":"0";using var k=Registry.CurrentUser.CreateSubKey(DirectXKey);k.SetValue("DirectXUserGlobalSettings",string.Concat(map.Select(p=>p.Key+"="+p.Value+";")));Record($"Windows: {label} {(on?"ativado":"desativado")}");}
 UIElement WindowsGraphics(){
  var map=DirectXGlobal();ToggleSwitch Toggle(string header,bool on,Action<bool> set){var t=new ToggleSwitch{Header=header,IsOn=on,OnContent="Ativado",OffContent="Desativado",MinWidth=250};t.Toggled+=(s,e)=>{try{set(t.IsOn);}catch(Exception ex){Record(header+": "+ex.Message,true);}};return t;}
  var autoHdr=Toggle("Auto HDR",map.GetValueOrDefault("AutoHDREnable")=="1",v=>SetDirectXGlobal("AutoHDREnable",v,"Auto HDR"));
  var vrr=Toggle("Taxa de atualização variável (VRR)",map.GetValueOrDefault("VRROptimizeEnable")=="1",v=>SetDirectXGlobal("VRROptimizeEnable",v,"VRR"));
  var windowed=Toggle("Otimizações para jogos em janela",map.GetValueOrDefault("SwapEffectUpgradeEnable")=="1",v=>SetDirectXGlobal("SwapEffectUpgradeEnable",v,"otimizações para jogos em janela"));
  var gameMode=Toggle("Modo de Jogo",(Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\GameBar")?.GetValue("AutoGameModeEnabled") as int?)!=0,v=>{using var k=Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\GameBar");k.SetValue("AutoGameModeEnabled",v?1:0,RegistryValueKind.DWord);Record("Windows: Modo de Jogo "+(v?"ativado":"desativado"));});
  int hags=(Registry.LocalMachine.OpenSubKey(@"SYSTEM\CurrentControlSet\Control\GraphicsDrivers")?.GetValue("HwSchMode") as int?)??0;
  var hagsToggle=new ToggleSwitch{Header="Agendamento de GPU acelerado por hardware",IsOn=hags==2,OnContent="Ativado",OffContent=hags==0?"Padrão do sistema":"Desativado",MinWidth=250};
  hagsToggle.Toggled+=async(s,e)=>{try{using var p=Process.Start(new ProcessStartInfo("reg.exe",$@"add HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers /v HwSchMode /t REG_DWORD /d {(hagsToggle.IsOn?2:1)} /f"){UseShellExecute=true,Verb="runas",WindowStyle=ProcessWindowStyle.Hidden})!;await p.WaitForExitAsync();Record(p.ExitCode==0?"Windows: agendamento de GPU por hardware "+(hagsToggle.IsOn?"ativado":"desativado")+" · reinicie o computador para aplicar":"Windows: agendamento de GPU não alterado",p.ExitCode!=0);}catch(System.ComponentModel.Win32Exception){Record("Windows: autorização recusada; agendamento de GPU não alterado.");}};
  var wrap=new VariableSizedWrapGrid{Orientation=Orientation.Horizontal,ItemWidth=300,ItemHeight=70};foreach(var t in new UIElement[]{autoHdr,vrr,windowed,gameMode,hagsToggle})wrap.Children.Add(t);
  var panel=new StackPanel{Spacing=6};panel.Children.Add(wrap);panel.Children.Add(Text("Auto HDR exige monitor com HDR ativado (Monitores & Telas). O agendamento de GPU pede administrador e só vale após reiniciar.",11));return panel;
 }
 ComboBox GpuPreference(){
  var combo=new ComboBox{Header="GPU para este jogo",ItemsSource=new[]{"Deixar o Windows decidir","Economia de energia","Alto desempenho"},SelectedIndex=0,MinWidth=220};
  void Load(){var v=File.Exists(profileExe.Text)?Registry.CurrentUser.OpenSubKey(DirectXKey)?.GetValue(profileExe.Text) as string:null;combo.SelectedIndex=v?.Contains("GpuPreference=2")==true?2:v?.Contains("GpuPreference=1")==true?1:0;}
  Load();profileExe.TextChanged+=(s,e)=>Load();
  combo.SelectionChanged+=(s,e)=>{if(!File.Exists(profileExe.Text))return;using var k=Registry.CurrentUser.CreateSubKey(DirectXKey);var old=k.GetValue(profileExe.Text) as string??"";var parts=old.Split(';',StringSplitOptions.RemoveEmptyEntries).Where(p=>!p.StartsWith("GpuPreference=")).Append("GpuPreference="+combo.SelectedIndex);k.SetValue(profileExe.Text,string.Join(";",parts)+";");Record($"Perfil: GPU para {Path.GetFileName(profileExe.Text)} · {combo.SelectedItem}");};
  return combo;
 }
}
