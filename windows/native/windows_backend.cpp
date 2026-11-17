#include "gpupro_core.h"
#include "windows_devices.h"
#include "windows_disks.h"
#include <windows.h>
#include <pdh.h>
#include <powrprof.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <map>
namespace {
std::string utf8(const wchar_t *w){return WinDevices::text(w);}
std::string quote(std::string s){std::string o="\"";for(unsigned char c:s){if(c=='"'||c=='\\'){o+='\\';o+=char(c);}else if(c=='\n')o+="\\n";else if(c=='\r')o+="\\r";else if(c>=32)o+=char(c);}return o+'"';}
std::uint64_t fileTime(FILETIME t){return (std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
class Monitor {
 PDH_HQUERY query{};PDH_HCOUNTER counter{},gpuCounter{},memoryCounter{},sharedCounter{};std::uint64_t idle=0,total=0;
 HMODULE nv{};void *gpu{};std::mutex mutex;WinDisks::Monitor disks;
 template<class T>T fn(const char *name){return reinterpret_cast<T>(nv?GetProcAddress(nv,name):nullptr);}
 std::string cpuName() {wchar_t s[256]{};DWORD size=sizeof(s);if(RegGetValueW(HKEY_LOCAL_MACHINE,L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",L"ProcessorNameString",RRF_RT_REG_SZ,nullptr,s,&size)==ERROR_SUCCESS)return utf8(s);return "CPU";}
public:
 Monitor(){
  if(PdhOpenQueryW(nullptr,0,&query)==ERROR_SUCCESS){PdhAddEnglishCounterW(query,L"\\Processor Information(*)\\% Processor Time",0,&counter);PdhAddEnglishCounterW(query,L"\\GPU Engine(*)\\Utilization Percentage",0,&gpuCounter);PdhAddEnglishCounterW(query,L"\\GPU Adapter Memory(*)\\Dedicated Usage",0,&memoryCounter);PdhAddEnglishCounterW(query,L"\\GPU Adapter Memory(*)\\Shared Usage",0,&sharedCounter);disks.add(query);PdhCollectQueryData(query);}
  nv=LoadLibraryExW(L"nvml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  auto init=fn<int(*)()>("nvmlInit_v2");auto get=fn<int(*)(unsigned,void**)>("nvmlDeviceGetHandleByIndex_v2");
  if(init&&get&&init()==0)get(0,&gpu);
 }
 ~Monitor(){if(query)PdhCloseQuery(query);auto shutdown=fn<int(*)()>("nvmlShutdown");if(shutdown)shutdown();if(nv)FreeLibrary(nv);}
 std::vector<std::pair<std::wstring,double>> counterValues(PDH_HCOUNTER c){DWORD bytes=0,count=0;PdhGetFormattedCounterArrayW(c,PDH_FMT_DOUBLE,&bytes,&count,nullptr);std::vector<unsigned char> data(bytes);std::vector<std::pair<std::wstring,double>> values;if(bytes&&PdhGetFormattedCounterArrayW(c,PDH_FMT_DOUBLE,&bytes,&count,reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(data.data()))==ERROR_SUCCESS){auto items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(data.data());for(DWORD i=0;i<count;i++)if((items[i].FmtValue.CStatus==0||items[i].FmtValue.CStatus==1)&&std::isfinite(items[i].FmtValue.doubleValue)){std::wstring name=items[i].szName;for(auto &ch:name)ch=wchar_t(towlower(ch));values.emplace_back(name,items[i].FmtValue.doubleValue);}}return values;}
 std::string snapshot(){
  std::lock_guard<std::mutex> lock(mutex);auto inventory=WinDevices::enumerate();WinDevices::Adapter adapter;bool identified=!inventory.empty();if(identified)adapter=inventory[std::min(WinDevices::selected.load(),unsigned(inventory.size()-1))];gpu=identified?WinDevices::nvmlDevice(nv,adapter):nullptr;std::ostringstream out;out.precision(12);
  FILETIME i{},k{},u{};GetSystemTimes(&i,&k,&u);auto nowIdle=fileTime(i),nowTotal=fileTime(k)+fileTime(u);
  double usage=total&&nowTotal>total?100.*(1.-double(nowIdle-idle)/double(nowTotal-total)):-1;idle=nowIdle;total=nowTotal;
  MEMORYSTATUSEX memory{sizeof(memory)};GlobalMemoryStatusEx(&memory);
  SYSTEM_INFO info{};GetSystemInfo(&info);
  struct ProcessorPowerInfo {ULONG Number,MaxMhz,CurrentMhz,MhzLimit,MaxIdleState,CurrentIdleState;};
  std::vector<ProcessorPowerInfo> power(info.dwNumberOfProcessors);
  bool gotPower=CallNtPowerInformation(ProcessorInformation,nullptr,0,power.data(),ULONG(power.size()*sizeof(power[0])))==0;
  out<<"{\"platform\":\"Windows\",\"cpuName\":"<<quote(cpuName())<<",\"logicalProcessors\":"<<info.dwNumberOfProcessors<<",\"ramTotal\":"<<memory.ullTotalPhys<<",\"ramUsed\":"<<memory.ullTotalPhys-memory.ullAvailPhys;
  DWORD topologyBytes=0;GetLogicalProcessorInformationEx(RelationProcessorCore,nullptr,&topologyBytes);std::vector<unsigned char> topology(topologyBytes);unsigned physicalCores=0;if(topologyBytes&&GetLogicalProcessorInformationEx(RelationProcessorCore,reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(topology.data()),&topologyBytes)){for(DWORD offset=0;offset<topologyBytes;){auto item=reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(topology.data()+offset);if(!item->Size||item->Size>topologyBytes-offset)break;if(item->Relationship==RelationProcessorCore)physicalCores++;offset+=item->Size;}}if(physicalCores)out<<",\"physicalCores\":"<<physicalCores;
  if(usage>=0&&std::isfinite(usage))out<<",\"cpuUsage\":"<<std::clamp(usage,0.,100.);
  out<<",\"cores\":[";bool first=true;
  if(query&&counter&&PdhCollectQueryData(query)==ERROR_SUCCESS){
   DWORD bytes=0,count=0;PdhGetFormattedCounterArrayW(counter,PDH_FMT_DOUBLE,&bytes,&count,nullptr);
   std::vector<unsigned char> data(bytes);
   if(bytes&&PdhGetFormattedCounterArrayW(counter,PDH_FMT_DOUBLE,&bytes,&count,reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(data.data()))==ERROR_SUCCESS){
    auto *items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(data.data());unsigned index=0;
    for(DWORD n=0;n<count;++n){std::wstring name=items[n].szName;if(name.find(L"_Total")!=std::wstring::npos)continue;
     auto v=items[n].FmtValue;if(v.CStatus!=0&&v.CStatus!=1)continue;if(!std::isfinite(v.doubleValue))continue;
     if(!first)out<<',';first=false;out<<"{\"name\":"<<quote(utf8(name.c_str()))<<",\"usage\":"<<std::clamp(v.doubleValue,0.,100.);
     if(gotPower&&index<power.size())out<<",\"mhz\":"<<power[index].CurrentMhz;
     out<<'}';++index;
    }
   }
  }
  out<<"],\"disks\":"<<disks.json([this](PDH_HCOUNTER c){return counterValues(c);});
  out<<",\"commitTotal\":"<<memory.ullTotalPageFile<<",\"commitUsed\":"<<memory.ullTotalPageFile-memory.ullAvailPageFile<<",\"memoryLoad\":"<<memory.dwMemoryLoad;
  out<<",\"cpuTemperatureStatus\":\"Temperatura CPU não exposta pelas APIs de usuário deste Windows\",\"gpu\":{";
  std::string name=identified?adapter.name:"Nenhuma GPU física identificada";
  out<<"\"id\":"<<quote(adapter.id)<<",\"vendor\":"<<quote(adapter.vendor)<<",\"deviceId\":\"0x"<<std::hex<<std::setw(4)<<std::setfill('0')<<adapter.desc.DeviceId<<std::dec<<"\",";
  if(gpu){
   char b[256]{};auto getName=fn<int(*)(void*,char*,unsigned)>("nvmlDeviceGetName");if(getName&&getName(gpu,b,sizeof(b))==0)name=b;
   out<<"\"name\":"<<quote(name);
   struct Mem{unsigned long long total,free,used;}m{};auto mem=fn<int(*)(void*,Mem*)>("nvmlDeviceGetMemoryInfo");if(mem&&mem(gpu,&m)==0)out<<",\"vramTotal\":"<<m.total<<",\"vram\":"<<m.used;
   for(auto pair:std::vector<std::pair<const char*,const char*>>{{"power","nvmlDeviceGetPowerUsage"},{"fan","nvmlDeviceGetFanSpeed"},{"memoryBusWidthBits","nvmlDeviceGetMemoryBusWidth"},{"gpuCores","nvmlDeviceGetNumGpuCores"}}){
    auto get=fn<int(*)(void*,unsigned*)>(pair.second);unsigned value=0;if(get&&get(gpu,&value)==0)out<<",\""<<pair.first<<"\":"<<(std::string(pair.first)=="power"?value/1000.:double(value));
   }
   auto temp=fn<int(*)(void*,unsigned,unsigned*)>("nvmlDeviceGetTemperature");unsigned value=0;if(temp&&temp(gpu,0,&value)==0)out<<",\"temp\":"<<value;
   auto clk=fn<int(*)(void*,unsigned,unsigned*)>("nvmlDeviceGetClockInfo");if(clk&&clk(gpu,0,&value)==0)out<<",\"clock\":"<<value;if(clk&&clk(gpu,2,&value)==0)out<<",\"memClock\":"<<value;
   struct Util{unsigned gpu,memory;}v{};auto util=fn<int(*)(void*,Util*)>("nvmlDeviceGetUtilizationRates");if(util&&util(gpu,&v)==0)out<<",\"usage\":"<<v.gpu;
   auto driver=fn<int(*)(char*,unsigned)>("nvmlSystemGetDriverVersion");if(driver&&driver(b,sizeof(b))==0)out<<",\"driverVersion\":"<<quote(b);
  }else{out<<"\"name\":"<<quote(name)<<",\"status\":\"Sem telemetria NVML; sensores não disponíveis ficam N/D\"";}
  if(identified){
   std::wostringstream marker;marker<<L"luid_0x"<<std::hex<<std::setfill(L'0')<<std::setw(8)<<unsigned(adapter.desc.AdapterLuid.HighPart)<<L"_0x"<<std::setw(8)<<adapter.desc.AdapterLuid.LowPart;std::map<std::wstring,double> engines;double used=-1;
   if(gpuCounter)for(auto &[instance,value]:counterValues(gpuCounter)){if(instance.find(marker.str())==std::wstring::npos)continue;auto position=instance.find(L"_eng_");if(position!=std::wstring::npos)engines[instance.substr(position)]+=std::max(0.,value);}
   double busiest=-1,encode=-1,decode=-1;for(const auto &[engine,value]:engines){busiest=std::max(busiest,value);if(engine.find(L"videoencode")!=std::wstring::npos)encode=std::max(encode,value);if(engine.find(L"videodecode")!=std::wstring::npos)decode=std::max(decode,value);}
   if(!gpu&&busiest>=0)out<<",\"usage\":"<<std::clamp(busiest,0.,100.);if(encode>=0)out<<",\"encoder\":"<<std::clamp(encode,0.,100.);if(decode>=0)out<<",\"decoder\":"<<std::clamp(decode,0.,100.);
   double shared=-1;if(sharedCounter)for(auto &[instance,value]:counterValues(sharedCounter))if(instance.find(marker.str())!=std::wstring::npos)shared=std::max(shared,value);if(shared>=0)out<<",\"sharedUsed\":"<<shared;
   if(!gpu){if(memoryCounter)for(auto &[instance,value]:counterValues(memoryCounter))if(instance.find(marker.str())!=std::wstring::npos)used=std::max(used,value);if(used>=0)out<<",\"vram\":"<<used;if(adapter.desc.DedicatedVideoMemory)out<<",\"vramTotal\":"<<adapter.desc.DedicatedVideoMemory;out<<",\"sharedMemoryBytes\":"<<adapter.desc.SharedSystemMemory;}
  }
  double memoryTemp=-1000;std::string sensorStatus;WinDevices::memoryTemperature(nv,gpu,memoryTemp,sensorStatus); struct FieldValue{unsigned fieldId,scopeId;long long timestamp,latency;int valueType,nvmlReturn;union{double d;unsigned u;unsigned long ul;unsigned long long ull;long long sll;}value;}; auto field=fn<int(*)(void*,int,FieldValue*)>("nvmlDeviceGetFieldValues");FieldValue memoryField{};memoryField.fieldId=82;if(gpu&&field&&field(gpu,1,&memoryField)==0&&memoryField.nvmlReturn==0){double value=memoryField.valueType==0?memoryField.value.d:memoryField.valueType==1?double(memoryField.value.u):memoryField.valueType==2?double(memoryField.value.ul):memoryField.valueType==3?double(memoryField.value.ull):-1000;if(value>=-40&&value<=150){memoryTemp=value;sensorStatus="NVML field 82 · temperatura de memória";}}if(memoryTemp>=-40)out<<",\"memoryTemp\":"<<memoryTemp;out<<",\"memoryTempStatus\":"<<quote(sensorStatus)<<",\"hotspotStatus\":\"Nenhum sensor identificado como hotspot foi exposto pelas APIs consultadas\"";
  out<<"}}";return out.str();
 }
};
}
extern "C" GPP_API int gpp_snapshot_json(char *buffer,int capacity){
 try{static Monitor monitor;const auto json=monitor.snapshot();if(!buffer||capacity<=int(json.size()))return int(json.size()+1);memcpy(buffer,json.c_str(),json.size()+1);return int(json.size());}catch(...){if(buffer&&capacity>2){buffer[0]='{';buffer[1]='}';buffer[2]=0;}return 2;}
}
extern "C" GPP_API GppCpuResult gpp_run_cpu(int mode,int threads,int milliseconds){return gpp_cpu_benchmark(mode,threads,milliseconds);}

extern "C" GPP_API int gpp_inventory_json(char *buffer,int capacity){try{auto adapters=WinDevices::enumerate();std::ostringstream out;out<<"[";bool first=true;for(const auto &a:adapters){if(!first)out<<",";first=false;out<<"{\"id\":"<<quote(a.id)<<",\"name\":"<<quote(a.name)<<",\"vendor\":"<<quote(a.vendor)<<",\"deviceId\":\"0x"<<std::hex<<std::setfill('0')<<std::setw(4)<<a.desc.DeviceId<<std::dec<<"\",\"memoryBytes\":"<<a.desc.DedicatedVideoMemory<<",\"sharedMemoryBytes\":"<<a.desc.SharedSystemMemory<<"}";}out<<"]";auto json=out.str();if(!buffer||capacity<=int(json.size()))return int(json.size()+1);memcpy(buffer,json.c_str(),json.size()+1);return int(json.size());}catch(...){return -1;}}
extern "C" GPP_API int gpp_select_gpu(const char *id){if(!id)return -1;auto adapters=WinDevices::enumerate();for(unsigned n=0;n<adapters.size();n++)if(adapters[n].id==id){WinDevices::selected=n;return 0;}return -1;}
