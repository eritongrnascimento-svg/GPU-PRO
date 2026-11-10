#pragma once
#include "model.h"
QJsonObject parseAmdOverdrive(const QString &);
QJsonObject sysfsClockCaps(const Gpu &);
bool setSysfsClock(const Gpu &, const QString &domain, int mhz, QString &error);
// NVML is optional at runtime; a missing vendor library must not stop other
// GPUs.
class Nvml {
  QLibrary lib;

public:
  Nvml();
  ~Nvml();
  bool ready = false;
  template <class F> F fn(const char *name) { return reinterpret_cast<F>(lib.resolve(name)); }
  void *handle(const QString &pci);
  QJsonObject metrics(const Gpu &);
  QJsonObject specs(void *h);
  QString diagnostic;
  QJsonObject offsets(void *h);
  bool setOffset(const QString &pci, const QString &domain, int mhz, QString &error);
};
class GpuBackend {
  Nvml nv;
  QHash<QString, quint64> lastEngines;
  QElapsedTimer engineTime;
  QString lastEngineGpu;
  void drmUsage(const Gpu &, QJsonObject &);

public:
  QList<Gpu> enumerate();
  QJsonObject sample(const Gpu &);
};
