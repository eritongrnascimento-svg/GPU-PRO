using Microsoft.UI.Xaml;
using Microsoft.Windows.AppLifecycle;
namespace GpuPro.Windows;
public partial class App:Application {
 private MainWindow? window;private AppInstance? instance;
 public App(){InitializeComponent();UnhandledException+=(s,e)=>MainWindow.AppendLogFile(System.DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss")+" · ERRO NÃO TRATADO · "+e.Exception);}
 protected override async void OnLaunched(LaunchActivatedEventArgs args){instance=AppInstance.FindOrRegisterForKey("GPU-PRO-main-"+System.Diagnostics.Process.GetCurrentProcess().SessionId);if(!instance.IsCurrent){await instance.RedirectActivationToAsync(AppInstance.GetCurrent().GetActivatedEventArgs());Exit();return;}var dispatcher=Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();instance.Activated+=(s,e)=>dispatcher.TryEnqueue(()=>{window?.AppWindow.Show();window?.Activate();});Accent.Install(Resources);window=new MainWindow();window.Activate();}
}
