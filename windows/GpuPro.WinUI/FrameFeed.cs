using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO.Pipes;
using System.Security.Principal;
using System.Text.Json;
namespace GpuPro.Windows;

// FPS dos jogos pelo contador próprio do GPU-PRO (gpupro-frames.exe), sem programas externos.
// O contador lê os eventos de apresentação do Windows e publica os tempos entre quadros de cada
// processo no pipe GPU-PRO-frames; este leitor não precisa de administrador.
internal sealed class FrameFeed {
 internal sealed class Source {
  internal string Name="",Api="";internal readonly Queue<double> Times=new();internal DateTime Last;internal int Recent;internal DateTime RecentStart=DateTime.UtcNow;internal double Rate;
 }
 internal const string ServiceName="GPUPROFrames";
 internal static readonly string Executable=Path.Combine(AppContext.BaseDirectory,"gpupro-frames.exe");
 // Processos que apresentam quadros mas não são jogos: compositor, sobreposições, navegadores, gravação.
 static readonly HashSet<string> ignored=new(StringComparer.OrdinalIgnoreCase){"","dwm.exe","GPU-PRO.exe","gpupro-frames.exe","explorer.exe","obs64.exe","obs32.exe","NVIDIA Overlay.exe","nvcontainer.exe","NVIDIA Share.exe","steamwebhelper.exe","chrome.exe","msedge.exe","msedgewebview2.exe","firefox.exe","opera.exe","brave.exe","Discord.exe","Teams.exe","ms-teams.exe","WindowsTerminal.exe","ApplicationFrameHost.exe","ShellExperienceHost.exe","StartMenuExperienceHost.exe","SearchHost.exe","TextInputHost.exe","LockApp.exe","PresentMon.exe","presentmon.exe","RadeonSoftware.exe","AMDRSServ.exe","XboxGameBar.exe","GameBar.exe"};
 readonly ConcurrentDictionary<int,Source> sources=new();
 volatile bool connected;Process? session;int sticky;
 internal bool Connected=>connected;
 internal string? LastError{get;private set;}
 internal FrameFeed(){var thread=new Thread(Run){IsBackground=true,Name="GPU-PRO frames"};thread.Start();}
 internal static bool Elevated(){using var id=WindowsIdentity.GetCurrent();return new WindowsPrincipal(id).IsInRole(WindowsBuiltInRole.Administrator);}
 internal static bool ServiceInstalled(){try{using var key=Microsoft.Win32.Registry.LocalMachine.OpenSubKey(@"SYSTEM\CurrentControlSet\Services\"+ServiceName);return key!=null;}catch{return false;}}
 void Run(){
  while(true){
   try{
    using var pipe=new NamedPipeClientStream(".","GPU-PRO-frames",PipeDirection.In);
    pipe.Connect(1000);connected=true;LastError=null;
    using var reader=new StreamReader(pipe);
    for(string? line;(line=reader.ReadLine())!=null;)Parse(line);
   }catch(TimeoutException){}catch(Exception e){LastError=e.Message;}
   connected=false;Thread.Sleep(1500);
  }
 }
 void Parse(string line){
  using var doc=JsonDocument.Parse(line);var now=DateTime.UtcNow;
  if(!doc.RootElement.TryGetProperty("p",out var list))return;
  foreach(var p in list.EnumerateArray()){
   int pid=p.GetProperty("pid").GetInt32();var s=sources.GetOrAdd(pid,_=>new Source());
   lock(s){s.Name=p.GetProperty("n").GetString()??"";s.Api=p.GetProperty("s").GetString()??"";
    foreach(var f in p.GetProperty("f").EnumerateArray()){s.Times.Enqueue(f.GetDouble());if(s.Times.Count>240)s.Times.Dequeue();s.Recent++;}
    s.Last=now;var span=(now-s.RecentStart).TotalSeconds;if(span>=1){s.Rate=s.Recent/span;s.Recent=0;s.RecentStart=now;}}
  }
  foreach(var (pid,s) in sources)if(now-s.Last>TimeSpan.FromSeconds(10))sources.TryRemove(pid,out _);
 }
 // Processos apresentando quadros agora (último quadro há menos de 2 s), do mais rápido ao mais lento.
 internal List<(int Pid,string Name,string Api,double Fps)> Active(bool includeIgnored=false){
  var now=DateTime.UtcNow;var list=new List<(int,string,string,double)>();
  foreach(var (pid,s) in sources)lock(s){if(now-s.Last<TimeSpan.FromSeconds(2)&&(includeIgnored||!ignored.Contains(s.Name)))list.Add((pid,s.Name,s.Api,s.Rate));}
  return list.OrderByDescending(x=>x.Item4).ToList();
 }
 internal double[] Times(int pid){if(!sources.TryGetValue(pid,out var s))return Array.Empty<double>();lock(s){return DateTime.UtcNow-s.Last<TimeSpan.FromSeconds(3)?s.Times.ToArray():Array.Empty<double>();}}
 // Escolha automática: janela em primeiro plano se estiver apresentando quadros; senão o último jogo
 // escolhido enquanto continuar ativo; senão o processo com mais quadros por segundo.
 internal int Pick(uint foreground){
  var active=Active();if(active.Count==0){sticky=0;return 0;}
  if(active.Any(a=>a.Pid==foreground))return sticky=(int)foreground;
  if(sticky!=0&&active.Any(a=>a.Pid==sticky))return sticky;
  return sticky=active[0].Pid;
 }
 internal string Describe(int pid){if(!sources.TryGetValue(pid,out var s))return "";lock(s){return s.Name+" · "+(s.Api=="Kernel"?"Vulkan/OpenGL/DX":s.Api);}}
 // Sem o serviço: se o GPU-PRO já roda como administrador, o contador abre junto, sem confirmação.
 internal void StartSession(bool ask){
  if(connected||(session!=null&&!session.HasExited)||!File.Exists(Executable))return;
  if(!ask&&!Elevated())return;
  var start=new ProcessStartInfo(Executable,"--console --parent "+Environment.ProcessId){UseShellExecute=true,WindowStyle=ProcessWindowStyle.Hidden};
  if(!Elevated())start.Verb="runas";
  session=Process.Start(start);
 }
 internal static async Task<int> RunElevated(string args){
  var start=new ProcessStartInfo(Executable,args){UseShellExecute=true,Verb="runas",WindowStyle=ProcessWindowStyle.Hidden};
  using var p=Process.Start(start)??throw new IOException("O contador de quadros não iniciou.");
  await p.WaitForExitAsync();return p.ExitCode;
 }
}
