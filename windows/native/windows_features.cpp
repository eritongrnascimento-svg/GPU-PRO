// Recursos 3D da GPU selecionada pelo Direct3D 12 (o mesmo que os jogos consultam):
// ray tracing (DXR), mesh shaders, VRS, sampler feedback e shader model máximo.
#include "gpupro_core.h"
#include "windows_devices.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <sstream>
extern "C" GPP_API int gpp_features_json(char *buffer,int capacity){
 try{
  std::ostringstream out;out<<'{';auto adapters=WinDevices::enumerate();
  if(adapters.empty()){out<<"\"d3d12\":false}";}
  else{
   const auto &a=adapters[std::min(WinDevices::selected.load(),unsigned(adapters.size()-1))];
   Microsoft::WRL::ComPtr<IDXGIFactory4> factory;Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;Microsoft::WRL::ComPtr<ID3D12Device> device;
   bool ok=SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->EnumAdapterByLuid(a.desc.AdapterLuid,IID_PPV_ARGS(&adapter)))&&SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
   out<<"\"d3d12\":"<<(ok?"true":"false");
   if(ok){
    D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0,D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};D3D12_FEATURE_DATA_FEATURE_LEVELS fl{};fl.NumFeatureLevels=5;fl.pFeatureLevelsRequested=levels;
    if(SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,&fl,sizeof fl)))out<<",\"featureLevel\":\""<<((fl.MaxSupportedFeatureLevel>>12)&0xF)<<'_'<<((fl.MaxSupportedFeatureLevel>>8)&0xF)<<'"';
    D3D12_FEATURE_DATA_SHADER_MODEL sm{D3D_SHADER_MODEL_6_7};for(auto m:{D3D_SHADER_MODEL_6_7,D3D_SHADER_MODEL_6_6,D3D_SHADER_MODEL_6_5,D3D_SHADER_MODEL_6_0}){sm.HighestShaderModel=m;if(SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&sm,sizeof sm)))break;}
    out<<",\"shaderModel\":\""<<(sm.HighestShaderModel>>4)<<'.'<<(sm.HighestShaderModel&0xF)<<'"';
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5{};if(SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&o5,sizeof o5)))out<<",\"raytracingTier\":"<<int(o5.RaytracingTier)<<",\"renderPassesTier\":"<<int(o5.RenderPassesTier);
    D3D12_FEATURE_DATA_D3D12_OPTIONS6 o6{};if(SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6,&o6,sizeof o6)))out<<",\"vrsTier\":"<<int(o6.VariableShadingRateTier);
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 o7{};if(SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7,&o7,sizeof o7)))out<<",\"meshShaderTier\":"<<int(o7.MeshShaderTier)<<",\"samplerFeedbackTier\":"<<int(o7.SamplerFeedbackTier);
   }
   out<<'}';
  }
  auto json=out.str();if(!buffer||capacity<=int(json.size()))return int(json.size()+1);memcpy(buffer,json.c_str(),json.size()+1);return int(json.size());
 }catch(...){return -1;}
}
