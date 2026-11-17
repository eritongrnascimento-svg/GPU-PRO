// Leituras do sistema para o HUD (igual à versão Windows): RAM, disco (leitura/escrita por segundo,
// tempo ativo e temperatura) e clock médio da CPU. Somente arquivos do kernel, sem privilégios.
#include "system_metrics.h"
#include "model.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QRegularExpression>

namespace {
// Discos físicos inteiros (sem partições): sdX, vdX, xvdX, nvmeXnY, mmcblkN.
bool wholeDisk(const QString &name) {
  static const QRegularExpression pattern("^(sd[a-z]+|vd[a-z]+|xvd[a-z]+|nvme[0-9]+n[0-9]+|mmcblk[0-9]+)$");
  return pattern.match(name).hasMatch();
}
struct DiskCounters { double readSectors = 0, writeSectors = 0, ioMs = 0; };
}

void addSystemMetrics(QJsonObject &current) {
  qint64 total = 0, available = 0;
  for (const auto &line : readText("/proc/meminfo").split('\n')) {
    if (line.startsWith("MemTotal:")) total = line.section(':', 1).trimmed().section(' ', 0, 0).toLongLong() * 1024;
    else if (line.startsWith("MemAvailable:")) available = line.section(':', 1).trimmed().section(' ', 0, 0).toLongLong() * 1024;
  }
  if (total > 0) {
    current["ramTotal"] = double(total);
    current["ramUsed"] = double(total - available);
    current["ramPct"] = 100.0 * double(total - available) / double(total);
  }
  double mhz = 0;
  int cores = 0;
  for (const auto &line : readText("/proc/cpuinfo").split('\n'))
    if (line.startsWith("cpu MHz")) { mhz += line.section(':', 1).trimmed().toDouble(); ++cores; }
  if (cores > 0) current["cpuClock"] = mhz / cores;

  static QHash<QString, DiskCounters> previous;
  static QElapsedTimer clock;
  QHash<QString, DiskCounters> now;
  for (const auto &line : readText("/proc/diskstats").split('\n')) {
    const auto f = line.simplified().split(' ');
    if (f.size() < 14 || !wholeDisk(f[2])) continue;
    now[f[2]] = {f[5].toDouble(), f[9].toDouble(), f[12].toDouble()};
  }
  if (clock.isValid() && clock.elapsed() > 0 && !previous.isEmpty()) {
    const double seconds = clock.elapsed() / 1000.0;
    double read = 0, write = 0, active = 0;
    for (auto it = now.constBegin(); it != now.constEnd(); ++it) {
      if (!previous.contains(it.key())) continue;
      const auto &p = previous[it.key()];
      read += qMax(0.0, it->readSectors - p.readSectors) * 512.0 / seconds;
      write += qMax(0.0, it->writeSectors - p.writeSectors) * 512.0 / seconds;
      active = qMax(active, qBound(0.0, (it->ioMs - p.ioMs) / (seconds * 10.0), 100.0));
    }
    current["diskRead"] = read;
    current["diskWrite"] = write;
    current["diskActive"] = active;
  }
  previous = now;
  clock.restart();

  // Temperatura: hwmon de NVMe ("nvme") e SATA/SAS com o módulo drivetemp; mostra a maior.
  double temperature = -1000;
  for (const auto &hwmon : QDir("/sys/class/hwmon").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    const QString base = "/sys/class/hwmon/" + hwmon;
    const QString name = readText(base + "/name").trimmed();
    if (name != "nvme" && name != "drivetemp") continue;
    if (auto milli = readNumber(base + "/temp1_input")) temperature = qMax(temperature, *milli / 1000.0);
  }
  if (temperature > -40) current["diskTemp"] = temperature;
}
