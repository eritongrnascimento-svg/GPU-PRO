// Isolated graphics workloads. This executable has no GPU-setting write path.
#include "gpu.h"
#include <QGuiApplication>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vulkan/vulkan.h>
static void output(QString prefix, QJsonObject o) {
  auto bytes = QJsonDocument(o).toJson(QJsonDocument::Compact);
  printf("%s %s\n", prefix.toLatin1().constData(), bytes.constData());
  fflush(stdout);
}
static void vkCheck(VkResult rc, const char *operation) {
  if (rc != VK_SUCCESS)
    throw std::runtime_error(QString("%1: Vulkan %2").arg(operation).arg(rc).toStdString());
}
static QByteArray resource(QString p) {
  QFile f(p);
  if (!f.open(QIODevice::ReadOnly))
    throw std::runtime_error("Shader ausente");
  return f.readAll();
}
static volatile double cpuSink = 0;
static double cpuWork(int iterations) {
  QElapsedTimer timer;
  timer.start();
  double sum = 0;
  for (int i = 0; i < iterations; ++i) {
    double angle = i * .001;
    double x = std::sin(angle), y = std::cos(angle * .7);
    sum += x * .87 - y * .34 + (x * y) * .23;
  }
  cpuSink = sum;
  return timer.nsecsElapsed() / 1e6;
}
class Graphics {
public:
  virtual ~Graphics() = default;
  virtual double frame(float seconds, int scene) = 0;
  virtual QImage image() = 0;
  QString device, verification;
};
class GLGraphics : public Graphics {
  QOpenGLContext context;
  QOffscreenSurface surface;
  std::unique_ptr<QOpenGLFramebufferObject> fbo;
  QOpenGLShaderProgram program;
  QOpenGLFunctions_3_3_Core gl;
  GLuint vao = 0, query = 0;
  int width, height;

public:
  GLGraphics(const Gpu &target, int w, int h) : width(w), height(h) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(0);
    surface.setFormat(format);
    surface.create();
    context.setFormat(format);
    if (!context.create() || !context.makeCurrent(&surface) || !gl.initializeOpenGLFunctions())
      throw std::runtime_error("Contexto OpenGL 3.3 indisponível");
    device = QString::fromLatin1(reinterpret_cast<const char *>(gl.glGetString(GL_RENDERER)));
    if (device.contains("llvmpipe", Qt::CaseInsensitive) ||
        device.contains("softpipe", Qt::CaseInsensitive) ||
        device.contains("software", Qt::CaseInsensitive))
      throw std::runtime_error("Renderizador por software não elegível");
    bool found = false, other = false;
    for (QString fd : QDir("/proc/self/fdinfo").entryList(QDir::Files)) {
      auto info = readText("/proc/self/fdinfo/" + fd);
      for (QString line : info.split('\n'))
        if (line.startsWith("drm-pdev:")) {
          QString pci = line.mid(9).trimmed();
          found |= pci == target.pci;
          other |= pci != target.pci;
        }
    }
    if (found)
      verification = "DRM-PCI";
    else {
      GpuBackend backend;
      int matches = 0;
      for (auto g : backend.enumerate())
        if (g.deviceId == target.deviceId && g.vendor == target.vendor)
          ++matches;
      QString model = target.name;
      model.remove("NVIDIA ", Qt::CaseInsensitive);
      model.remove("AMD ", Qt::CaseInsensitive);
      model.remove("Intel ", Qt::CaseInsensitive);
      if (other || matches != 1 || !device.contains(model, Qt::CaseInsensitive))
        throw std::runtime_error(
            "OpenGL não confirmou a GPU selecionada; use Vulkan para seleção PCI explícita");
      verification = "renderer-unique-model";
    }
    GLint max = 0;
    gl.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
    if (w > max || h > max)
      throw std::runtime_error("Resolução excede GL_MAX_TEXTURE_SIZE");
    fbo = std::make_unique<QOpenGLFramebufferObject>(w, h);
    if (!fbo->isValid())
      throw std::runtime_error("Sem memória/contexto para o framebuffer solicitado");
    QByteArray vertex =
        "#version 330 core\nvoid main(){vec2 "
        "p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}";
    QByteArray fragment = resource(":/source/city.frag");
    fragment.replace("#version 450", "#version 330 core");
    fragment.replace("layout(push_constant) uniform Settings {vec4 data;} settings;",
                     "uniform vec4 settingsData;");
    fragment.replace("settings.data", "settingsData");
    fragment.replace("uv.y=-uv.y;", "");
    if (!program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex) ||
        !program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragment) || !program.link())
      throw std::runtime_error(program.log().toStdString());
    gl.glGenVertexArrays(1, &vao);
    gl.glGenQueries(1, &query);
  }
  ~GLGraphics() override {
    context.makeCurrent(&surface);
    if (query)
      gl.glDeleteQueries(1, &query);
    if (vao)
      gl.glDeleteVertexArrays(1, &vao);
    fbo.reset();
  }
  double frame(float t, int scene) override {
    context.makeCurrent(&surface);
    fbo->bind();
    program.bind();
    program.setUniformValue("settingsData", QVector4D(t, width, height, scene));
    gl.glViewport(0, 0, width, height);
    gl.glBindVertexArray(vao);
    gl.glBeginQuery(GL_TIME_ELAPSED, query);
    gl.glDrawArrays(GL_TRIANGLES, 0, 3);
    gl.glEndQuery(GL_TIME_ELAPSED);
    GLuint64 ns = 0;
    gl.glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
    GLenum error = gl.glGetError();
    if (error != GL_NO_ERROR)
      throw std::runtime_error(QString("OpenGL erro %1").arg(error).toStdString());
    if (!ns)
      throw std::runtime_error("OpenGL sem timestamp válido");
    return ns / 1e6;
  }
  QImage image() override { return fbo->toImage(); }
};
class VKGraphics : public Graphics {
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice dev = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t family = 0, bits = 0;
  float period = 0;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  VkQueryPool queries = VK_NULL_HANDLE;
  VkImage target = VK_NULL_HANDLE;
  VkDeviceMemory imageMemory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkRenderPass pass = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkShaderModule vertex = VK_NULL_HANDLE, fragment = VK_NULL_HANDLE;
  int width, height;
  uint32_t memoryType(uint32_t allowed, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties p{};
    vkGetPhysicalDeviceMemoryProperties(physical, &p);
    for (uint32_t i = 0; i < p.memoryTypeCount; ++i)
      if ((allowed & (1u << i)) && (p.memoryTypes[i].propertyFlags & flags) == flags)
        return i;
    throw std::runtime_error("Memória Vulkan incompatível");
  }
  VkShaderModule shader(QString path) {
    auto bytes = resource(path);
    QList<uint32_t> aligned(bytes.size() / 4);
    memcpy(aligned.data(), bytes.data(), bytes.size());
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = bytes.size();
    info.pCode = aligned.constData();
    VkShaderModule m{};
    vkCheck(vkCreateShaderModule(dev, &info, nullptr, &m), "shader");
    return m;
  }
  void begin() {
    vkCheck(vkResetCommandBuffer(cmd, 0), "reset command");
    VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(cmd, &b), "begin command");
  }
  void submit() {
    vkCheck(vkEndCommandBuffer(cmd), "end command");
    vkCheck(vkResetFences(dev, 1, &fence), "reset fence");
    VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    s.commandBufferCount = 1;
    s.pCommandBuffers = &cmd;
    vkCheck(vkQueueSubmit(queue, 1, &s, fence), "submit");
    vkCheck(vkWaitForFences(dev, 1, &fence, VK_TRUE, 10000000000ull), "GPU timeout/fence");
  }

public:
  VKGraphics(const Gpu &gpu, int w, int h) : width(w), height(h) {
    try {
      VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
      app.pApplicationName = "LinuxGPUControl benchmark";
      app.apiVersion = VK_API_VERSION_1_1;
      VkInstanceCreateInfo ii{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
      ii.pApplicationInfo = &app;
      vkCheck(vkCreateInstance(&ii, nullptr, &instance), "instance");
      uint32_t n = 0;
      vkCheck(vkEnumeratePhysicalDevices(instance, &n, nullptr), "enumerate");
      QList<VkPhysicalDevice> devices(n);
      vkCheck(vkEnumeratePhysicalDevices(instance, &n, devices.data()), "enumerate");
      int fallback = 0;
      for (auto candidate : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(candidate, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
          continue;
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr);
        QList<VkExtensionProperties> extensions(count);
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, extensions.data());
        bool hasPci = false;
        for (auto ext : extensions)
          if (QByteArray(ext.extensionName) == "VK_EXT_pci_bus_info")
            hasPci = true;
        if (hasPci) {
          VkPhysicalDevicePCIBusInfoPropertiesEXT pci{
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT};
          VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
          p.pNext = &pci;
          vkGetPhysicalDeviceProperties2(candidate, &p);
          QString id = QString("%1:%2:%3.%4")
                           .arg(pci.pciDomain, 4, 16, QChar('0'))
                           .arg(pci.pciBus, 2, 16, QChar('0'))
                           .arg(pci.pciDevice, 2, 16, QChar('0'))
                           .arg(pci.pciFunction);
          if (id == gpu.pci) {
            physical = candidate;
            verification = "DRM-PCI";
            break;
          }
        }
        uint32_t vendor = gpu.vendor == "NVIDIA"  ? 0x10de
                          : gpu.vendor == "AMD"   ? 0x1002
                          : gpu.vendor == "Intel" ? 0x8086
                                                  : 0;
        if (props.vendorID == vendor && props.deviceID == gpu.deviceId.toUInt(nullptr, 16)) {
          physical = candidate;
          ++fallback;
          verification = "unique-vendor-device";
        }
      }
      if (!physical || (verification != "DRM-PCI" && fallback != 1))
        throw std::runtime_error("GPU Vulkan não identificada inequivocamente");
      VkPhysicalDeviceProperties p{};
      vkGetPhysicalDeviceProperties(physical, &p);
      device = p.deviceName;
      period = p.limits.timestampPeriod;
      if (uint32_t(w) > p.limits.maxImageDimension2D || uint32_t(h) > p.limits.maxImageDimension2D)
        throw std::runtime_error("Resolução excede o limite Vulkan");
      n = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(physical, &n, nullptr);
      QList<VkQueueFamilyProperties> q(n);
      vkGetPhysicalDeviceQueueFamilyProperties(physical, &n, q.data());
      bool found = false;
      for (uint32_t i = 0; i < n; ++i)
        if ((q[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && q[i].timestampValidBits) {
          family = i;
          bits = q[i].timestampValidBits;
          found = true;
          break;
        }
      if (!found)
        throw std::runtime_error("Fila gráfica sem timestamps");
      float priority = 1;
      VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
      qi.queueFamilyIndex = family;
      qi.queueCount = 1;
      qi.pQueuePriorities = &priority;
      VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
      di.queueCreateInfoCount = 1;
      di.pQueueCreateInfos = &qi;
      VkPhysicalDeviceFeatures features{};
      vkGetPhysicalDeviceFeatures(physical, &features);
      di.pEnabledFeatures = &features;
      vkCheck(vkCreateDevice(physical, &di, nullptr, &dev), "device");
      vkGetDeviceQueue(dev, family, 0, &queue);
      VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      cp.queueFamilyIndex = family;
      cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      vkCheck(vkCreateCommandPool(dev, &cp, nullptr, &pool), "command pool");
      VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
      ca.commandPool = pool;
      ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      ca.commandBufferCount = 1;
      vkCheck(vkAllocateCommandBuffers(dev, &ca, &cmd), "command buffer");
      VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
      vkCheck(vkCreateFence(dev, &fi, nullptr, &fence), "fence");
      VkQueryPoolCreateInfo query{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
      query.queryCount = 2;
      query.queryType = VK_QUERY_TYPE_TIMESTAMP;
      vkCheck(vkCreateQueryPool(dev, &query, nullptr, &queries), "queries");
      VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
      image.imageType = VK_IMAGE_TYPE_2D;
      image.format = VK_FORMAT_R8G8B8A8_UNORM;
      image.extent = {uint32_t(w), uint32_t(h), 1};
      image.mipLevels = 1;
      image.arrayLayers = 1;
      image.samples = VK_SAMPLE_COUNT_1_BIT;
      image.tiling = VK_IMAGE_TILING_OPTIMAL;
      image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      vkCheck(vkCreateImage(dev, &image, nullptr, &target), "image");
      VkMemoryRequirements req{};
      vkGetImageMemoryRequirements(dev, target, &req);
      VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
      ai.allocationSize = req.size;
      ai.memoryTypeIndex = memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      vkCheck(vkAllocateMemory(dev, &ai, nullptr, &imageMemory), "image memory");
      vkCheck(vkBindImageMemory(dev, target, imageMemory, 0), "bind image");
      VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      vi.image = target;
      vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
      vi.format = image.format;
      vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      vkCheck(vkCreateImageView(dev, &vi, nullptr, &view), "view");
      VkAttachmentDescription attachment{};
      attachment.format = image.format;
      attachment.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
      VkSubpassDescription sub{};
      sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      sub.colorAttachmentCount = 1;
      sub.pColorAttachments = &color;
      VkSubpassDependency dependency{};
      dependency.srcSubpass = 0;
      dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
      dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
      dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
      rp.attachmentCount = 1;
      rp.pAttachments = &attachment;
      rp.subpassCount = 1;
      rp.pSubpasses = &sub;
      rp.dependencyCount = 1;
      rp.pDependencies = &dependency;
      vkCheck(vkCreateRenderPass(dev, &rp, nullptr, &pass), "render pass");
      VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
      fb.renderPass = pass;
      fb.attachmentCount = 1;
      fb.pAttachments = &view;
      fb.width = w;
      fb.height = h;
      fb.layers = 1;
      vkCheck(vkCreateFramebuffer(dev, &fb, nullptr, &framebuffer), "framebuffer");
      vertex = shader(":/shaders/city.vert.spv");
      fragment = shader(":/shaders/city.frag.spv");
      VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16};
      VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
      li.pushConstantRangeCount = 1;
      li.pPushConstantRanges = &range;
      vkCheck(vkCreatePipelineLayout(dev, &li, nullptr, &layout), "layout");
      VkPipelineShaderStageCreateInfo stages[2] = {
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = vertex;
      stages[0].pName = "main";
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = fragment;
      stages[1].pName = "main";
      VkPipelineVertexInputStateCreateInfo input{
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
      VkPipelineInputAssemblyStateCreateInfo assembly{
          VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
      assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      VkViewport viewport{0, 0, float(w), float(h), 0, 1};
      VkRect2D scissor{{0, 0}, {uint32_t(w), uint32_t(h)}};
      VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
      vp.viewportCount = 1;
      vp.pViewports = &viewport;
      vp.scissorCount = 1;
      vp.pScissors = &scissor;
      VkPipelineRasterizationStateCreateInfo raster{
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
      raster.polygonMode = VK_POLYGON_MODE_FILL;
      raster.lineWidth = 1;
      VkPipelineMultisampleStateCreateInfo multi{
          VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
      multi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      VkPipelineColorBlendAttachmentState blend{};
      blend.colorWriteMask = 15;
      VkPipelineColorBlendStateCreateInfo blendState{
          VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
      blendState.attachmentCount = 1;
      blendState.pAttachments = &blend;
      VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
      pipelineInfo.stageCount = 2;
      pipelineInfo.pStages = stages;
      pipelineInfo.pVertexInputState = &input;
      pipelineInfo.pInputAssemblyState = &assembly;
      pipelineInfo.pViewportState = &vp;
      pipelineInfo.pRasterizationState = &raster;
      pipelineInfo.pMultisampleState = &multi;
      pipelineInfo.pColorBlendState = &blendState;
      pipelineInfo.layout = layout;
      pipelineInfo.renderPass = pass;
      vkCheck(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
              "pipeline");
    } catch (...) {
      cleanup();
      throw;
    }
  }
  void cleanup() {
    if (dev) {
      vkDeviceWaitIdle(dev);
      if (pipeline)
        vkDestroyPipeline(dev, pipeline, nullptr);
      if (layout)
        vkDestroyPipelineLayout(dev, layout, nullptr);
      if (vertex)
        vkDestroyShaderModule(dev, vertex, nullptr);
      if (fragment)
        vkDestroyShaderModule(dev, fragment, nullptr);
      if (framebuffer)
        vkDestroyFramebuffer(dev, framebuffer, nullptr);
      if (pass)
        vkDestroyRenderPass(dev, pass, nullptr);
      if (view)
        vkDestroyImageView(dev, view, nullptr);
      if (target)
        vkDestroyImage(dev, target, nullptr);
      if (imageMemory)
        vkFreeMemory(dev, imageMemory, nullptr);
      if (queries)
        vkDestroyQueryPool(dev, queries, nullptr);
      if (fence)
        vkDestroyFence(dev, fence, nullptr);
      if (pool)
        vkDestroyCommandPool(dev, pool, nullptr);
      vkDestroyDevice(dev, nullptr);
      dev = VK_NULL_HANDLE;
    }
    if (instance) {
      vkDestroyInstance(instance, nullptr);
      instance = VK_NULL_HANDLE;
    }
  }
  ~VKGraphics() override { cleanup(); }
  double frame(float t, int scene) override {
    begin();
    vkCmdResetQueryPool(cmd, queries, 0, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries, 0);
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = pass;
    rp.framebuffer = framebuffer;
    rp.renderArea.extent = {uint32_t(width), uint32_t(height)};
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    float values[] = {t, float(width), float(height), float(scene)};
    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, values);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1);
    submit();
    uint64_t times[2];
    vkCheck(vkGetQueryPoolResults(dev, queries, 0, 2, sizeof times, times, sizeof(uint64_t),
                                  VK_QUERY_RESULT_64_BIT),
            "timestamps");
    uint64_t mask = bits == 64 ? ~uint64_t(0) : (uint64_t(1) << bits) - 1;
    return ((times[1] - times[0]) & mask) * period / 1e6;
  }
  QJsonObject compute(const Gpu &, GpuBackend &);
  QImage image() override {
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    VkDeviceSize bytes = VkDeviceSize(width) * height * 4;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = bytes;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vkCheck(vkCreateBuffer(dev, &bi, nullptr, &buffer), "readback buffer");
    VkMemoryRequirements r{};
    vkGetBufferMemoryRequirements(dev, buffer, &r);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = r.size;
    ai.memoryTypeIndex = memoryType(r.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    auto rc = vkAllocateMemory(dev, &ai, nullptr, &memory);
    if (rc != VK_SUCCESS) {
      vkDestroyBuffer(dev, buffer, nullptr);
      vkCheck(rc, "readback memory");
    }
    vkBindBufferMemory(dev, buffer, memory, 0);
    begin();
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {uint32_t(width), uint32_t(height), 1};
    vkCmdCopyImageToBuffer(cmd, target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
    submit();
    void *mapped = nullptr;
    vkCheck(vkMapMemory(dev, memory, 0, bytes, 0, &mapped), "map image");
    QImage result =
        QImage(static_cast<const uchar *>(mapped), width, height, QImage::Format_RGBA8888).copy();
    vkUnmapMemory(dev, memory);
    vkDestroyBuffer(dev, buffer, nullptr);
    vkFreeMemory(dev, memory, nullptr);
    return result;
  }
};
QJsonObject VKGraphics::compute(const Gpu &gpu, GpuBackend &backend) {
  VkPhysicalDeviceFeatures features{};
  vkGetPhysicalDeviceFeatures(physical, &features);
  VkBuffer buffers[3] = {};
  VkDeviceMemory memories[3] = {};
  VkDescriptorSetLayout descriptorLayout{};
  VkDescriptorPool descriptors{};
  VkPipelineLayout computeLayout{};
  VkPipeline computePipeline{};
  VkShaderModule module{};
  auto cleanup = qScopeGuard([&] {
    vkDeviceWaitIdle(dev);
    if (computePipeline)
      vkDestroyPipeline(dev, computePipeline, nullptr);
    if (module)
      vkDestroyShaderModule(dev, module, nullptr);
    if (computeLayout)
      vkDestroyPipelineLayout(dev, computeLayout, nullptr);
    if (descriptors)
      vkDestroyDescriptorPool(dev, descriptors, nullptr);
    if (descriptorLayout)
      vkDestroyDescriptorSetLayout(dev, descriptorLayout, nullptr);
    for (int i = 0; i < 3; ++i) {
      if (buffers[i])
        vkDestroyBuffer(dev, buffers[i], nullptr);
      if (memories[i])
        vkFreeMemory(dev, memories[i], nullptr);
    }
  });
  constexpr VkDeviceSize bytes = 32ull * 1024 * 1024;
  for (int i = 0; i < 3; ++i) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = i == 2 ? 64 : bytes;
    bi.usage = i == 2 ? VK_BUFFER_USAGE_TRANSFER_DST_BIT
                      : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vkCheck(vkCreateBuffer(dev, &bi, nullptr, &buffers[i]), "compute buffer");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(dev, buffers[i], &requirements);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = requirements.size;
    ai.memoryTypeIndex =
        memoryType(requirements.memoryTypeBits, i == 2 ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                                       : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkCheck(vkAllocateMemory(dev, &ai, nullptr, &memories[i]), "compute memory");
    vkCheck(vkBindBufferMemory(dev, buffers[i], memories[i], 0), "bind compute buffer");
  }
  begin();
  vkCmdFillBuffer(cmd, buffers[0], 0, bytes, 0x3f800000u);
  vkCmdFillBuffer(cmd, buffers[1], 0, bytes, 0);
  VkMemoryBarrier inputReady{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  inputReady.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  inputReady.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                       1, &inputReady, 0, nullptr, 0, nullptr);
  submit();
  VkDescriptorSetLayoutBinding bindings[2] = {
      {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
      {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
  VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  dsl.bindingCount = 2;
  dsl.pBindings = bindings;
  vkCheck(vkCreateDescriptorSetLayout(dev, &dsl, nullptr, &descriptorLayout),
          "compute descriptors");
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
  VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  dp.maxSets = 1;
  dp.poolSizeCount = 1;
  dp.pPoolSizes = &size;
  vkCheck(vkCreateDescriptorPool(dev, &dp, nullptr, &descriptors), "compute descriptor pool");
  VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  da.descriptorPool = descriptors;
  da.descriptorSetCount = 1;
  da.pSetLayouts = &descriptorLayout;
  VkDescriptorSet set{};
  vkCheck(vkAllocateDescriptorSets(dev, &da, &set), "compute descriptor set");
  VkDescriptorBufferInfo info[2] = {{buffers[0], 0, bytes}, {buffers[1], 0, bytes}};
  VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
  for (int i = 0; i < 2; ++i) {
    writes[i].dstSet = set;
    writes[i].dstBinding = i;
    writes[i].descriptorCount = 1;
    writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[i].pBufferInfo = &info[i];
  }
  vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);
  VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, 12};
  VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  li.setLayoutCount = 1;
  li.pSetLayouts = &descriptorLayout;
  li.pushConstantRangeCount = 1;
  li.pPushConstantRanges = &range;
  vkCheck(vkCreatePipelineLayout(dev, &li, nullptr, &computeLayout), "compute layout");
  QJsonArray rows, telemetry;
  QStringList kernels = {"read", "write", "copy", "fp32", "fp64", "int32", "int64"};
  int passed = 0;
  for (int index = 0; index < kernels.size(); ++index) {
    QString kernel = kernels[index];
    QString name = QMap<QString, QString>{{"read", "Leitura memória (redução)"},
                                          {"write", "Escrita memória"},
                                          {"copy", "Cópia memória (leitura + escrita)"},
                                          {"fp32", "FP32 FMA"},
                                          {"fp64", "FP64 FMA"},
                                          {"int32", "Inteiros 32 bits"},
                                          {"int64", "Inteiros 64 bits"}}[kernel];
    QString unit = index < 3 ? "GB/s" : index < 5 ? "GFLOPS" : "GIOPS";
    if ((kernel == "fp64" && !features.shaderFloat64) ||
        (kernel == "int64" && !features.shaderInt64)) {
      rows.append(QJsonObject{{"metric", name}, {"unit", unit}, {"status", "unsupported"}});
      continue;
    }
    module = shader(":/shaders/" + kernel + ".comp.spv");
    VkComputePipelineCreateInfo pi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pi.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    pi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pi.stage.module = module;
    pi.stage.pName = "main";
    pi.layout = computeLayout;
    vkCheck(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pi, nullptr, &computePipeline),
            "compute pipeline");
    uint32_t params[] = {index < 3 ? uint32_t(bytes / 4) : 262144u, 1024u, 17u};
    QList<double> times;
    for (int repeat = 0; repeat < 9; ++repeat) {
      begin();
      vkCmdResetQueryPool(cmd, queries, 0, 2);
      vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries, 0);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 1, &set, 0,
                              nullptr);
      vkCmdPushConstants(cmd, computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 12, params);
      vkCmdDispatch(cmd, (params[0] + 255) / 256, 1, 1);
      vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1);
      VkMemoryBarrier ready{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
      ready.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      ready.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &ready, 0, nullptr, 0, nullptr);
      submit();
      uint64_t stamps[2];
      vkCheck(vkGetQueryPoolResults(dev, queries, 0, 2, sizeof stamps, stamps, sizeof(uint64_t),
                                    VK_QUERY_RESULT_64_BIT),
              "compute timestamps");
      uint64_t mask = bits == 64 ? ~uint64_t(0) : (uint64_t(1) << bits) - 1;
      double ms = ((stamps[1] - stamps[0]) & mask) * period / 1e6;
      if (ms <= 0)
        throw std::runtime_error("Timestamp computacional inválido");
      if (repeat)
        times << ms;
      auto sample = backend.sample(gpu);
      if (!sample["temp"].isDouble())
        throw std::runtime_error("Computação sem telemetria térmica confiável");
      if (sample["temp"].toDouble() >= 78)
        throw std::runtime_error("Limite térmico no teste computacional");
      telemetry.append(QJsonObject{
          {"temp", sample["temp"]}, {"power", sample["power"]}, {"clock", sample["clock"]}});
      output(
          "PROGRESS",
          {{"progress", (index * 9 + repeat + 1) * 100 / 63}, {"scene", index}, {"gpuMeanMs", ms}});
    }
    begin();
    VkBufferCopy copy{0, 0, 64};
    vkCmdCopyBuffer(cmd, buffers[1], buffers[2], 1, &copy);
    VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                         &host, 0, nullptr, 0, nullptr);
    submit();
    void *mapped = nullptr;
    vkCheck(vkMapMemory(dev, memories[2], 0, 64, 0, &mapped), "compute validation readback");
    uint32_t values[16];
    memcpy(values, mapped, 64);
    vkUnmapMemory(dev, memories[2]);
    bool correct = true;
    if (kernel == "copy")
      correct = values[0] == 0x3f800000u + 17u;
    else if (kernel == "write")
      correct = values[0] == 1013904223u + 17u;
    else if (kernel == "read")
      correct = values[0] == uint32_t(uint64_t(0x3f800000u) * 256u) + 17u;
    else if (kernel == "fp32") {
      float number;
      memcpy(&number, values, 4);
      correct = std::isfinite(number) && number > 3 && number < 6;
    } else if (kernel == "fp64") {
      uint64_t bits64 = uint64_t(values[0]) | (uint64_t(values[1]) << 32);
      double number;
      memcpy(&number, &bits64, 8);
      correct = std::isfinite(number) && number > 3 && number < 6;
    } else if (kernel == "int32") {
      uint32_t a = 17, b = 17, c = 20, d = 7;
      for (uint32_t j = 0; j < params[1]; ++j) {
        a = a * 1664525u + 1013904223u;
        b = b * 22695477u + 1u;
        c = c * 1103515245u + 12345u;
        d = d * 214013u + 2531011u;
      }
      correct = values[0] == (a ^ b ^ c ^ d);
    } else {
      uint64_t a = 17, b = 17, c = 20, d = 7;
      for (uint32_t j = 0; j < params[1]; ++j) {
        a = a * 1664525u + 1013904223u;
        b = b * 22695477u + 1u;
        c = c * 1103515245u + 12345u;
        d = d * 214013u + 2531011u;
      }
      uint64_t combined = a ^ b ^ c ^ d;
      correct = values[0] == (uint32_t(combined) ^ uint32_t(combined >> 32));
    }
    if (!correct)
      throw std::runtime_error("Validação de saída falhou: " + kernel.toStdString());
    auto timing = summarize(times);
    double ms = 1000 / timing.mean;
    double work =
        index < 3 ? double(bytes) * (kernel == "copy" ? 2 : 1) : double(params[0]) * params[1] * 8;
    double rate = work / (ms * 1e6);
    rows.append(QJsonObject{{"metric", name},
                            {"status", "passed"},
                            {"unit", unit},
                            {"value", rate},
                            {"gpuMeanMs", ms},
                            {"frames", 8}});
    ++passed;
    vkDestroyPipeline(dev, computePipeline, nullptr);
    computePipeline = VK_NULL_HANDLE;
    vkDestroyShaderModule(dev, module, nullptr);
    module = VK_NULL_HANDLE;
  }
  return {{"valid", passed > 0},
          {"kind", "compute"},
          {"api", "Vulkan"},
          {"method", "compute-micro-v1"},
          {"computeResults", rows},
          {"telemetry", telemetry},
          {"supported", passed},
          {"attempted", 7},
          {"deviceVerified", verification},
          {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
}

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QCoreApplication::setApplicationName("gpu-bench-worker");
  QCommandLineParser p;
  p.addHelpOption();
  p.addOptions({{"pci", "GPU PCI", "id"},
                {"api", "Vulkan or OpenGL", "api", "Vulkan"},
                {"width", "Width", "w", "1280"},
                {"height", "Height", "h", "720"},
                {"seconds", "Measured seconds (all scenes)", "seconds", "18"},
                {"cpu-iterations", "CPU transform work per frame", "count", "0"},
                {"preview", "Last rendered frame PNG", "path"},
                {"live-preview", "Atomic JPEG live preview path", "path"}});
  p.addOption({"compute", "Run Vulkan compute microbenchmarks"});
  p.process(app);
  QJsonObject result;
  try {
    int width = p.value("width").toInt(), height = p.value("height").toInt();
    double duration = p.value("seconds").toDouble();
    int iterations = p.value("cpu-iterations").toInt();
    if (width < 320 || height < 240 || width > 7680 || height > 4320 || duration < 3 ||
        duration > 120 || iterations < 0 || iterations > 2000000)
      throw std::runtime_error("Parâmetros fora dos limites");
    GpuBackend backend;
    Gpu gpu;
    bool found = false;
    for (auto g : backend.enumerate())
      if (g.pci == p.value("pci")) {
        gpu = g;
        found = true;
        break;
      }
    if (!found)
      throw std::runtime_error("GPU PCI não encontrada");
    std::unique_ptr<Graphics> graphics;
    if (p.value("api") == "Vulkan")
      graphics = std::make_unique<VKGraphics>(gpu, width, height);
    else if (p.value("api") == "OpenGL")
      graphics = std::make_unique<GLGraphics>(gpu, width, height);
    else
      throw std::runtime_error("API inválida");
    if (p.isSet("compute")) {
      auto *vk = dynamic_cast<VKGraphics *>(graphics.get());
      if (!vk)
        throw std::runtime_error("Computação exige Vulkan");
      result = vk->compute(gpu, backend);
      output("RESULT", result);
      return result["valid"].toBool() ? 0 : 1;
    }
    std::unique_ptr<Graphics> live;
    if (p.isSet("live-preview")) {
      if (p.value("api") == "Vulkan") live = std::make_unique<VKGraphics>(gpu, 640, 360);
      else live = std::make_unique<GLGraphics>(gpu, 640, 360);
    }
    QElapsedTimer lastPreview;
    lastPreview.start();
    QElapsedTimer all, lastOutput;
    all.start();
    lastOutput.start();
    std::array<QList<double>, 3> gpuMs, wallMs, cpuMs;
    QJsonArray samples;
    QElapsedTimer frameTime;
    double warmup = 2;
    while (all.nsecsElapsed() / 1e9 < warmup + duration) {
      double t = all.nsecsElapsed() / 1e9;
      int scene = t < warmup ? -1 : qMin(2, int((t - warmup) / (duration / 3)));
      frameTime.start();
      double cpu = cpuWork(iterations);
      double ms =
          graphics->frame(float(std::fmod(qMax(0., t - warmup), duration / 3)), qMax(0, scene));
      double wall = frameTime.nsecsElapsed() / 1e6;
      if (!std::isfinite(ms) || ms <= 0)
        throw std::runtime_error("Timestamp GPU inválido");
      if (scene >= 0) {
        gpuMs[scene] << ms;
        wallMs[scene] << wall;
        cpuMs[scene] << cpu;
      }
      if (live && lastPreview.elapsed() >= 66) {
        live->frame(float(std::fmod(qMax(0., t - warmup), duration / 3)), qMax(0, scene));
        QSaveFile preview(p.value("live-preview"));
        if (preview.open(QIODevice::WriteOnly) && live->image().save(&preview, "JPEG", 85) && preview.commit())
          output("FRAME", {{"scene", scene}, {"elapsedMs", all.elapsed()}});
        lastPreview.restart();
      }
      if (lastOutput.elapsed() > 250) {
        auto telemetry = backend.sample(gpu);
        if (telemetry["temp"].isDouble() && telemetry["temp"].toDouble() >= 78)
          throw std::runtime_error("Interrupção térmica: GPU >= 78 °C");
        if (telemetry["hotspot"].isDouble() && telemetry["hotspot"].toDouble() >= 95)
          throw std::runtime_error("Interrupção térmica: hotspot >= 95 °C");
        QJsonObject sample{{"elapsedMs", all.elapsed()},
                           {"gpuMeanMs", ms},
                           {"cpuWorkMs", cpu},
                           {"temp", telemetry["temp"]},
                           {"power", telemetry["power"]},
                           {"clock", telemetry["clock"]},
                           {"coreCurrent", telemetry["coreCurrent"]},
                           {"memCurrent", telemetry["memCurrent"]}};
        samples.append(sample);
        output("PROGRESS", {{"progress", qBound(0, int((t - warmup) / duration * 100), 99)},
                            {"scene", scene},
                            {"gpuMeanMs", ms},
                            {"cpuWorkMs", cpu}});
        lastOutput.restart();
      }
    }
    QJsonArray scenes;
    double product = 1;
    int total = 0;
    for (int i = 0; i < 3; ++i) {
      auto gs = summarize(gpuMs[i]), ws = summarize(wallMs[i]);
      if (gs.frames < 3)
        throw std::runtime_error("Amostras insuficientes; aumente a duração ou reduza a resolução");
      double cpu = 0;
      for (double n : cpuMs[i])
        cpu += n;
      cpu /= cpuMs[i].size();
      double time = 1000 / gs.mean, mpix = gs.mean * double(width) * height / 1e6;
      product *= mpix;
      total += gs.frames;
      scenes.append(QJsonObject{{"scene", i},
                                {"gpuMeanMs", time},
                                {"gpuP99Ms", gs.p99},
                                {"presentedFps", ws.mean},
                                {"onePercentLow", ws.low},
                                {"cpuWorkMs", cpu},
                                {"score", mpix},
                                {"frames", gs.frames}});
    }
    result = {{"valid", true},
              {"kind", "graphics"},
              {"api", p.value("api")},
              {"method", "neon-throughput-v0.3"},
              {"shaderVersion", "neon-city-2"},
              {"width", width},
              {"height", height},
              {"durationSeconds", duration},
              {"livePreview", bool(live)},
              {"cpuIterations", iterations},
              {"sceneResults", scenes},
              {"samples", samples},
              {"score", std::cbrt(product)},
              {"scoreUnit", "MPix-s-geomean"},
              {"frames", total},
              {"deviceVerified", graphics->verification},
              {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    if (!p.value("preview").isEmpty())
      graphics->image()
          .scaled(1280, 720, Qt::KeepAspectRatio, Qt::SmoothTransformation)
          .save(p.value("preview"));
  } catch (const std::exception &e) {
    result = {{"valid", false},
              {"kind", "graphics"},
              {"error", e.what()},
              {"errorCode", "RENDER_FAILED"}};
  }
  output("RESULT", result);
  return result["valid"].toBool() ? 0 : 1;
}
