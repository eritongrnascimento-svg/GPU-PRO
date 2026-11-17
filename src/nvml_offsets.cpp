#include "gpu.h"
using H=void*;
QJsonObject Nvml::offsets(H h) {
  QJsonObject o;
  if (!h)
    return o;
  for (auto domain : {QString("core"), QString("mem")}) {
    QByteArray prefix = domain == "core" ? "Gpc" : "Mem";
    auto bounds =
        fn<int (*)(H, int *, int *)>(("nvmlDeviceGet" + prefix + "ClkMinMaxVfOffset").constData());
    auto get = fn<int (*)(H, int *)>(("nvmlDeviceGet" + prefix + "ClkVfOffset").constData());
    int lo = 0, hi = 0, now = 0;
    if (bounds && get && bounds(h, &lo, &hi) == 0 && get(h, &now) == 0 && lo <= hi) {
      o[domain + "Min"] = lo;
      o[domain + "Max"] = hi;
      o[domain + "Current"] = now;
      o[domain + "Mode"] = "offset";
    }
  }
  return o;
}
bool Nvml::setOffset(const QString &pci, const QString &domain, int mhz, QString &error) {
  if (domain != "core" && domain != "mem") {
    error = "Domínio inválido";
    return false;
  }
  H h = handle(pci);
  auto caps = offsets(h);
  if (!caps.contains(domain + "Min") || mhz < caps[domain + "Min"].toInt() ||
      mhz > caps[domain + "Max"].toInt()) {
    error = "Offset fora da faixa informada pelo driver";
    return false;
  }
  QByteArray symbol =
      domain == "core" ? "nvmlDeviceSetGpcClkVfOffset" : "nvmlDeviceSetMemClkVfOffset";
  auto set = fn<int (*)(H, int)>(symbol.constData());
  int rc = set ? set(h, mhz) : -1;
  if (rc) {
    error = QString("NVML retornou %1").arg(rc);
    return false;
  }
  auto current = offsets(h);
  if (!current.contains(domain + "Current") || current[domain + "Current"].toInt() != mhz) {
    error = "Driver não confirmou offset; confira o estado atual";
    return false;
  }
  return true;
}
