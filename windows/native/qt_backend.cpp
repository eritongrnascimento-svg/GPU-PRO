#include "gpu.h"
#include "repository.h"
#include "windows_devices.h"
extern "C" int gpp_snapshot_json(char*,int);
extern "C" int gpp_select_gpu(const char*);
QList<Gpu> GpuBackend::enumerate(){QList<Gpu> result;int index=0;for(auto a:WinDevices::enumerate()){Gpu g;g.pci=QString::fromStdString(a.id);g.vendor=QString::fromStdString(a.vendor);g.name=QString::fromStdString(a.name);g.deviceId=QString("0x%1").arg(a.desc.DeviceId,4,16,QChar('0'));g.sys=QString::number(index++);g.specs={{"memoryBytes",double(a.desc.DedicatedVideoMemory)},{"sharedMemoryBytes",double(a.desc.SharedSystemMemory)},{"identitySource","DXGI-LUID"}};result.append(g);}return result;}
QJsonObject GpuBackend::sample(const Gpu &g){if(gpp_select_gpu(g.pci.toUtf8().constData())!=0)return {};QByteArray data(262144,0);int n=gpp_snapshot_json(data.data(),data.size());if(n<2||n>=data.size())return {};return QJsonDocument::fromJson(data.left(n)).object()["gpu"].toObject();}
// No NVML constructor is needed by this read-only Windows worker adapter.
Nvml::Nvml(){wchar_t folder[MAX_PATH]{};GetSystemDirectoryW(folder,MAX_PATH);lib.setFileName(QString::fromWCharArray(folder)+"/nvml.dll");if(!lib.load())return;auto init=fn<int(*)()>("nvmlInit_v2");ready=init&&init()==0;}
Nvml::~Nvml(){if(ready){auto shutdown=fn<int(*)()>("nvmlShutdown");if(shutdown)shutdown();}}
void *Nvml::handle(const QString &id){if(!ready)return nullptr;auto module=GetModuleHandleW(L"nvml.dll");for(auto a:WinDevices::enumerate())if(a.id==id.toStdString())return WinDevices::nvmlDevice(module,a);return nullptr;}
bool runningInWsl(){return false;}
