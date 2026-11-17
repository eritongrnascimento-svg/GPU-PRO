using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
namespace GpuPro.Windows;
internal static class Native {
 [StructLayout(LayoutKind.Sequential)] internal struct CpuResult {public double Seconds,PerSecond,Checksum;public ulong Operations;public int Threads,Cancelled;}
 [StructLayout(LayoutKind.Sequential)] internal struct Balance {public double Model,GpuPotential,CpuPotential,GpuLoss,CpuLoss,GpuLost,CpuLost;public int Valid;}
 [StructLayout(LayoutKind.Sequential)] internal struct Frames {public double Current,Mean,Maximum,Low;public int Count,LowAvailable;}
 [DllImport("gpupro-native.dll",CallingConvention=CallingConvention.Cdecl)]static extern int gpp_snapshot_json(byte[] b,int size);
 [DllImport("gpupro-native.dll",CallingConvention=CallingConvention.Cdecl)]internal static extern CpuResult gpp_run_cpu(int mode,int threads,int ms);
 [DllImport("gpupro-native.dll",CallingConvention=CallingConvention.Cdecl)]internal static extern void gpp_cancel_benchmark();
 [DllImport("gpupro-native.dll",CallingConvention=CallingConvention.Cdecl)]internal static extern Balance gpp_balance(double cpu,double gpu);
 [DllImport("gpupro-native.dll",CallingConvention=CallingConvention.Cdecl)]internal static extern Frames gpp_frame_stats(double[] times,int count);
 [DllImport("gpupro-native.dll",CallingConvention=CallingConvention.Cdecl)]static extern int gpp_inventory_json(byte[] b,int size);
 [DllImport("gpupro-native.dll",CallingConvention=CallingConvention.Cdecl)]internal static extern int gpp_select_gpu([MarshalAs(UnmanagedType.LPUTF8Str)]string id);
 internal static JsonElement Inventory(){var bytes=new byte[262144];int n=gpp_inventory_json(bytes,bytes.Length);if(n<2||n>=bytes.Length)throw new InvalidOperationException("Inventário indisponível");return JsonDocument.Parse(bytes.AsMemory(0,n)).RootElement.Clone();}
 [DllImport("gpupro-native.dll",CallingConvention=CallingConvention.Cdecl)]static extern int gpp_features_json(byte[] b,int size);
 internal static JsonElement Features(){var bytes=new byte[65536];int n=gpp_features_json(bytes,bytes.Length);if(n<2||n>=bytes.Length)throw new InvalidOperationException("Recursos 3D indisponíveis");return JsonDocument.Parse(bytes.AsMemory(0,n)).RootElement.Clone();}
 [DllImport("gpupro-native.dll",CallingConvention=CallingConvention.Cdecl)]static extern int gpp_specs_json(byte[] b,int size);
 internal static JsonElement Specs(){var bytes=new byte[1048576];int n=gpp_specs_json(bytes,bytes.Length);if(n<2||n>=bytes.Length)throw new InvalidOperationException("Ficha técnica indisponível");return JsonDocument.Parse(bytes.AsMemory(0,n)).RootElement.Clone();}
 internal static JsonElement Snapshot(){var bytes=new byte[262144];int n=gpp_snapshot_json(bytes,bytes.Length);if(n<2||n>=bytes.Length)throw new InvalidOperationException("Snapshot inválido");return JsonDocument.Parse(bytes.AsMemory(0,n)).RootElement.Clone();}
 internal static double? Number(JsonElement e,string key)=>e.ValueKind==JsonValueKind.Object&&e.TryGetProperty(key,out var v)&&v.TryGetDouble(out var n)&&double.IsFinite(n)?n:null;
 internal static string Text(JsonElement e,string key)=>e.ValueKind==JsonValueKind.Object&&e.TryGetProperty(key,out var v)?v.ToString():"N/D";
 internal static readonly FrameCsvReader reader=new();
 internal static double[] LastTimes=>reader.Samples;
 internal static Frames ReadFrames(string path)=>reader.Read(path);
}
