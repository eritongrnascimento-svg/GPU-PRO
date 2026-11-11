#include "intel_specs.h"
#include <cstring>
#include <fcntl.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <unistd.h>
QJsonObject parseIntelTopology(const QByteArray &bytes) {
  if (bytes.size() < int(sizeof(drm_i915_query_topology_info)))
    return {};
  auto *t = reinterpret_cast<const drm_i915_query_topology_info *>(bytes.constData());
  const int size = bytes.size() - sizeof(*t);
  const auto *data = t->data;
  if (!t->max_slices || t->max_slices > 32 || !t->max_subslices || t->max_subslices > 64 ||
      !t->max_eus_per_subslice || t->max_eus_per_subslice > 64)
    return {};
  auto bit = [&](int offset, int n) {
    int b = offset + n / 8;
    return b >= 0 && b < size && (data[b] & (1 << (n % 8)));
  };
  int slices = 0, subslices = 0, eus = 0;
  for (int s = 0; s < t->max_slices; ++s)
    if (bit(0, s)) {
      ++slices;
      for (int ss = 0; ss < t->max_subslices; ++ss)
        if (bit(t->subslice_offset + s * t->subslice_stride, ss)) {
          ++subslices;
          for (int eu = 0; eu < t->max_eus_per_subslice; ++eu)
            if (bit(t->eu_offset + (s * t->max_subslices + ss) * t->eu_stride, eu))
              ++eus;
        }
    }
  if (eus == 0)
    return {};
  return {{"executionUnits", eus},
          {"subsliceCount", subslices},
          {"sliceCount", slices},
          {"topologySource", "DRM i915 query"}};
}
QJsonObject intelSpecifications(const Gpu &g) {
  QJsonObject out{{"driverVersion", "kernel " + QSysInfo::kernelVersion()},
                  {"driver", g.driver},
                  {"intelProbeStatus", "Sem topologia exposta por esta interface"}};
  const QMap<QString, QString> integrated{{"0x5912", "Kaby Lake · Gen9.5"},
                                          {"0x5916", "Kaby Lake · Gen9.5"},
                                          {"0xa7a8", "Raptor Lake · Xe-LP"},
                                          {"0xa7a0", "Raptor Lake · Xe-LP"}};
  if (integrated.contains(g.deviceId)) {
    out["architecture"] = integrated[g.deviceId];
    out["integrated"] = true;
    out["memoryType"] = "RAM compartilhada (UMA)";
  }
  QStringList roots{g.sys, g.sys + "/tile0/gt0/freq0"};
  for (auto card : QDir(g.sys + "/drm").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    if (QRegularExpression("^card[0-9]+$").match(card).hasMatch())
      roots << g.sys + "/drm/" + card;
  for (auto root : roots)
    for (auto pair : QList<QPair<QString, QString>>{{"maxGraphicsMHz", "/gt_RP0_freq_mhz"},
                                                    {"minGraphicsMHz", "/gt_RPn_freq_mhz"},
                                                    {"maxGraphicsMHz", "/gt/gt0/rps_RP0_freq_mhz"},
                                                    {"minGraphicsMHz", "/gt/gt0/rps_RPn_freq_mhz"},
                                                    {"maxGraphicsMHz", "/rp0_freq"},
                                                    {"minGraphicsMHz", "/rpn_freq"}})
      if (!out.contains(pair.first))
        if (auto n = readNumber(root + pair.second))
          out[pair.first] = *n;
  if (g.render.isEmpty()) {
    out["intelProbeStatus"] = "Render node não localizado";
    return out;
  }
  int fd = ::open(QFile::encodeName(g.render).constData(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    out["intelProbeStatus"] = "Sem acesso ao render node para consulta DRM";
    return out;
  }
  auto close = qScopeGuard([&] { ::close(fd); });
  if (g.driver != "i915") {
    out["intelProbeStatus"] = "Driver " + g.driver + "; capacidades adicionais via consulta Vulkan";
    return out;
  }
  drm_i915_query_item item{};
  item.query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
  drm_i915_query query{};
  query.num_items = 1;
  query.items_ptr = reinterpret_cast<quintptr>(&item);
  if (::ioctl(fd, DRM_IOCTL_I915_QUERY, &query) == 0 && item.length > 0 &&
      item.length < 1024 * 1024) {
    QByteArray data(item.length, 0);
    item.data_ptr = reinterpret_cast<quintptr>(data.data());
    if (::ioctl(fd, DRM_IOCTL_I915_QUERY, &query) == 0) {
      auto topology = parseIntelTopology(data);
      for (auto it = topology.begin(); it != topology.end(); ++it)
        out[it.key()] = it.value();
    }
  }
  for (auto pair : QList<QPair<QString, int>>{{"executionUnits", I915_PARAM_EU_TOTAL},
                                              {"subsliceCount", I915_PARAM_SUBSLICE_TOTAL}})
    if (!out.contains(pair.first)) {
      int value = 0;
      drm_i915_getparam param{};
      param.param = pair.second;
      param.value = &value;
      if (::ioctl(fd, DRM_IOCTL_I915_GETPARAM, &param) == 0 && value > 0 && value < 65536)
        out[pair.first] = value;
    }
  if (out.contains("executionUnits"))
    out["intelProbeStatus"] = "Topologia consultada no driver i915; EUs não equivalem a CUDA cores";
  return out;
}
