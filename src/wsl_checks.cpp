#include "gpu.h"
#include <cstdio>
int main(int argc,char **argv) {
  QCoreApplication app(argc,argv);
  if(!runningInWsl())return 77;
  Nvml nv;const auto devices=nv.wslDevices();if(devices.isEmpty())return 77;
  GpuBackend backend;const auto inventory=backend.enumerate();int failures=0;QSet<QString> seen;
  auto check=[&](bool ok,const char *why){if(!ok){++failures;fprintf(stderr,"FAIL %s\n",why);}};
  check(!inventory.isEmpty()&&inventory.first().vendor=="NVIDIA","real NVML GPU is the default, not virtual vgem");
  for(const auto &gpu:devices) {
    check(gpu.vendor=="NVIDIA"&&gpu.specs["wslReadOnly"].toBool(),"WSL NVIDIA is read-only");
    check(gpu.sys.isEmpty()&&gpu.render.isEmpty(),"no fictitious native sysfs/render paths");
    check(!seen.contains(gpu.pci),"no duplicate PCI identities");seen.insert(gpu.pci);
    int count=0;for(const auto &item:inventory)if(item.pci==gpu.pci)++count;
    check(count==1,"NVML GPU discovered once without native DRM");
    auto sample=backend.sample(gpu);
    check(sample["pci"].toString()==gpu.pci&&sample["name"].toString()==gpu.name,"metrics retain GPU identity");
    check(!sample.contains("error")&&sample["wslReadOnly"].toBool(),"WSL NVML handle returns real metrics");
    check(sample["tuning"].toObject().isEmpty()&&!sample.contains("coreCurrent"),"WSL never advertises writable controls");
    check(gpuTuningCaps(gpu,nv).isEmpty(),"read-only device refuses tuning capabilities");
    check(gpu.specs["architecture"].isString()||gpu.specs["memoryBytes"].isDouble(),"NVML specifications are available");
  }
  fprintf(stdout,"WSL NVML: %lld GPU(s), %d failure(s)\n",static_cast<long long>(devices.size()),failures);
  return failures?1:0;
}
