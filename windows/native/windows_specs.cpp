// Ficha técnica da GPU selecionada no Windows: DXGI (todas as marcas), NVML (NVIDIA)
// e Vulkan carregado dinamicamente e associado ao adaptador pelo LUID.
// Nada é inferido pelo nome do modelo: campos não expostos simplesmente não são emitidos.
#define VK_NO_PROTOTYPES
#include "gpupro_core.h"
#include "windows_devices.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>
namespace {
std::string jsonText(const std::string &s){std::string o="\"";for(unsigned char c:s){if(c=='"'||c=='\\'){o+='\\';o+=char(c);}else if(c=='\n')o+="\\n";else if(c=='\r')o+="\\r";else if(c=='\t')o+=' ';else if(c>=32)o+=char(c);}return o+'"';}
struct Json {
 std::ostringstream out;bool first=true;
 Json(){out.precision(12);out<<'{';}
 void key(const char *k){if(!first)out<<',';first=false;out<<'"'<<k<<"\":";}
 void text(const char *k,const std::string &v){if(v.empty())return;key(k);out<<jsonText(v);}
 void number(const char *k,double v){key(k);out<<v;}
 void boolean(const char *k,bool v){key(k);out<<(v?"true":"false");}
 void raw(const char *k,const std::string &v){key(k);out<<v;}
 std::string str(){return out.str()+'}';}
};
std::string hex(unsigned value,int width){std::ostringstream o;o<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(width)<<value;return o.str();}
std::string umdVersion(const LUID &luid){
 Microsoft::WRL::ComPtr<IDXGIFactory1> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return {};
 for(UINT i=0;;i++){Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(i,&adapter)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};if(!adapter||FAILED(adapter->GetDesc1(&d))||memcmp(&d.AdapterLuid,&luid,sizeof(LUID)))continue;
  LARGE_INTEGER v{};if(FAILED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),&v)))return {};
  std::ostringstream o;o<<HIWORD(v.HighPart)<<'.'<<LOWORD(v.HighPart)<<'.'<<HIWORD(v.LowPart)<<'.'<<LOWORD(v.LowPart);return o.str();}
 return {};
}
void nvml(Json &j,const WinDevices::Adapter &adapter){
 HMODULE nv=LoadLibraryExW(L"nvml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!nv)return;
 auto init=reinterpret_cast<int(*)()>(GetProcAddress(nv,"nvmlInit_v2"));auto shutdown=reinterpret_cast<int(*)()>(GetProcAddress(nv,"nvmlShutdown"));
 if(!init||init()!=0){FreeLibrary(nv);return;}
 void *h=WinDevices::nvmlDevice(nv,adapter);
 if(h){
  auto fn=[&](const char *name){return GetProcAddress(nv,name);};
  for(auto [key,name]:std::vector<std::pair<const char*,const char*>>{{"nvmlName","nvmlDeviceGetName"},{"uuid","nvmlDeviceGetUUID"},{"vbios","nvmlDeviceGetVbiosVersion"}}){char b[256]{};auto f=reinterpret_cast<int(*)(void*,char*,unsigned)>(fn(name));if(f&&f(h,b,sizeof b)==0)j.text(key,b);}
  char driver[128]{};if(auto f=reinterpret_cast<int(*)(char*,unsigned)>(fn("nvmlSystemGetDriverVersion")))if(f(driver,sizeof driver)==0)j.text("nvidiaDriverVersion",driver);
  std::map<std::string,unsigned> u;
  for(auto [key,name]:std::vector<std::pair<const char*,const char*>>{{"powerLimit","nvmlDeviceGetPowerManagementLimit"},{"powerDefault","nvmlDeviceGetPowerManagementDefaultLimit"},{"pcieGeneration","nvmlDeviceGetCurrPcieLinkGeneration"},{"pcieWidth","nvmlDeviceGetCurrPcieLinkWidth"},{"pcieMaxGeneration","nvmlDeviceGetMaxPcieLinkGeneration"},{"pcieMaxWidth","nvmlDeviceGetMaxPcieLinkWidth"},{"memoryBusWidthBits","nvmlDeviceGetMemoryBusWidth"},{"gpuCores","nvmlDeviceGetNumGpuCores"},{"architectureId","nvmlDeviceGetArchitecture"},{"fanCount","nvmlDeviceGetNumFans"}}){unsigned v=0;auto f=reinterpret_cast<int(*)(void*,unsigned*)>(fn(name));if(f&&f(h,&v)==0){u[key]=v;j.number(key,v);}}
  unsigned lo=0,hi=0;if(auto f=reinterpret_cast<int(*)(void*,unsigned*,unsigned*)>(fn("nvmlDeviceGetPowerManagementLimitConstraints")))if(f(h,&lo,&hi)==0){j.number("powerMin",lo);j.number("powerMax",hi);}
  if(auto f=reinterpret_cast<int(*)(void*,unsigned,unsigned*)>(fn("nvmlDeviceGetMaxClockInfo")))for(auto [key,domain]:std::vector<std::pair<const char*,unsigned>>{{"maxGraphicsMHz",0},{"maxMemoryMHz",2}}){unsigned v=0;if(f(h,domain,&v)==0)j.number(key,v);}
  struct Memory{unsigned long long total,free,used;}m{};if(auto f=reinterpret_cast<int(*)(void*,Memory*)>(fn("nvmlDeviceGetMemoryInfo")))if(f(h,&m)==0)j.number("memoryBytes",double(m.total));
  static const std::map<unsigned,const char*> architectures{{2,"Kepler"},{3,"Maxwell"},{4,"Pascal"},{5,"Volta"},{6,"Turing"},{7,"Ampere"},{8,"Ada Lovelace"},{9,"Hopper"},{10,"Blackwell"}};
  if(u.count("architectureId")&&architectures.count(u["architectureId"]))j.text("architecture",architectures.at(u["architectureId"]));
  struct Pci{char legacy[16];unsigned domain,bus,device,pciDeviceId,pciSubSystemId;char id[32];}pci{};
  if(auto f=reinterpret_cast<int(*)(void*,Pci*)>(fn("nvmlDeviceGetPciInfo_v3")))if(f(h,&pci)==0)j.text("pciBusId",pci.id);
  std::ostringstream fans;fans<<'[';auto speed=reinterpret_cast<int(*)(void*,unsigned,unsigned*)>(fn("nvmlDeviceGetFanSpeed_v2"));
  for(unsigned i=0;speed&&u.count("fanCount")&&i<u["fanCount"]&&i<16;i++){unsigned v=0;if(speed(h,i,&v)!=0)continue;if(fans.tellp()>1)fans<<',';fans<<"{\"index\":"<<i<<",\"percent\":"<<v<<",\"source\":\"NVML\",\"rpmMeaning\":\"percentual do alvo informado pelo driver\"}";}
  fans<<']';j.raw("fans",fans.str());
 }
 if(shutdown)shutdown();FreeLibrary(nv);
}
void vulkan(Json &j,const LUID &luid){
 HMODULE lib=LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!lib){j.text("vulkanStatus","Carregador Vulkan (vulkan-1.dll) não encontrado");return;}
 auto gipa=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib,"vkGetInstanceProcAddr"));
 auto create=gipa?reinterpret_cast<PFN_vkCreateInstance>(gipa(nullptr,"vkCreateInstance")):nullptr;
 VkInstance instance=VK_NULL_HANDLE;VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="GPU-PRO";app.apiVersion=VK_API_VERSION_1_1;
 VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pApplicationInfo=&app;
 if(!create||create(&info,nullptr,&instance)!=VK_SUCCESS){j.text("vulkanStatus","Não foi possível criar instância Vulkan");FreeLibrary(lib);return;}
 auto get=[&](const char *name){return gipa(instance,name);};
 auto destroy=reinterpret_cast<PFN_vkDestroyInstance>(get("vkDestroyInstance"));
 auto enumerate=reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get("vkEnumeratePhysicalDevices"));
 auto props=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(get("vkGetPhysicalDeviceProperties"));
 auto props2=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(get("vkGetPhysicalDeviceProperties2"));
 auto memory=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get("vkGetPhysicalDeviceMemoryProperties"));
 auto extensions=reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(get("vkEnumerateDeviceExtensionProperties"));
 uint32_t count=0;std::vector<VkPhysicalDevice> devices;
 if(enumerate&&enumerate(instance,&count,nullptr)==VK_SUCCESS&&count){devices.resize(count);enumerate(instance,&count,devices.data());}
 VkPhysicalDevice match=VK_NULL_HANDLE;int matches=0;
 for(auto d:devices){if(!props2)break;VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};p.pNext=&id;props2(d,&p);if(id.deviceLUIDValid&&!memcmp(id.deviceLUID,&luid,sizeof(LUID))){match=d;matches++;}}
 if(matches!=1||!props){j.text("vulkanStatus",matches?"GPU não identificada de forma inequívoca no Vulkan":"GPU selecionada não exposta pelo Vulkan");if(destroy)destroy(instance,nullptr);FreeLibrary(lib);return;}
 VkPhysicalDeviceProperties p{};props(match,&p);
 std::ostringstream api;api<<VK_API_VERSION_MAJOR(p.apiVersion)<<'.'<<VK_API_VERSION_MINOR(p.apiVersion)<<'.'<<VK_API_VERSION_PATCH(p.apiVersion);
 j.text("vulkanApi",api.str());j.text("vulkanDevice",p.deviceName);j.boolean("integrated",p.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
 j.number("vulkanMaxImageDimension2D",p.limits.maxImageDimension2D);j.number("vulkanMaxComputeWorkGroupInvocations",p.limits.maxComputeWorkGroupInvocations);
 if(props2&&p.apiVersion>=VK_API_VERSION_1_2){VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};p2.pNext=&driver;props2(match,&p2);std::string name=driver.driverName,detail=driver.driverInfo;j.text("vulkanDriver",detail.empty()?name:name+" · "+detail);}
 if(memory){VkPhysicalDeviceMemoryProperties m{};memory(match,&m);unsigned long long host=0;for(uint32_t i=0;i<m.memoryTypeCount;i++)if(m.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)host=std::max<unsigned long long>(host,m.memoryHeaps[m.memoryTypes[i].heapIndex].size);j.number("hostVisibleHeapBytes",double(host));}
 std::vector<std::string> names;uint32_t n=0;
 if(extensions&&extensions(match,nullptr,&n,nullptr)==VK_SUCCESS&&n){std::vector<VkExtensionProperties> e(n);extensions(match,nullptr,&n,e.data());for(auto &x:e)names.push_back(x.extensionName);std::sort(names.begin(),names.end());}
 auto has=[&](const char *x){return std::binary_search(names.begin(),names.end(),std::string(x));};
 j.boolean("rayTracingPipeline",has("VK_KHR_ray_tracing_pipeline"));j.boolean("videoDecodeQueue",has("VK_KHR_video_decode_queue"));j.boolean("videoEncodeQueue",has("VK_KHR_video_encode_queue"));
 std::string list;for(auto &x:names){if(!list.empty())list+='\n';list+=x;}j.text("vulkanExtensions",list);j.number("vulkanExtensionCount",double(names.size()));
 if(destroy)destroy(instance,nullptr);FreeLibrary(lib);
}
}
extern "C" GPP_API int gpp_specs_json(char *buffer,int capacity){
 try{
  auto adapters=WinDevices::enumerate();Json j;
  if(adapters.empty()){j.text("status","Nenhuma GPU física identificada pelo DXGI");}
  else{
   const auto &a=adapters[std::min(WinDevices::selected.load(),unsigned(adapters.size()-1))];const auto &d=a.desc;
   j.text("id",a.id);j.text("name",a.name);j.text("vendor",a.vendor);
   j.text("pciHardwareId","PCI\\VEN_"+hex(d.VendorId,4)+"&DEV_"+hex(d.DeviceId,4)+"&SUBSYS_"+hex(d.SubSysId,8)+"&REV_"+hex(d.Revision,2));
   j.text("deviceId","0x"+hex(d.DeviceId,4));
   j.number("dedicatedMemoryBytes",double(d.DedicatedVideoMemory));j.number("sharedMemoryBytes",double(d.SharedSystemMemory));
   j.text("driverVersion",umdVersion(d.AdapterLuid));
   nvml(j,a);vulkan(j,d.AdapterLuid);
  }
  auto json=j.str();if(!buffer||capacity<=int(json.size()))return int(json.size()+1);memcpy(buffer,json.c_str(),json.size()+1);return int(json.size());
 }catch(...){return -1;}
}
