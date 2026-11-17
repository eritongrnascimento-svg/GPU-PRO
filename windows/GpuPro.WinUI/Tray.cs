using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Runtime.InteropServices;
namespace GpuPro.Windows;
public sealed partial class MainWindow {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct NotifyData{public int Size;public nint Window;public uint Id,Flags,Callback;public nint Icon;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string Tip;public uint State,StateMask;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=256)]public string Info;public uint Timeout;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=64)]public string Title;public uint InfoFlags;public Guid Guid;public nint BalloonIcon;}
 delegate nint Subclass(nint window,uint message,nuint wParam,nint lParam,nuint id,nuint data);
 [DllImport("shell32.dll",CharSet=CharSet.Unicode)]static extern bool Shell_NotifyIcon(uint message,ref NotifyData data);
 [DllImport("comctl32.dll")]static extern bool SetWindowSubclass(nint window,Subclass procedure,nuint id,nuint data);
 [DllImport("comctl32.dll")]static extern bool RemoveWindowSubclass(nint window,Subclass procedure,nuint id);
 [DllImport("comctl32.dll")]static extern nint DefSubclassProc(nint window,uint message,nuint wParam,nint lParam);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern nint LoadImage(nint instance,string file,uint type,int x,int y,uint flags);
 [DllImport("user32.dll")]static extern bool DestroyIcon(nint icon);
 [DllImport("user32.dll")]static extern nint CreatePopupMenu();
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool AppendMenu(nint menu,uint flags,nuint id,string text);
 [DllImport("user32.dll")]static extern uint TrackPopupMenu(nint menu,uint flags,int x,int y,int reserved,nint window,nint rectangle);
 [DllImport("user32.dll")]static extern bool DestroyMenu(nint menu);
 [DllImport("user32.dll")]static extern bool GetCursorPos(out global::Windows.Graphics.PointInt32 point);
 [DllImport("user32.dll")]static extern bool SetForegroundWindow(nint window);
 NotifyData tray;Subclass? trayProcedure;bool trayReady,keepInTray,exiting;
 void InitializeTray(){trayProcedure=TrayMessage;if(!SetWindowSubclass(hwnd,trayProcedure,1,0))return;tray=new NotifyData{Size=Marshal.SizeOf<NotifyData>(),Window=hwnd,Id=1,Flags=7,Callback=0x8001,Tip="GPU-PRO",Icon=LoadImage(0,Path.Combine(AppContext.BaseDirectory,"Assets","gpu-pro.ico"),1,32,32,0x10)};trayReady=Shell_NotifyIcon(0,ref tray);Closed+=(s,e)=>{if(trayReady)Shell_NotifyIcon(2,ref tray);if(tray.Icon!=0)DestroyIcon(tray.Icon);if(trayProcedure!=null)RemoveWindowSubclass(hwnd,trayProcedure,1);};}
 nint TrayMessage(nint h,uint message,nuint w,nint l,nuint id,nuint data){try{if(message==0x10&&trayReady&&keepInTray&&!exiting){AppWindow.Hide();return 0;}if(message==0x8001){if(l==0x202){AppWindow.Show();Activate();}else if(l==0x205){var menu=CreatePopupMenu();AppendMenu(menu,0,1,"Mostrar GPU-PRO");AppendMenu(menu,0,2,"Ocultar");AppendMenu(menu,0,3,"Sair");GetCursorPos(out var point);SetForegroundWindow(hwnd);var choice=TrackPopupMenu(menu,0x102,point.X,point.Y,0,hwnd,0);DestroyMenu(menu);if(choice==1){AppWindow.Show();Activate();}else if(choice==2)AppWindow.Hide();else if(choice==3){exiting=true;Close();}}return 0;}}catch(Exception e){Record("Bandeja: "+e.Message,true);}return DefSubclassProc(h,message,w,l);}
 void BuildTrayControls(){var keep=new CheckBox{Content="Manter na bandeja ao fechar a janela",IsChecked=keepInTray,IsEnabled=trayReady};keep.Checked+=(s,e)=>keepInTray=true;keep.Unchecked+=(s,e)=>keepInTray=false;body.Children.Add(Section("Comportamento",keep,Text("Clique no ícone da bandeja para mostrar o painel. Use Sair no menu da bandeja para encerrar.")));}
}
