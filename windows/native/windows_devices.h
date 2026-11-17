#pragma once
#include <windows.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <atomic>
namespace WinDevices {
struct Adapter{DXGI_ADAPTER_DESC1 desc{};std::string id,name,vendor;};
inline std::string text(const wchar_t *w){int n=WideCharToMultiByte(CP_UTF8,0,w,-1,nullptr,0,nullptr,nullptr);if(n<=1)return {};std::string value(n,0);WideCharToMultiByte(CP_UTF8,0,w,-1,value.data(),n,nullptr,nullptr);value.resize(n-1);return value;}
inline std::string luid(const LUID &id){std::ostringstream out;out<<"luid:"<<std::hex<<std::setfill('0')<<std::setw(8)<<unsigned(id.HighPart)<<std::setw(8)<<id.LowPart;return out.str();}
inline std::vector<Adapter> enumerate(){std::vector<Adapter> result;Microsoft::WRL::ComPtr<IDXGIFactory1> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return result;for(UINT i=0;;i++){Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(i,&adapter)==DXGI_ERROR_NOT_FOUND)break;if(!adapter)continue;Adapter item;if(FAILED(adapter->GetDesc1(&item.desc))||(item.desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))continue;item.id=luid(item.desc.AdapterLuid);item.name=text(item.desc.Description);item.vendor=item.desc.VendorId==0x10de?"NVIDIA":item.desc.VendorId==0x1002?"AMD":item.desc.VendorId==0x8086?"Intel":"Outro";result.push_back(item);}return result;}
inline std::atomic<unsigned> selected{0};
inline void* nvmlDevice(HMODULE nv,const Adapter &adapter){if(!nv||adapter.vendor!="NVIDIA")return nullptr;
 auto byPci=reinterpret_cast<int(*)(const char*,void**)>(GetProcAddress(nv,"nvmlDeviceGetHandleByPciBusId_v2"));
 HMODULE cuda=LoadLibraryExW(L"nvcuda.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);void *handle=nullptr;
 if(cuda){auto init=reinterpret_cast<int(*)(unsigned)>(GetProcAddress(cuda,"cuInit"));auto count=reinterpret_cast<int(*)(int*)>(GetProcAddress(cuda,"cuDeviceGetCount"));auto getLuid=reinterpret_cast<int(*)(char*,unsigned*,int)>(GetProcAddress(cuda,"cuDeviceGetLuid"));auto pci=reinterpret_cast<int(*)(char*,int,int)>(GetProcAddress(cuda,"cuDeviceGetPCIBusId"));int n=0;if(init&&count&&getLuid&&pci&&byPci&&init(0)==0&&count(&n)==0){for(int i=0;i<n;i++){char bytes[8]{};unsigned mask=0;char bus[64]{};if(getLuid(bytes,&mask,i)==0&&!memcmp(bytes,&adapter.desc.AdapterLuid,8)&&pci(bus,sizeof(bus),i)==0){byPci(bus,&handle);break;}}}FreeLibrary(cuda);}
 return handle;
}
struct ThermalSettings{unsigned version,count;struct Sensor{int controller,min,max,current,target;} sensors[3];};
// Public NVAPI ABI: sensor target MEMORY=2, never reclassify BOARD or GPU as hotspot.
inline void memoryTemperature(HMODULE nv,void *device,double &value,std::string &status){
 status="Sensor de memória não exposto pelas APIs consultadas";if(!nv||!device)return;
 struct Pci{char legacy[16];unsigned domain,bus,device,pciDeviceId,pciSubSystemId;char id[32];}pci{};
 auto getPci=reinterpret_cast<int(*)(void*,Pci*)>(GetProcAddress(nv,"nvmlDeviceGetPciInfo_v3"));if(!getPci||getPci(device,&pci)!=0)return;
 HMODULE api=LoadLibraryExW(L"nvapi64.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!api)return;
 auto query=reinterpret_cast<void*(*)(unsigned)>(GetProcAddress(api,"nvapi_QueryInterface"));if(query){auto init=reinterpret_cast<int(*)()>(query(0x0150e828));auto enumerate=reinterpret_cast<int(*)(void**,int*)>(query(0xe5ac921f));auto busId=reinterpret_cast<int(*)(void*,unsigned*)>(query(0x1be0b8e5));auto slotId=reinterpret_cast<int(*)(void*,unsigned*)>(query(0x2a0a350f));auto thermal=reinterpret_cast<int(*)(void*,int,ThermalSettings*)>(query(0xe3640a56));void *handles[64]{};int count=0;
 if(init&&enumerate&&busId&&slotId&&thermal&&init()==0&&enumerate(handles,&count)==0){void *match=nullptr;int matches=0;for(int i=0;i<count&&i<64;i++){unsigned bus=0,slot=0;if(busId(handles[i],&bus)==0&&slotId(handles[i],&slot)==0&&bus==pci.bus&&slot==pci.device){match=handles[i];matches++;}}if(matches==1){ThermalSettings settings{};settings.version=sizeof(settings)|(2u<<16);if(thermal(match,15,&settings)==0){for(unsigned i=0;i<settings.count&&i<3;i++){const auto &sensor=settings.sensors[i];if(sensor.target==2&&sensor.current>=-40&&sensor.current<=150){value=sensor.current;status="NVAPI · sensor explicitamente identificado como memória";break;}}}}}}
 FreeLibrary(api);
}
}
