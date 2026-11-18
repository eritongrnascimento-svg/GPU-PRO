// libgpupro_fps.so — contador de quadros do GPU-PRO para Linux, sem MangoHud.
//
// 1. Camada Vulkan implícita (VK_LAYER_GPUPRO_fps): o carregador Vulkan a inclui em todo programa
//    Vulkan — jogos nativos e, pelo Proton, DirectX 9–11 (DXVK) e 12 (VKD3D-Proton). Ela só registra
//    o instante de cada vkQueuePresentKHR.
// 2. OpenGL (opcional): com LD_PRELOAD, pelo lançador gpu-pro-fps, registra glXSwapBuffers e
//    eglSwapBuffers.
//
// A cada 100 ms os tempos entre quadros vão para o GPU-PRO num datagrama local (socket abstrato
// "gpu-pro-frames"); se o GPU-PRO não estiver aberto, o envio falha na hora e nada mais acontece.
// Nenhuma chamada do jogo é alterada. Desative com DISABLE_GPUPRO_FPS=1.
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

#define GPUPRO_EXPORT extern "C" __attribute__((visibility("default")))

namespace {

unsigned long long Now() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<unsigned long long>(ts.tv_sec) * 1000000000ull + static_cast<unsigned long long>(ts.tv_nsec);
}

std::string ProcessName() {
  // No Proton o argv[0] é o .exe do jogo (Z:\...\jogo.exe); fora dele, o executável Linux.
  std::string name;
  if (FILE *f = fopen("/proc/self/cmdline", "rb")) {
    char buffer[4096];
    size_t n = fread(buffer, 1, sizeof buffer - 1, f);
    fclose(f);
    buffer[n] = 0;
    name = buffer;
    size_t cut = name.find_last_of("/\\");
    if (cut != std::string::npos) name = name.substr(cut + 1);
  }
  if (name.empty())
    if (FILE *f = fopen("/proc/self/comm", "r")) {
      char buffer[64] = {};
      if (fgets(buffer, sizeof buffer, f)) name = buffer;
      fclose(f);
      if (!name.empty() && name.back() == '\n') name.pop_back();
    }
  for (char &c : name)
    if (c == '\t' || c == '\n') c = ' ';
  return name;
}

class Reporter {
  std::mutex mutex;
  int fd = -1;
  unsigned long long last = 0, flushAt = 0;
  std::string times, header;
  int count = 0;

  void Flush(unsigned long long now) {
    if (count) {
      if (fd < 0) fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
      if (fd >= 0) {
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        static const char name[] = "gpu-pro-frames";
        memcpy(address.sun_path + 1, name, sizeof name - 1);  // socket abstrato: sun_path[0] = 0
        std::string message = header + times;
        sendto(fd, message.data(), message.size(), MSG_NOSIGNAL, reinterpret_cast<sockaddr *>(&address),
               static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + sizeof name - 1));
      }
    }
    times.clear();
    count = 0;
    flushAt = now + 100000000ull;
  }

public:
  void Frame(const char *api) {
    const unsigned long long now = Now();
    std::lock_guard lock(mutex);
    if (header.empty()) header = "GPUPRO1\t" + std::to_string(getpid()) + "\t" + ProcessName() + "\t" + api + "\t";
    if (last && now > last) {
      const double ms = (now - last) / 1e6;
      if (ms < 5000) {
        char value[32];
        snprintf(value, sizeof value, count ? ",%.3f" : "%.3f", ms);
        times += value;
        ++count;
      }
    }
    last = now;
    if (!flushAt) flushAt = now + 100000000ull;
    if (now >= flushAt || count >= 240) Flush(now);
  }
};

Reporter &Frames() {
  static Reporter reporter;
  return reporter;
}

// --- Camada Vulkan ----------------------------------------------------------------------------

struct InstanceData {
  PFN_vkGetInstanceProcAddr next;
  PFN_vkDestroyInstance destroy;
};
struct DeviceData {
  PFN_vkGetDeviceProcAddr next;
  PFN_vkQueuePresentKHR present;
  PFN_vkDestroyDevice destroy;
};
std::mutex tables;
std::unordered_map<void *, InstanceData> instances;
std::unordered_map<void *, DeviceData> devices;

void *Key(const void *handle) { return *static_cast<void *const *>(handle); }  // tabela de despacho do carregador

VKAPI_ATTR VkResult VKAPI_CALL CreateInstance(const VkInstanceCreateInfo *info, const VkAllocationCallbacks *allocator, VkInstance *instance) {
  auto *link = static_cast<VkLayerInstanceCreateInfo *>(const_cast<void *>(info->pNext));
  while (link && !(link->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && link->function == VK_LAYER_LINK_INFO))
    link = static_cast<VkLayerInstanceCreateInfo *>(const_cast<void *>(link->pNext));
  if (!link) return VK_ERROR_INITIALIZATION_FAILED;
  PFN_vkGetInstanceProcAddr next = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  link->u.pLayerInfo = link->u.pLayerInfo->pNext;
  auto create = reinterpret_cast<PFN_vkCreateInstance>(next(VK_NULL_HANDLE, "vkCreateInstance"));
  if (!create) return VK_ERROR_INITIALIZATION_FAILED;
  VkResult result = create(info, allocator, instance);
  if (result != VK_SUCCESS) return result;
  std::lock_guard lock(tables);
  instances[Key(*instance)] = {next, reinterpret_cast<PFN_vkDestroyInstance>(next(*instance, "vkDestroyInstance"))};
  return result;
}

VKAPI_ATTR void VKAPI_CALL DestroyInstance(VkInstance instance, const VkAllocationCallbacks *allocator) {
  PFN_vkDestroyInstance destroy = nullptr;
  {
    std::lock_guard lock(tables);
    auto it = instances.find(Key(instance));
    if (it == instances.end()) return;
    destroy = it->second.destroy;
    instances.erase(it);
  }
  if (destroy) destroy(instance, allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL CreateDevice(VkPhysicalDevice physical, const VkDeviceCreateInfo *info, const VkAllocationCallbacks *allocator, VkDevice *device) {
  auto *link = static_cast<VkLayerDeviceCreateInfo *>(const_cast<void *>(info->pNext));
  while (link && !(link->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && link->function == VK_LAYER_LINK_INFO))
    link = static_cast<VkLayerDeviceCreateInfo *>(const_cast<void *>(link->pNext));
  if (!link) return VK_ERROR_INITIALIZATION_FAILED;
  PFN_vkGetInstanceProcAddr nextInstance = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  PFN_vkGetDeviceProcAddr nextDevice = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
  link->u.pLayerInfo = link->u.pLayerInfo->pNext;
  auto create = reinterpret_cast<PFN_vkCreateDevice>(nextInstance(VK_NULL_HANDLE, "vkCreateDevice"));
  if (!create) return VK_ERROR_INITIALIZATION_FAILED;
  VkResult result = create(physical, info, allocator, device);
  if (result != VK_SUCCESS) return result;
  std::lock_guard lock(tables);
  devices[Key(*device)] = {nextDevice, reinterpret_cast<PFN_vkQueuePresentKHR>(nextDevice(*device, "vkQueuePresentKHR")),
                           reinterpret_cast<PFN_vkDestroyDevice>(nextDevice(*device, "vkDestroyDevice"))};
  return result;
}

VKAPI_ATTR void VKAPI_CALL DestroyDevice(VkDevice device, const VkAllocationCallbacks *allocator) {
  PFN_vkDestroyDevice destroy = nullptr;
  {
    std::lock_guard lock(tables);
    auto it = devices.find(Key(device));
    if (it == devices.end()) return;
    destroy = it->second.destroy;
    devices.erase(it);
  }
  if (destroy) destroy(device, allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *info) {
  PFN_vkQueuePresentKHR present = nullptr;
  {
    std::lock_guard lock(tables);
    auto it = devices.find(Key(queue));  // filas compartilham a tabela de despacho do dispositivo
    if (it != devices.end()) present = it->second.present;
  }
  if (!present) return VK_ERROR_DEVICE_LOST;
  VkResult result = present(queue, info);
  if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) Frames().Frame("Vulkan");
  return result;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL GetDeviceProcAddr(VkDevice device, const char *name);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL GetInstanceProcAddr(VkInstance instance, const char *name) {
  if (!strcmp(name, "vkGetInstanceProcAddr")) return reinterpret_cast<PFN_vkVoidFunction>(GetInstanceProcAddr);
  if (!strcmp(name, "vkCreateInstance")) return reinterpret_cast<PFN_vkVoidFunction>(CreateInstance);
  if (!strcmp(name, "vkDestroyInstance")) return reinterpret_cast<PFN_vkVoidFunction>(DestroyInstance);
  if (!strcmp(name, "vkCreateDevice")) return reinterpret_cast<PFN_vkVoidFunction>(CreateDevice);
  if (!strcmp(name, "vkGetDeviceProcAddr")) return reinterpret_cast<PFN_vkVoidFunction>(GetDeviceProcAddr);
  if (!strcmp(name, "vkDestroyDevice")) return reinterpret_cast<PFN_vkVoidFunction>(DestroyDevice);
  if (!strcmp(name, "vkQueuePresentKHR")) return reinterpret_cast<PFN_vkVoidFunction>(QueuePresentKHR);
  if (!instance) return nullptr;
  PFN_vkGetInstanceProcAddr next = nullptr;
  {
    std::lock_guard lock(tables);
    auto it = instances.find(Key(instance));
    if (it != instances.end()) next = it->second.next;
  }
  return next ? next(instance, name) : nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL GetDeviceProcAddr(VkDevice device, const char *name) {
  if (!strcmp(name, "vkGetDeviceProcAddr")) return reinterpret_cast<PFN_vkVoidFunction>(GetDeviceProcAddr);
  if (!strcmp(name, "vkDestroyDevice")) return reinterpret_cast<PFN_vkVoidFunction>(DestroyDevice);
  if (!strcmp(name, "vkQueuePresentKHR")) return reinterpret_cast<PFN_vkVoidFunction>(QueuePresentKHR);
  PFN_vkGetDeviceProcAddr next = nullptr;
  {
    std::lock_guard lock(tables);
    auto it = devices.find(Key(device));
    if (it != devices.end()) next = it->second.next;
  }
  return next ? next(device, name) : nullptr;
}

// --- OpenGL (somente com LD_PRELOAD) ----------------------------------------------------------

using DlsymFn = void *(*)(void *, const char *);
DlsymFn RealDlsym() {
  static DlsymFn real = [] {
    auto f = reinterpret_cast<DlsymFn>(dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.34"));
    return f ? f : reinterpret_cast<DlsymFn>(dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5"));
  }();
  return real;
}
// Função original: primeiro a próxima biblioteca na ordem de carga; se o jogo abriu a libGL/libEGL
// com dlopen local (fora do escopo global), busca direto nela.
template <class F> F Next(const char *name, const char *library) {
  if (!RealDlsym()) return nullptr;
  if (void *f = RealDlsym()(RTLD_NEXT, name)) return reinterpret_cast<F>(f);
  for (const char *lib : {library, "libGLX.so.0", "libGL.so.1", "libEGL.so.1"})
    if (void *handle = dlopen(lib, RTLD_LAZY | RTLD_NOLOAD)) {
      void *f = RealDlsym()(handle, name);
      dlclose(handle);
      if (f) return reinterpret_cast<F>(f);
    }
  return nullptr;
}

}  // namespace

GPUPRO_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *version) {
  if (!version || version->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) return VK_ERROR_INITIALIZATION_FAILED;
  if (version->loaderLayerInterfaceVersion >= 2) {
    version->pfnGetInstanceProcAddr = GetInstanceProcAddr;
    version->pfnGetDeviceProcAddr = GetDeviceProcAddr;
    version->pfnGetPhysicalDeviceProcAddr = nullptr;
  }
  if (version->loaderLayerInterfaceVersion > 2) version->loaderLayerInterfaceVersion = 2;
  return VK_SUCCESS;
}

GPUPRO_EXPORT void glXSwapBuffers(void *display, unsigned long drawable) {
  static auto real = Next<void (*)(void *, unsigned long)>("glXSwapBuffers", "libGL.so.1");
  if (real) real(display, drawable);
  Frames().Frame("OpenGL");
}

GPUPRO_EXPORT unsigned eglSwapBuffers(void *display, void *surface) {
  static auto real = Next<unsigned (*)(void *, void *)>("eglSwapBuffers", "libEGL.so.1");
  unsigned result = real ? real(display, surface) : 0;
  Frames().Frame("OpenGL");
  return result;
}

namespace {
void *OwnSymbol(const char *name) {
  if (!name) return nullptr;
  if (!strcmp(name, "glXSwapBuffers")) return reinterpret_cast<void *>(glXSwapBuffers);
  if (!strcmp(name, "eglSwapBuffers")) return reinterpret_cast<void *>(eglSwapBuffers);
  return nullptr;
}
}  // namespace

GPUPRO_EXPORT void *glXGetProcAddress(const unsigned char *name) {
  if (void *own = OwnSymbol(reinterpret_cast<const char *>(name))) return own;
  static auto real = Next<void *(*)(const unsigned char *)>("glXGetProcAddress", "libGL.so.1");
  return real ? real(name) : nullptr;
}

GPUPRO_EXPORT void *glXGetProcAddressARB(const unsigned char *name) {
  if (void *own = OwnSymbol(reinterpret_cast<const char *>(name))) return own;
  static auto real = Next<void *(*)(const unsigned char *)>("glXGetProcAddressARB", "libGL.so.1");
  return real ? real(name) : nullptr;
}

GPUPRO_EXPORT void *eglGetProcAddress(const char *name) {
  if (void *own = OwnSymbol(name)) return own;
  static auto real = Next<void *(*)(const char *)>("eglGetProcAddress", "libEGL.so.1");
  return real ? real(name) : nullptr;
}

// Jogos que abrem a libGL com dlopen e buscam as funções com dlsym (ex.: SDL) passam por aqui.
GPUPRO_EXPORT void *dlsym(void *handle, const char *name) {
  if (void *own = OwnSymbol(name)) return own;
  if (name && !strcmp(name, "glXGetProcAddress")) return reinterpret_cast<void *>(glXGetProcAddress);
  if (name && !strcmp(name, "glXGetProcAddressARB")) return reinterpret_cast<void *>(glXGetProcAddressARB);
  if (name && !strcmp(name, "eglGetProcAddress")) return reinterpret_cast<void *>(eglGetProcAddress);
  return RealDlsym() ? RealDlsym()(handle, name) : nullptr;
}
