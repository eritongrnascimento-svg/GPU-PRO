#include "gpu.h"
#include <cstring>
bool runningInWsl() {
  return readText("/proc/sys/kernel/osrelease").contains("microsoft",Qt::CaseInsensitive);
}
QList<Gpu> Nvml::wslDevices() {
  QList<Gpu> result;if(!runningInWsl()||!ready)return result;
  auto count=fn<int(*)(unsigned*)>("nvmlDeviceGetCount_v2");
  auto byIndex=fn<int(*)(unsigned,void**)>("nvmlDeviceGetHandleByIndex_v2");
  struct Pci {char legacy[16];unsigned domain,bus,device,deviceId,subsystemId;char busId[32];};
  static_assert(sizeof(Pci)==68);
  auto pciInfo=fn<int(*)(void*,Pci*)>("nvmlDeviceGetPciInfo_v3");
  unsigned total=0;if(!count||!byIndex||!pciInfo||count(&total)!=0)return result;
  for(unsigned i=0;i<qMin(total,64u);++i) {
    void *h=nullptr;Pci pci{};
    if(byIndex(i,&h)!=0||!h||pciInfo(h,&pci)!=0)continue;
    QString bus=QString::fromLatin1(pci.busId,int(strnlen(pci.busId,sizeof(pci.busId)))).toLower();
    if(!QRegularExpression("^[0-9a-f]{8}:[0-9a-f]{2}:[0-9a-f]{2}\\.[0-7]$").match(bus).hasMatch())continue;
    if(bus.startsWith("0000"))bus.remove(0,4);
    Gpu gpu;gpu.pci=bus;gpu.vendor="NVIDIA";gpu.nvIndex=int(i);gpu.driver="NVIDIA / WSL";
    gpu.deviceId=QString("0x%1").arg(pci.deviceId>>16,4,16,QChar('0'));
    gpu.specs=specs(h);gpu.name=gpu.specs["name"].toString("NVIDIA");
    gpu.uuid=gpu.specs["uuid"].toString();gpu.specs["vendorId"]="0x10de";
    gpu.specs["deviceId"]=gpu.deviceId;gpu.specs["wslReadOnly"]=true;
    gpu.specs["telemetryMode"]="WSL: leitura NVML; sensores não expostos aparecem como N/D";
    for(auto pair:QList<QPair<QString,QString>>{{"pcieSpeed","pcieGeneration"},{"pcieMaxSpeed","pcieMaxGeneration"}})
      if(gpu.specs[pair.second].isDouble())gpu.specs[pair.first]="PCIe Gen "+QString::number(gpu.specs[pair.second].toInt());
    result.append(gpu);
  }
  return result;
}
