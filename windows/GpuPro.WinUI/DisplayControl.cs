using System.Runtime.InteropServices;
namespace GpuPro.Windows;
// Configuração de monitores pelo próprio app: modos (resolução/frequência/orientação), monitor
// principal, escala, HDR, brilho/contraste por DDC/CI e perfil de cor. APIs públicas do Windows
// (ChangeDisplaySettingsEx, DisplayConfig, Dxva2, Mscms). A escala usa o par GET/SET de DPI do
// DisplayConfig empregado pelo próprio Painel de Configurações (não documentado; validado na leitura).
internal static class DisplayControl {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]
 internal struct DevMode{[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string DeviceName;public ushort SpecVersion,DriverVersion,Size,DriverExtra;public uint Fields;public int X,Y;public uint Orientation,FixedOutput;public short Color,Duplex,YResolution,TTOption,Collate;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string FormName;public ushort LogPixels;public uint BitsPerPel,PelsWidth,PelsHeight,DisplayFlags,DisplayFrequency,IcmMethod,IcmIntent,MediaType,DitherType,Reserved1,Reserved2,PanningWidth,PanningHeight;}
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]
 struct DisplayDevice{public uint cb;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string Name;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string Description;public uint Flags;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string Id;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string Key;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool EnumDisplayDevices(string? device,uint n,ref DisplayDevice value,uint flags);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool EnumDisplaySettingsEx(string device,int n,ref DevMode mode,uint flags);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int ChangeDisplaySettingsEx(string? device,ref DevMode mode,nint hwnd,uint flags,nint param);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="ChangeDisplaySettingsExW")]static extern int ApplyPending(string? device,nint mode,nint hwnd,uint flags,nint param);
 const int CurrentSettings=-1;const uint CdsUpdateRegistry=1,CdsTest=2,CdsSetPrimary=0x10,CdsNoReset=0x10000000;
 const uint DmPosition=0x20,DmOrientation=0x80,DmWidth=0x80000,DmHeight=0x100000,DmFrequency=0x400000;
 // ---- DisplayConfig ----
 [StructLayout(LayoutKind.Sequential)]internal struct Luid{public uint Low;public int High;}
 [StructLayout(LayoutKind.Sequential)]struct PathInfo{public Luid SourceAdapter;public uint SourceId,SourceModeIdx,SourceStatus;public Luid TargetAdapter;public uint TargetId,TargetModeIdx,OutputTechnology,Rotation,Scaling;public uint RefreshNum,RefreshDen,ScanlineOrdering;public int TargetAvailable;public uint TargetStatus,Flags;}
 [StructLayout(LayoutKind.Sequential)]struct ModeInfo{public uint InfoType,Id;public Luid Adapter;[MarshalAs(UnmanagedType.ByValArray,SizeConst=48)]public byte[] Data;}
 [StructLayout(LayoutKind.Sequential)]struct Header{public int Type;public uint Size;public Luid Adapter;public uint Id;}
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct SourceName{public Header H;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string GdiName;}
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct TargetName{public Header H;public uint Flags,OutputTechnology;public ushort ManufacturerId,ProductCodeId;public uint ConnectorInstance;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=64)]public string FriendlyName;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string DevicePath;}
 [StructLayout(LayoutKind.Sequential)]struct AdvancedColorInfo{public Header H;public uint Value;public uint ColorEncoding,BitsPerColor;}
 [StructLayout(LayoutKind.Sequential)]struct AdvancedColorState{public Header H;public uint Enable;}
 [StructLayout(LayoutKind.Sequential)]struct DpiGet{public Header H;public int Min,Current,Max;}
 [StructLayout(LayoutKind.Sequential)]struct DpiSet{public Header H;public int Relative;}
 [DllImport("user32.dll")]static extern int GetDisplayConfigBufferSizes(uint flags,out uint paths,out uint modes);
 [DllImport("user32.dll")]static extern int QueryDisplayConfig(uint flags,ref uint paths,[Out]PathInfo[] p,ref uint modes,[Out]ModeInfo[] m,nint topology);
 [DllImport("user32.dll")]static extern int DisplayConfigGetDeviceInfo(ref SourceName v);
 [DllImport("user32.dll")]static extern int DisplayConfigGetDeviceInfo(ref TargetName v);
 [DllImport("user32.dll")]static extern int DisplayConfigGetDeviceInfo(ref AdvancedColorInfo v);
 [DllImport("user32.dll")]static extern int DisplayConfigGetDeviceInfo(ref DpiGet v);
 [DllImport("user32.dll")]static extern int DisplayConfigSetDeviceInfo(ref AdvancedColorState v);
 [DllImport("user32.dll")]static extern int DisplayConfigSetDeviceInfo(ref DpiSet v);
 // ---- DDC/CI ----
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct Physical{public nint Handle;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string Description;}
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct MonitorInfoEx{public uint Size;public int L,T,R,B,WL,WT,WR,WB;public uint Flags;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string Device;}
 delegate bool MonitorEnum(nint monitor,nint dc,nint rect,nint data);
 [DllImport("user32.dll")]static extern bool EnumDisplayMonitors(nint dc,nint clip,MonitorEnum callback,nint data);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool GetMonitorInfo(nint monitor,ref MonitorInfoEx info);
 [DllImport("dxva2.dll")]static extern bool GetNumberOfPhysicalMonitorsFromHMONITOR(nint monitor,out uint count);
 [DllImport("dxva2.dll")]static extern bool GetPhysicalMonitorsFromHMONITOR(nint monitor,uint count,[Out]Physical[] list);
 [DllImport("dxva2.dll")]static extern bool DestroyPhysicalMonitors(uint count,Physical[] list);
 [DllImport("dxva2.dll")]static extern bool GetMonitorBrightness(nint h,out uint min,out uint current,out uint max);
 [DllImport("dxva2.dll")]static extern bool SetMonitorBrightness(nint h,uint value);
 [DllImport("dxva2.dll")]static extern bool GetMonitorContrast(nint h,out uint min,out uint current,out uint max);
 [DllImport("dxva2.dll")]static extern bool SetMonitorContrast(nint h,uint value);
 // ---- Perfil de cor ----
 [DllImport("mscms.dll",CharSet=CharSet.Unicode)]static extern int ColorProfileAddDisplayAssociation(int scope,string profile,Luid adapter,uint sourceId,bool setAsDefault,bool advancedColor);
 [DllImport("mscms.dll",CharSet=CharSet.Unicode)]static extern int ColorProfileGetDisplayDefault(int scope,Luid adapter,uint sourceId,int type,int subtype,out nint profile);
 [DllImport("kernel32.dll")]static extern nint LocalFree(nint p);

 internal record Mode(uint Width,uint Height,uint Hz){public override string ToString()=>$"{Width} × {Height}";}
 internal sealed class Display{
  public string Device="",Name="",Adapter="";public bool Primary;public DevMode Current;public List<Mode> Modes=new();
  public Luid AdapterLuid;public uint SourceId,TargetId;public bool HasPath;
  public bool HdrSupported,HdrEnabled;public int? ScaleCurrent,ScaleRecommended;public int[] Scales=Array.Empty<int>();
  public (uint Min,uint Cur,uint Max)? Brightness,Contrast;public string? ColorProfile;
 }
 internal static readonly int[] DpiSteps={100,125,150,175,200,225,250,300,350,400,450,500};
 internal static List<Display> Read(){
  var result=new List<Display>();var paths=Paths();
  for(uint n=0;n<32;n++){var adapter=new DisplayDevice{cb=(uint)Marshal.SizeOf<DisplayDevice>()};if(!EnumDisplayDevices(null,n,ref adapter,0))break;if((adapter.Flags&1)==0)continue;
   var d=new Display{Device=adapter.Name,Adapter=adapter.Description,Primary=(adapter.Flags&4)!=0};var mon=new DisplayDevice{cb=(uint)Marshal.SizeOf<DisplayDevice>()};d.Name=EnumDisplayDevices(adapter.Name,0,ref mon,0)?mon.Description:adapter.Name;
   var cur=new DevMode{Size=(ushort)Marshal.SizeOf<DevMode>()};EnumDisplaySettingsEx(adapter.Name,CurrentSettings,ref cur,0);d.Current=cur;
   var seen=new HashSet<(uint,uint,uint)>();for(int i=0;;i++){var m=new DevMode{Size=(ushort)Marshal.SizeOf<DevMode>()};if(!EnumDisplaySettingsEx(adapter.Name,i,ref m,0))break;if(m.BitsPerPel>=24&&seen.Add((m.PelsWidth,m.PelsHeight,m.DisplayFrequency)))d.Modes.Add(new(m.PelsWidth,m.PelsHeight,m.DisplayFrequency));}
   if(paths.TryGetValue(adapter.Name,out var p)){d.HasPath=true;d.AdapterLuid=p.SourceAdapter;d.SourceId=p.SourceId;d.TargetId=p.TargetId;
    var t=new TargetName{H=new(){Type=2,Size=(uint)Marshal.SizeOf<TargetName>(),Adapter=p.TargetAdapter,Id=p.TargetId}};if(DisplayConfigGetDeviceInfo(ref t)==0&&!string.IsNullOrWhiteSpace(t.FriendlyName))d.Name=t.FriendlyName;
    var hdr=new AdvancedColorInfo{H=new(){Type=9,Size=(uint)Marshal.SizeOf<AdvancedColorInfo>(),Adapter=p.TargetAdapter,Id=p.TargetId}};if(DisplayConfigGetDeviceInfo(ref hdr)==0){d.HdrSupported=(hdr.Value&1)!=0;d.HdrEnabled=(hdr.Value&2)!=0;}
    var dpi=new DpiGet{H=new(){Type=-3,Size=(uint)Marshal.SizeOf<DpiGet>(),Adapter=p.SourceAdapter,Id=p.SourceId}};
    if(DisplayConfigGetDeviceInfo(ref dpi)==0){int rec=Math.Abs(dpi.Min);if(rec<DpiSteps.Length){d.ScaleRecommended=DpiSteps[rec];int curIdx=Math.Clamp(rec+dpi.Current,0,DpiSteps.Length-1);d.ScaleCurrent=DpiSteps[curIdx];d.Scales=DpiSteps.Take(Math.Min(DpiSteps.Length,rec+dpi.Max+1)).ToArray();}}
    if(ColorProfileGetDisplayDefault(1,p.SourceAdapter,p.SourceId,0,4,out var name)==0&&name!=0){d.ColorProfile=Marshal.PtrToStringUni(name);LocalFree(name);}
   }
   WithPhysical(adapter.Name,h=>{if(GetMonitorBrightness(h,out var a,out var b,out var c))d.Brightness=(a,b,c);if(GetMonitorContrast(h,out a,out b,out c))d.Contrast=(a,b,c);});
   result.Add(d);}
  return result;
 }
 static Dictionary<string,PathInfo> Paths(){
  var map=new Dictionary<string,PathInfo>();if(GetDisplayConfigBufferSizes(2,out var pc,out var mc)!=0)return map;var paths=new PathInfo[pc];var modes=new ModeInfo[mc];if(QueryDisplayConfig(2,ref pc,paths,ref mc,modes,0)!=0)return map;
  foreach(var p in paths.Take((int)pc)){var s=new SourceName{H=new(){Type=1,Size=(uint)Marshal.SizeOf<SourceName>(),Adapter=p.SourceAdapter,Id=p.SourceId}};if(DisplayConfigGetDeviceInfo(ref s)==0&&!map.ContainsKey(s.GdiName))map[s.GdiName]=p;}
  return map;
 }
 static void WithPhysical(string device,Action<nint> action){
  EnumDisplayMonitors(0,0,(monitor,dc,rect,data)=>{var info=new MonitorInfoEx{Size=(uint)Marshal.SizeOf<MonitorInfoEx>()};if(GetMonitorInfo(monitor,ref info)&&info.Device==device&&GetNumberOfPhysicalMonitorsFromHMONITOR(monitor,out var count)&&count>0){var list=new Physical[count];if(GetPhysicalMonitorsFromHMONITOR(monitor,count,list)){try{action(list[0].Handle);}finally{DestroyPhysicalMonitors(count,list);}}}return true;},0);
 }
 static string Error(int code)=>code switch{-1=>"o driver recusou o modo",-2=>"modo inválido para este monitor",-3=>"o registro não pôde ser gravado",-4=>"parâmetros inválidos",-5=>"parâmetros inválidos",-6=>"o sistema não suporta a alteração",1=>"reinicialização necessária",_=>"código "+code};
 // Testa antes de aplicar; o app confirma com o usuário e reverte se não houver resposta.
 internal static void ApplyMode(Display d,Mode mode,uint orientation){
  var m=d.Current;m.PelsWidth=orientation%2==1?mode.Height:mode.Width;m.PelsHeight=orientation%2==1?mode.Width:mode.Height;m.DisplayFrequency=mode.Hz;m.Orientation=orientation;m.Fields=DmWidth|DmHeight|DmFrequency|DmOrientation;
  int test=ChangeDisplaySettingsEx(d.Device,ref m,0,CdsTest,0);if(test!=0)throw new InvalidOperationException("Modo recusado: "+Error(test));
  int r=ChangeDisplaySettingsEx(d.Device,ref m,0,CdsUpdateRegistry,0);if(r!=0)throw new InvalidOperationException("Não aplicado: "+Error(r));
 }
 internal static void Restore(Display d){var m=d.Current;m.Fields=DmWidth|DmHeight|DmFrequency|DmOrientation;ChangeDisplaySettingsEx(d.Device,ref m,0,CdsUpdateRegistry,0);}
 internal static void SetPrimary(List<Display> all,Display chosen){
  int dx=chosen.Current.X,dy=chosen.Current.Y;
  foreach(var d in all){var m=d.Current;m.X-=dx;m.Y-=dy;m.Fields=DmPosition;uint flags=CdsUpdateRegistry|CdsNoReset|(d==chosen?CdsSetPrimary:0);int r=ChangeDisplaySettingsEx(d.Device,ref m,0,flags,0);if(r!=0)throw new InvalidOperationException("Monitor principal não alterado: "+Error(r));}
  ApplyPending(null,0,0,0,0);
 }
 internal static void SetHdr(Display d,bool on){var s=new AdvancedColorState{H=new(){Type=10,Size=(uint)Marshal.SizeOf<AdvancedColorState>(),Adapter=d.AdapterLuid,Id=d.TargetId},Enable=on?1u:0u};int r=DisplayConfigSetDeviceInfo(ref s);if(r!=0)throw new InvalidOperationException("HDR não alterado (código "+r+").");}
 internal static void SetScale(Display d,int percent){int rec=Array.IndexOf(DpiSteps,d.ScaleRecommended??100),target=Array.IndexOf(DpiSteps,percent);if(rec<0||target<0)throw new InvalidOperationException("Escala inválida.");var s=new DpiSet{H=new(){Type=-4,Size=(uint)Marshal.SizeOf<DpiSet>(),Adapter=d.AdapterLuid,Id=d.SourceId},Relative=target-rec};int r=DisplayConfigSetDeviceInfo(ref s);if(r!=0)throw new InvalidOperationException("Escala não alterada (código "+r+").");}
 internal static bool SetBrightness(Display d,uint value){bool ok=false;WithPhysical(d.Device,h=>ok=SetMonitorBrightness(h,value));return ok;}
 internal static bool SetContrast(Display d,uint value){bool ok=false;WithPhysical(d.Device,h=>ok=SetMonitorContrast(h,value));return ok;}
 internal static IEnumerable<string> ColorProfiles(){var folder=Path.Combine(Environment.SystemDirectory,"spool","drivers","color");return Directory.Exists(folder)?Directory.EnumerateFiles(folder).Where(f=>f.EndsWith(".icc",StringComparison.OrdinalIgnoreCase)||f.EndsWith(".icm",StringComparison.OrdinalIgnoreCase)).Select(Path.GetFileName).OfType<string>().OrderBy(f=>f):Enumerable.Empty<string>();}
 internal static void SetColorProfile(Display d,string profile){int r=ColorProfileAddDisplayAssociation(1,profile,d.AdapterLuid,d.SourceId,true,false);if(r!=0)throw new InvalidOperationException("Perfil de cor não associado (HRESULT 0x"+r.ToString("X8")+").");}
}
