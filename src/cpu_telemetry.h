#pragma once
#include "model.h"
struct CpuCounters { quint64 total = 0, idle = 0; };
std::optional<CpuCounters> parseCpuCounters(const QString &);
class CpuTelemetry {
  std::optional<CpuCounters> previous;
  QHash<int,CpuCounters> previousCores;
public:
  QJsonObject sample(const QString &procStat = "/proc/stat", const QString &sys = "/sys/class", const QString &cpuRoot = "/sys/devices/system/cpu");
};

QJsonObject cpuDetails(const QString &procStat,const QString &cpuRoot);
