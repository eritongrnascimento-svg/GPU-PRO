#include "gpu.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cstdio>
int main(int argc,char **argv){QCoreApplication app(argc,argv);QCommandLineParser p;p.addOptions({{"id","GPU DXGI LUID","id"},{"domain","Control domain","domain"},{"value","Requested integer value","value"}});p.addOption({"apply","Apply an explicitly requested setting"});p.process(app);GpuBackend backend;Gpu g;for(auto item:backend.enumerate())if(item.pci==p.value("id"))g=item;if(g.pci.isEmpty())return 2;Nvml nv;QJsonObject caps=gpuTuningCaps(g,nv);auto clocks=nv.offsets(nv.handle(g.pci));for(auto domain:{QString("core"),QString("mem")})if(clocks.contains(domain+"Min"))caps[domain]=QJsonObject{{"min",clocks[domain+"Min"]},{"max",clocks[domain+"Max"]},{"current",clocks[domain+"Current"]},{"unit","MHz"}};
 if(!p.isSet("apply")){auto json=QJsonDocument(caps).toJson(QJsonDocument::Compact);fwrite(json.constData(),1,json.size(),stdout);return 0;}
 if(!IsUserAnAdmin())return 3;QString domain=p.value("domain");bool valid=false;int value=p.value("value").toInt(&valid);auto c=caps[domain].toObject();if(!valid||c.isEmpty()||((!domain.startsWith("fan")||value!=-1)&&(value<c["min"].toInt()||value>c["max"].toInt())))return 2;
 QString error;bool ok=(domain=="core"||domain=="mem")?nv.setOffset(g.pci,domain,value,error):setGpuTuning(g,nv,domain,value,error);if(!ok)MessageBoxW(nullptr,reinterpret_cast<const wchar_t*>(error.utf16()),L"GPU-PRO · ajuste não confirmado",MB_OK|MB_ICONWARNING);return ok?0:1;
}
