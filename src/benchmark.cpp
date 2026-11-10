#include "benchmark.h"
#include <QHideEvent>
#include <QVulkanFunctions>
#include <cmath>
#include <stdexcept>
namespace {
QString pciOf(QVulkanInstance &instance, VkPhysicalDevice dev) {
  auto *f = instance.functions();
  uint32_t n = 0;
  f->vkEnumerateDeviceExtensionProperties(dev, nullptr, &n, nullptr);
  QList<VkExtensionProperties> ext(n);
  f->vkEnumerateDeviceExtensionProperties(dev, nullptr, &n, ext.data());
  bool pci = false;
  for (const auto &e : ext)
    if (QByteArray(e.extensionName) == "VK_EXT_pci_bus_info")
      pci = true;
  if (!pci)
    return {};
  auto get = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
      instance.getInstanceProcAddr("vkGetPhysicalDeviceProperties2"));
  if (!get)
    return {};
  VkPhysicalDevicePCIBusInfoPropertiesEXT info{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT};
  VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  p.pNext = &info;
  get(dev, &p);
  return QString("%1:%2:%3.%4")
      .arg(info.pciDomain, 4, 16, QChar('0'))
      .arg(info.pciBus, 2, 16, QChar('0'))
      .arg(info.pciDevice, 2, 16, QChar('0'))
      .arg(info.pciFunction);
}
QList<VkPhysicalDevice> devices(QVulkanInstance &i) {
  uint32_t n = 0;
  i.functions()->vkEnumeratePhysicalDevices(i.vkInstance(), &n, nullptr);
  QList<VkPhysicalDevice> d(n);
  i.functions()->vkEnumeratePhysicalDevices(i.vkInstance(), &n, d.data());
  return d;
}
} // namespace
int vulkanDeviceIndex(QVulkanInstance &i, const Gpu &g) {
  auto d = devices(i);
  int match = -1, count = 0;
  for (int n = 0; n < d.size(); ++n) {
    if (pciOf(i, d[n]) == g.pci)
      return n;
    VkPhysicalDeviceProperties p{};
    i.functions()->vkGetPhysicalDeviceProperties(d[n], &p);
    bool ok = false;
    unsigned id = g.deviceId.toUInt(&ok, 16);
    unsigned vendor = g.vendor == "NVIDIA" ? 0x10de : g.vendor == "AMD" ? 0x1002 : 0x8086;
    if (ok && p.deviceID == id && p.vendorID == vendor) {
      match = n;
      ++count;
    }
  }
  return count == 1 ? match : -1;
}
QJsonObject vulkanSpecifications(const Gpu &g) {
  QVulkanInstance i;
  i.setApiVersion(QVersionNumber(1, 1));
  if (!i.create())
    return {{"Vulkan", "Não foi possível criar instância"}};
  int idx = vulkanDeviceIndex(i, g);
  auto d = devices(i);
  if (idx < 0)
    return {{"Vulkan", "GPU não identificada de forma inequívoca"}};
  auto dev = d[idx];
  VkPhysicalDeviceProperties p{};
  i.functions()->vkGetPhysicalDeviceProperties(dev, &p);
  QJsonObject o{
      {"Vulkan device", p.deviceName},
      {"Vulkan API", QString("%1.%2.%3")
                         .arg(VK_VERSION_MAJOR(p.apiVersion))
                         .arg(VK_VERSION_MINOR(p.apiVersion))
                         .arg(VK_VERSION_PATCH(p.apiVersion))},
      {"Vulkan maxImageDimension2D", int(p.limits.maxImageDimension2D)},
      {"Vulkan maxComputeWorkGroupInvocations", int(p.limits.maxComputeWorkGroupInvocations)},
      {"Vulkan timestampPeriod ns", p.limits.timestampPeriod}};
  uint32_t n = 0;
  i.functions()->vkEnumerateDeviceExtensionProperties(dev, nullptr, &n, nullptr);
  QList<VkExtensionProperties> ext(n);
  i.functions()->vkEnumerateDeviceExtensionProperties(dev, nullptr, &n, ext.data());
  QStringList names;
  for (const auto &e : ext)
    names << e.extensionName;
  names.sort();
  o["Vulkan extensions"] = names.join("\n");
  o["Ray tracing pipeline extension"] = names.contains("VK_KHR_ray_tracing_pipeline")
                                            ? "Exposta pelo driver; depende das features e do jogo"
                                            : "Não exposta";
  return o;
}
