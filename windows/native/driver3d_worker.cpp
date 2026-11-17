// gpu-3d-worker: lê e grava configurações 3D globais do driver NVIDIA (perfil base do DRS).
//   --read                      JSON com o valor atual de cada configuração conhecida
//   --apply chave=valor ...     grava no perfil base e confere lendo de volta (o app executa elevado)
//   --restore chave ...         volta a configuração ao padrão do driver (também chave=default em --apply)
// IDs e valores vêm de NvApiDriverSettings.h (NVIDIA/nvapi, MIT). Sem NVIDIA/NVAPI: {"available":false}.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
namespace {
#pragma pack(push,4)
struct DrsSetting{uint32_t version;uint16_t name[2048];uint32_t id,type,location,isCurrentPredefined,isPredefinedValid;union{uint32_t u32;uint8_t raw[4100];}predefined;union{uint32_t u32;uint8_t raw[4100];}current;};
#pragma pack(pop)
constexpr uint32_t SettingVersion=uint32_t(sizeof(DrsSetting))|(1u<<16);
struct Known{const char *key;uint32_t id;};
// Somente configurações DWORD globais documentadas no cabeçalho oficial.
const Known known[]={
 {"dlssSrOverride",0x10E41E01},{"dlssSrMode",0x10AFB768},{"dlssSrPreset",0x10E41DF3},{"dlssRrOverride",0x10E41E02},
 {"dlssFgOverride",0x10E41E03},{"dlssMultiFrame",0x104D6667},{"dlaaOverride",0x10E41DF4},{"imageScaling",0x10444444},
 {"vsync",0x00A879CF},{"maxPrerendered",0x007BA09E},{"frameLimit",0x10835002},{"powerMode",0x1057EB71},
 {"anisoSelector",0x10D2BB16},{"anisoLevel",0x101E61A9},{"textureQuality",0x00CE2691},{"vrrApp",0x10A879CF},
 {"shaderCache",0x00198FFF},{"fxaa",0x1074C972}};
using Query=void*(*)(uint32_t);HMODULE lib;Query query;void *session;
template<class T>T fn(uint32_t id){return reinterpret_cast<T>(query?query(id):nullptr);}
bool open(){
 lib=LoadLibraryExW(L"nvapi64.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!lib)return false;query=reinterpret_cast<Query>(GetProcAddress(lib,"nvapi_QueryInterface"));
 auto init=fn<int(*)()>(0x0150e828);auto create=fn<int(*)(void**)>(0x0694d52e);auto load=fn<int(*)(void*)>(0x375dbd6b);
 return init&&create&&load&&init()==0&&create(&session)==0&&load(session)==0;
}
void* base(){void *profile=nullptr;auto get=fn<int(*)(void*,void**)>(0xda8466a0);return get&&get(session,&profile)==0?profile:nullptr;}
bool read(void *profile,uint32_t id,DrsSetting &s){std::memset(&s,0,sizeof s);s.version=SettingVersion;auto get=fn<int(*)(void*,void*,uint32_t,DrsSetting*)>(0x73bf8338);if(!get)return false;int r=get(session,profile,id,&s);if(r!=0&&std::getenv("GPUPRO_DRS_DEBUG"))std::fprintf(stderr,"id %08x -> %d\n",id,r);return r==0;}
const Known* find(const std::string &key){for(auto &k:known)if(key==k.key)return &k;return nullptr;}
void close(){auto destroy=fn<int(*)(void*)>(0xdad9cff8);if(session&&destroy)destroy(session);if(lib)FreeLibrary(lib);}
int print(void *profile){
 std::string out="{\"available\":true,\"settings\":{";bool first=true;
 for(auto &k:known){DrsSetting s;bool ok=read(profile,k.id,s);if(!first)out+=',';first=false;out+="\"";out+=k.key;out+="\":";
  if(ok&&s.type==0){char b[160];snprintf(b,sizeof b,"{\"value\":%u,\"default\":%s,\"location\":%u}",s.current.u32,s.isCurrentPredefined?"true":"false",s.location);out+=b;}
  else out+="{\"value\":null,\"default\":true}";}
 out+="}}";std::puts(out.c_str());return 0;
}
}
int main(int argc,char **argv){
 if(argc<2)return 2;std::string mode=argv[1];
 if(!open()){std::puts("{\"available\":false,\"reason\":\"NVAPI/DRS indisponível (sem driver NVIDIA)\"}");close();return mode=="--read"?0:3;}
 void *profile=base();if(!profile){std::puts("{\"available\":false,\"reason\":\"Perfil base do driver não encontrado\"}");close();return 4;}
 if(mode=="--read"){int r=print(profile);close();return r;}
 auto set=fn<int(*)(void*,void*,DrsSetting*)>(0x577dd202);auto restore=fn<int(*)(void*,void*,uint32_t)>(0x53f0381e);auto save=fn<int(*)(void*)>(0xfcbc7e14);
 std::map<uint32_t,uint32_t> wanted;
 for(int i=2;i<argc;i++){std::string arg=argv[i];auto eq=arg.find('=');auto *k=find(eq==std::string::npos?arg:arg.substr(0,eq));if(!k){close();return 5;}
  if(mode=="--restore"){if(!restore||restore(session,profile,k->id)!=0){close();return 6;}continue;}
  if(mode!="--apply"||eq==std::string::npos){close();return 2;}
  if(arg.substr(eq+1)=="default"){if(!restore||restore(session,profile,k->id)!=0){close();return 6;}continue;}
  char *end=nullptr;unsigned long v=std::strtoul(arg.c_str()+eq+1,&end,0);if(!end||*end){close();return 2;}
  DrsSetting s;std::memset(&s,0,sizeof s);s.version=SettingVersion;s.id=k->id;s.type=0;s.current.u32=uint32_t(v);
  if(!set||set(session,profile,&s)!=0){close();return 6;}wanted[k->id]=uint32_t(v);}
 if(!save||save(session)!=0){close();return 7;}
 // Confirmação: recarrega do driver e compara o que foi gravado.
 auto load=fn<int(*)(void*)>(0x375dbd6b);if(load)load(session);profile=base();
 for(auto &[id,v]:wanted){DrsSetting s;if(!profile||!read(profile,id,s)||s.current.u32!=v){close();return 8;}}
 close();return 0;
}
