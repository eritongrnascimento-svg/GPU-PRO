#include "gpu.h"
#include <cmath>
#include <sys/stat.h>
#include <unistd.h>
using H = void *;
using U = unsigned;
Nvml::Nvml() : lib("libnvidia-ml.so.1") {
  if (lib.load()) {
    auto init = fn<int (*)()>("nvmlInit_v2");
    int rc = init ? init() : -1;
    ready = rc == 0;
    if (!ready) {
      auto error = fn<const char *(*)(int)>("nvmlErrorString");
      diagnostic = error ? QString::fromLatin1(error(rc)) : "NVML sem inicialização";
    }

  } else
    diagnostic = lib.errorString();
}
Nvml::~Nvml() {
  if (ready)
    if (auto f = fn<int (*)()>("nvmlShutdown"))
      f();
}
void *Nvml::handle(const QString &pci) {
  H h = nullptr;
  auto f = fn<int (*)(const char *, H *)>("nvmlDeviceGetHandleByPciBusId_v2");
  if (ready && f && f(pci.toLatin1().constData(), &h) == 0)
    return h;
  return nullptr;
}
QJsonObject Nvml::specs(H h) {
  QJsonObject o;
  if (!h)
    return o;
  for (auto pair : QList<QPair<QString, const char *>>{{"name", "nvmlDeviceGetName"},
                                                       {"uuid", "nvmlDeviceGetUUID"},
                                                       {"vbios", "nvmlDeviceGetVbiosVersion"}}) {
    char b[256] = {};
    auto f = fn<int (*)(H, char *, U)>(pair.second);
    if (f && f(h, b, sizeof b) == 0)
      o[pair.first] = QString::fromLatin1(b);
  }
  char driver[128] = {};
  if (auto f = fn<int (*)(char *, U)>("nvmlSystemGetDriverVersion"))
    if (f(driver, sizeof driver) == 0)
      o["driverVersion"] = driver;
  for (auto pair : QList<QPair<QString, const char *>>{
           {"powerLimit", "nvmlDeviceGetPowerManagementLimit"},
           {"powerDefault", "nvmlDeviceGetPowerManagementDefaultLimit"},
           {"pcieGeneration", "nvmlDeviceGetCurrPcieLinkGeneration"},
           {"pcieWidth", "nvmlDeviceGetCurrPcieLinkWidth"},
           {"nvIndex", "nvmlDeviceGetIndex"},
           {"memoryBusWidthBits", "nvmlDeviceGetMemoryBusWidth"},
           {"gpuCores", "nvmlDeviceGetNumGpuCores"},
           {"architectureId", "nvmlDeviceGetArchitecture"},
           {"pcieMaxGeneration", "nvmlDeviceGetMaxPcieLinkGeneration"},
           {"pcieMaxWidth", "nvmlDeviceGetMaxPcieLinkWidth"}}) {
    U v = 0;
    auto f = fn<int (*)(H, U *)>(pair.second);
    if (f && f(h, &v) == 0)
      o[pair.first] = double(v);
  }
  U lo = 0, hi = 0;
  auto f = fn<int (*)(H, U *, U *)>("nvmlDeviceGetPowerManagementLimitConstraints");
  if (f && f(h, &lo, &hi) == 0) {
    o["powerMin"] = double(lo);
    o["powerMax"] = double(hi);
  }
  struct M {
    unsigned long long total, free, used;
  } memory{};
  if (auto f = fn<int (*)(H, M *)>("nvmlDeviceGetMemoryInfo"))
    if (f(h, &memory) == 0)
      o["memoryBytes"] = double(memory.total);
  if (auto f = fn<int (*)(H, U, U *)>("nvmlDeviceGetMaxClockInfo"))
    for (auto d : QList<QPair<QString, U>>{{"maxGraphicsMHz", 0}, {"maxMemoryMHz", 2}}) {
      U value = 0;
      if (f(h, d.second, &value) == 0)
        o[d.first] = int(value);
    }
  const QMap<int, QString> architectures = {{2, "Kepler"},       {3, "Maxwell"}, {4, "Pascal"},
                                            {5, "Volta"},        {6, "Turing"},  {7, "Ampere"},
                                            {8, "Ada Lovelace"}, {9, "Hopper"},  {10, "Blackwell"}};
  if (architectures.contains(o["architectureId"].toInt()))
    o["architecture"] = architectures[o["architectureId"].toInt()];
  return o;
}
QJsonObject Nvml::metrics(const Gpu &g) {
  H h = handle(g.pci);
  QJsonObject o;
  if (!h) {
    o["error"] = "NVML indisponível: " + diagnostic;
    return o;
  }
  struct Memory {
    unsigned long long total, free, used;
  } m{};
  if (auto f = fn<int (*)(H, Memory *)>("nvmlDeviceGetMemoryInfo"))
    if (f(h, &m) == 0) {
      o["vram"] = double(m.used);
      o["vramTotal"] = double(m.total);
    }
  struct Util {
    U gpu, memory;
  } u{};
  if (auto f = fn<int (*)(H, Util *)>("nvmlDeviceGetUtilizationRates"))
    if (f(h, &u) == 0)
      o["usage"] = int(u.gpu);
  if (auto f = fn<int (*)(H, U, U *)>("nvmlDeviceGetTemperature")) {
    U t;
    if (f(h, 0, &t) == 0)
      o["temp"] = int(t);
  }
  for (auto pair : QList<QPair<QString, const char *>>{{"power", "nvmlDeviceGetPowerUsage"},
                                                       {"fan", "nvmlDeviceGetFanSpeed"}}) {
    U v;
    auto f = fn<int (*)(H, U *)>(pair.second);
    if (f && f(h, &v) == 0)
      o[pair.first] = pair.first == "power" ? v / 1000.0 : double(v);
  }
  if (auto f = fn<int (*)(H, U, U *)>("nvmlDeviceGetClockInfo"))
    for (auto pair : QList<QPair<QString, U>>{{"clock", 0}, {"memClock", 2}}) {
      U v;
      if (f(h, pair.second, &v) == 0)
        o[pair.first] = int(v);
    }
  for (auto pair :
       QList<QPair<QString, const char *>>{{"encoder", "nvmlDeviceGetEncoderUtilization"},
                                           {"decoder", "nvmlDeviceGetDecoderUtilization"}}) {
    U v, period;
    auto f = fn<int (*)(H, U *, U *)>(pair.second);
    if (f && f(h, &v, &period) == 0)
      o[pair.first] = int(v);
  }
  struct Temperature {
    U version;
    int type, temperature;
  } temperature{U(sizeof(Temperature)) | (1u << 24), 0, 0};
  if (auto f = fn<int (*)(H, Temperature *)>("nvmlDeviceGetTemperatureV"))
    if (f(h, &temperature) == 0)
      o["temp"] = temperature.temperature;
  // Public NVML field API: memory temperature is distinct from GPU hotspot.
  union FieldValue { double d; unsigned int ui; unsigned long ul; unsigned long long ull; long long sll; int si; unsigned short us; };
  struct Field { U id, scope; long long timestamp, latency; int type, result; FieldValue value; };
  if (auto get = fn<int (*)(H, int, Field *)>("nvmlDeviceGetFieldValues")) {
    Field memory{};
    memory.id = 82; // NVML_FI_DEV_MEMORY_TEMP
    if (get(h, 1, &memory) == 0 && memory.result == 0) {
      double t = memory.type == 0 ? memory.value.d : memory.type == 1 ? double(memory.value.ui)
          : memory.type == 2 ? double(memory.value.ul) : memory.type == 3 ? double(memory.value.ull)
          : memory.type == 4 ? double(memory.value.sll) : -999;
      if (std::isfinite(t) && t >= -40 && t <= 150) {
        o["memoryTemp"] = t;
        o["memoryTempSource"] = "NVML field 82";
      }
    }
    o["memoryTempStatus"] = o.contains("memoryTemp") ? "Disponível via NVML" : "Driver não disponibilizou NVML field 82";
  }
  o["hotspotStatus"] = "Não exposto pela NVML pública desta GPU/driver; procurando hwmon";
  U fans = 1;
  if (auto n = fn<int (*)(H, U *)>("nvmlDeviceGetNumFans"))
    if (n(h, &fans) != 0)
      fans = 1;
  QJsonArray list;
  for (U i = 0; i < qMin(fans, 16u); ++i) {
    QJsonObject fan{{"index", int(i)}, {"source", "NVML"}};
    if (auto percent = fn<int (*)(H, U, U *)>("nvmlDeviceGetFanSpeed_v2")) {
      U value = 0;
      if (percent(h, i, &value) == 0)
        fan["percent"] = int(value);
    }
    struct Rpm {
      U version, index, speed;
    } rpm{U(sizeof(Rpm)) | (1u << 24), i, 0};
    if (auto get = fn<int (*)(H, Rpm *)>("nvmlDeviceGetFanSpeedRPM"))
      if (get(h, &rpm) == 0) {
        fan["rpm"] = int(rpm.speed);
        fan["rpmMeaning"] = "Velocidade pretendida reportada pelo driver";
        if (i == 0)
          o["fanRpm"] = int(rpm.speed);
      }
    if (fan.contains("rpm") || fan.contains("percent"))
      list.append(fan);
  }
  o["fans"] = list;
  if (auto f = fn<int (*)(H, U, U *)>("nvmlDeviceGetTemperatureThreshold")) {
    U limit = 0;
    if (f(h, 1, &limit) == 0)
      o["slowdownTemp"] = int(limit);
  }
  return o;
}
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
QList<Gpu> GpuBackend::enumerate() {
  QList<Gpu> out;
  QDir drm("/sys/class/drm");
  for (const auto &card : drm.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    if (!QRegularExpression("^card[0-9]+$").match(card).hasMatch())
      continue;
    QString sys = QFileInfo(drm.filePath(card + "/device")).canonicalFilePath();
    if (sys.isEmpty())
      continue;
    Gpu g;
    g.sys = sys;
    g.pci = QFileInfo(sys).fileName();
    g.deviceId = readText(sys + "/device");
    QString vendor = readText(sys + "/vendor");
    g.vendor = vendor == "0x10de"   ? "NVIDIA"
               : vendor == "0x1002" ? "AMD"
               : vendor == "0x8086" ? "Intel"
                                    : "Outro";
    g.driver = QFileInfo(QFileInfo(sys + "/driver").canonicalFilePath()).fileName();
    g.name = g.vendor + " " + g.deviceId;
    QDir nodes(sys + "/drm");
    for (const auto &node : nodes.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
      if (node.startsWith("renderD"))
        g.render = "/dev/dri/" + node;
    g.specs = {{"pci", g.pci},
               {"vendorId", vendor},
               {"deviceId", g.deviceId},
               {"subsystemVendor", readText(sys + "/subsystem_vendor")},
               {"subsystemDevice", readText(sys + "/subsystem_device")},
               {"driver", g.driver},
               {"renderNode", g.render},
               {"numa", readText(sys + "/numa_node")},
               {"pcieSpeed", readText(sys + "/current_link_speed")},
               {"pcieMaxSpeed", readText(sys + "/max_link_speed")},
               {"pcieWidth", readText(sys + "/current_link_width")},
               {"pcieMaxWidth", readText(sys + "/max_link_width")},
               {"bootVga", readText(sys + "/boot_vga")}};
    if (g.vendor == "NVIDIA") {
      auto n = nv.specs(nv.handle(g.pci));
      for (auto it = n.begin(); it != n.end(); ++it)
        g.specs[it.key()] = it.value();
      g.name = n["name"].toString(g.name);
      g.nvIndex = n["nvIndex"].toInt(-1);
      g.uuid = n["uuid"].toString();
    }
    // pci.ids, when installed, provides marketing names without invoking a
    // shell.
    if (!g.specs.contains("name")) {
      QString db;
      for (const auto &p : {"/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids"})
        if (QFileInfo::exists(p)) {
          db = readText(p);
          break;
        }
      bool found = false;
      QString vid = vendor.mid(2), did = g.deviceId.mid(2);
      for (const auto &line : db.split('\n')) {
        if (!line.startsWith('\t') && !line.startsWith('#') && !line.isEmpty())
          found = line.startsWith(vid + " ");
        if (found && line.startsWith("\t" + did + " ")) {
          g.name = g.vendor + " " + line.mid(6).trimmed();
          break;
        }
      }
    }
    out << g;
  }
  return out;
}
QJsonObject GpuBackend::sample(const Gpu &g) {
  QJsonObject o = g.vendor == "NVIDIA" ? nv.metrics(g) : QJsonObject{};
  auto read = [&](QString key, QString p, double scale = 1) {
    if (auto n = readNumber(p))
      o[key] = *n / scale;
  };
  read("usage", g.sys + "/gpu_busy_percent");
  read("vram", g.sys + "/mem_info_vram_used");
  read("vramTotal", g.sys + "/mem_info_vram_total");
  for (const auto &h : QDir(g.sys + "/hwmon").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    QString base = g.sys + "/hwmon/" + h + "/";
    QJsonArray sensors = o["temperatureSensors"].toArray();
    for (int i = 1; i <= 16; ++i) {
      QString label = readText(base + QString("temp%1_label").arg(i));
      auto value = readNumber(base + QString("temp%1_input").arg(i));
      if (!value)
        continue;
      sensors.append(QJsonObject{{"channel", i},
                                 {"label", label.isEmpty() ? QString("Sensor %1").arg(i) : label},
                                 {"celsius", *value / 1000.},
                                 {"source", "hwmon"}});
      QString lower = label.toLower();
      if (lower.contains("mem")) {
        o["memoryTemp"] = *value / 1000.;
        o["memoryTempSource"] = "hwmon · " + label;
      } else if (lower.contains("junction") || lower.contains("hotspot") || lower.contains("hot spot")) {
        o["hotspot"] = *value / 1000.;
        o["hotspotSource"] = "hwmon · " + label;
        o["hotspotStatus"] = "Disponível via hwmon";
      }
      else if (lower == "edge" || lower.contains("gpu") || (!o.contains("temp") && i == 1))
        o["temp"] = *value / 1000.;
    }
    o["temperatureSensors"] = sensors;
    QJsonArray fans = o["fans"].toArray();
    for (int i = 1; i <= 16; ++i) {
      auto rpm = readNumber(base + QString("fan%1_input").arg(i));
      if (!rpm)
        continue;
      fans.append(QJsonObject{{"index", i - 1},
                              {"rpm", *rpm},
                              {"source", "hwmon"},
                              {"rpmMeaning", "Tacômetro exposto pelo driver"}});
      if (i == 1)
        o["fanRpm"] = *rpm;
    }
    o["fans"] = fans;
    read("power", base + "power1_average", 1e6);
    if (!o.contains("power"))
      read("power", base + "power1_input", 1e6);
    read("clock", base + "freq1_input", 1e6);
    read("memClock", base + "freq2_input", 1e6);
    read("powerLimit", base + "power1_cap", 1000);
    read("powerMin", base + "power1_cap_min", 1000);
    read("powerMax", base + "power1_cap_max", 1000);
  }
  // i915 and xe expose different frequency paths. No fabricated utilization.
  for (const auto &p :
       {"/gt_cur_freq_mhz", "/gt/gt0/rps_cur_freq_mhz", "/tile0/gt0/freq0/cur_freq"})
    if (!o.contains("clock"))
      read("clock", g.sys + p);
  for (const auto &card : QDir(g.sys + "/drm").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    if (card.startsWith("card"))
      for (const auto &p : {"/gt_cur_freq_mhz", "/gt/gt0/rps_cur_freq_mhz"})
        if (!o.contains("clock"))
          read("clock", g.sys + "/drm/" + card + p);
  if (g.vendor == "Intel")
    drmUsage(g, o);
  if (g.vendor == "AMD" || g.vendor == "Intel") {
    auto c = sysfsClockCaps(g);
    for (auto it = c.begin(); it != c.end(); ++it)
      o[it.key()] = it.value();
  }
  if (g.vendor == "NVIDIA") {
    auto h = nv.handle(g.pci);
    auto s = nv.specs(h);
    for (auto key : {"powerLimit", "powerMin", "powerMax"})
      if (s.contains(key))
        o[key] = s[key];
    auto c = nv.offsets(h);
    for (auto it = c.begin(); it != c.end(); ++it)
      o[it.key()] = it.value();
  }
  o["pci"] = g.pci;
  o["name"] = g.name;
  o["accent"] = g.accent();
  o["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  return o;
}
void GpuBackend::drmUsage(const Gpu &g, QJsonObject &o) {
  // Optional DRM fdinfo accounting: accessible clients only, deduplicated by
  // client id.
  if (lastEngineGpu != g.pci) {
    lastEngines.clear();
    engineTime.invalidate();
    lastEngineGpu = g.pci;
  }
  QHash<QString, quint64> next;
  QMap<QString, double> deltas;
  QMap<QString, int> capacities;
  QSet<QString> seen;
  for (auto pid : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    bool numeric = false;
    pid.toInt(&numeric);
    if (!numeric)
      continue;
    QString root = "/proc/" + pid;
    struct stat st{};
    if (stat(QFile::encodeName(root).constData(), &st) || st.st_uid != getuid())
      continue;
    for (auto fd : QDir(root + "/fdinfo").entryList(QDir::Files)) {
      QString content = readText(root + "/fdinfo/" + fd);
      if (!content.contains("drm-client-id:"))
        continue;
      QHash<QString, QString> fields;
      for (auto line : content.split('\n')) {
        int colon = line.indexOf(':');
        if (colon > 0)
          fields[line.left(colon).trimmed()] = line.mid(colon + 1).trimmed();
      }
      if (fields["drm-pdev"].toLower() != g.pci.toLower())
        continue;
      QString client = fields["drm-client-id"];
      if (seen.contains(client))
        continue;
      seen.insert(client);
      for (auto it = fields.begin(); it != fields.end(); ++it) {
        if (!it.key().startsWith("drm-engine-") || it.key().startsWith("drm-engine-capacity-"))
          continue;
        QString engine = it.key().mid(11), key = client + "/" + engine;
        bool ok = false;
        quint64 ns = it.value().section(' ', 0, 0).toULongLong(&ok);
        if (!ok || !it.value().endsWith(" ns"))
          continue;
        next[key] = ns;
        int capacity = qMax(1, fields["drm-engine-capacity-" + engine].toInt());
        capacities[engine] = qMax(capacities.value(engine, 1), capacity);
        if (lastEngines.contains(key) && ns >= lastEngines[key])
          deltas[engine] += double(ns - lastEngines[key]);
      }
    }
  }
  if (engineTime.isValid() && !deltas.isEmpty()) {
    double elapsed = engineTime.nsecsElapsed(), usage = 0;
    for (auto it = deltas.begin(); it != deltas.end(); ++it)
      usage = qMax(usage, 100. * it.value() / qMax(1., elapsed) / capacities.value(it.key(), 1));
    o["usage"] = qBound(0., usage, 100.);
    o["usageSource"] = "DRM fdinfo: maior ocupação por família de engines; "
                       "apenas clientes acessíveis do usuário";
  }
  lastEngines = next;
  engineTime.restart();
}
QJsonObject parseAmdOverdrive(const QString &text) {
  QJsonObject out;
  QString section;
  QMap<QString, QMap<int, int>> states;
  QSet<QString> voltage;
  QMap<QString, QPair<int, int>> ranges;
  QRegularExpression state("^([01]):\\s*(\\d+)\\s*Mhz\\s*$",
                           QRegularExpression::CaseInsensitiveOption);
  QRegularExpression range("^(?:OD_)?(SCLK|MCLK):\\s*(\\d+)\\s*Mhz\\s+(\\d+)\\s*Mhz\\s*$",
                           QRegularExpression::CaseInsensitiveOption);
  for (QString line : text.split('\n')) {
    line = line.trimmed();
    if (line.startsWith("OD_") && line.endsWith(':')) {
      section = line;
      continue;
    }
    if (section == "OD_SCLK:" || section == "OD_MCLK:") {
      if (line.contains("mV", Qt::CaseInsensitive))
        voltage.insert(section);
      auto m = state.match(line);
      if (m.hasMatch())
        states[section][m.captured(1).toInt()] = m.captured(2).toInt();
    }
    if (section == "OD_RANGE:") {
      auto m = range.match(line);
      if (m.hasMatch())
        ranges[m.captured(1).toUpper()] = {m.captured(2).toInt(), m.captured(3).toInt()};
    }
  }
  for (QString domain : {"core", "mem"}) {
    QString s = domain == "core" ? "OD_SCLK:" : "OD_MCLK:", r = domain == "core" ? "SCLK" : "MCLK";
    if (voltage.contains(s) || !states[s].contains(1) || !ranges.contains(r))
      continue;
    int lo = qMax(ranges[r].first, states[s].value(0, ranges[r].first)), hi = ranges[r].second,
        now = states[s][1];
    if (lo <= now && now <= hi) {
      out[domain + "Min"] = lo;
      out[domain + "Max"] = hi;
      out[domain + "Current"] = now;
      out[domain + "Mode"] = "max-frequency";
    }
  }
  return out;
}
QJsonObject sysfsClockCaps(const Gpu &g) {
  if (g.vendor == "AMD")
    return parseAmdOverdrive(readText(g.sys + "/pp_od_clk_voltage"));
  QJsonObject out;
  if (g.vendor != "Intel")
    return out;
  QStringList roots;
  for (auto card : QDir(g.sys + "/drm").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    if (card.startsWith("card"))
      roots << g.sys + "/drm/" + card;
  for (auto root : roots) {
    auto lo = readNumber(root + "/gt_RPn_freq_mhz"), hi = readNumber(root + "/gt_RP0_freq_mhz"),
         now = readNumber(root + "/gt_max_freq_mhz");
    if (lo && hi && now && *lo <= *now && *now <= *hi) {
      out = {{"coreMin", *lo},
             {"coreMax", *hi},
             {"coreCurrent", *now},
             {"coreMode", "max-frequency"},
             {"coreControlPath", root + "/gt_max_freq_mhz"}};
      break;
    }
  }
  return out;
}
bool setSysfsClock(const Gpu &g, const QString &domain, int mhz, QString &error) {
  if (domain != "core" && domain != "mem") {
    error = "Domínio inválido";
    return false;
  }
  auto c = sysfsClockCaps(g);
  if (!c.contains(domain + "Min") || mhz < c[domain + "Min"].toInt() ||
      mhz > c[domain + "Max"].toInt()) {
    error = "Frequência fora da faixa consultada no driver";
    return false;
  }
  auto write = [&](QString path, QString value) {
    QFile f(path);
    QByteArray b = (value + "\n").toLatin1();
    if (!f.open(QIODevice::WriteOnly) || f.write(b) != b.size() || !f.flush()) {
      error = f.errorString();
      return false;
    }
    return true;
  };
  if (g.vendor == "Intel") {
    if (domain != "core" || !write(c["coreControlPath"].toString(), QString::number(mhz)))
      return false;
    auto check = sysfsClockCaps(g);
    if (check["coreCurrent"].toInt() != mhz) {
      error = "Driver não confirmou limite de frequência";
      return false;
    }
    return true;
  }
  if (g.vendor != "AMD") {
    error = "Fabricante não suportado";
    return false;
  }
  QString perf = g.sys + "/power_dpm_force_performance_level", od = g.sys + "/pp_od_clk_voltage",
          previous = readText(perf);
  if (!QStringList{"auto", "manual", "low", "high", "profile_standard", "profile_min_sclk",
                   "profile_min_mclk", "profile_peak"}
           .contains(previous)) {
    error = "Perfil de energia atual não reconhecido";
    return false;
  }
  if (!write(perf, "manual"))
    return false;
  QString prefix = domain == "core" ? "s 1 " : "m 1 ";
  int old = c[domain + "Current"].toInt();
  bool ok = write(od, prefix + QString::number(mhz)) && write(od, "c");
  auto after = sysfsClockCaps(g);
  ok = ok && after.contains(domain + "Current") && after[domain + "Current"].toInt() == mhz;
  if (!ok) {
    QString original = error;
    bool restored = write(od, prefix + QString::number(old)) && write(od, "c");
    auto check = sysfsClockCaps(g);
    restored =
        restored && check.contains(domain + "Current") && check[domain + "Current"].toInt() == old;
    bool profile = write(perf, previous);
    error = "Ajuste recusado: " + original +
            QString("; frequência anterior %1; perfil anterior %2")
                .arg(restored ? "restaurada" : "NÃO confirmado",
                     profile ? "restaurado" : "NÃO confirmado");
    return false;
  }
  if (!write(perf, previous)) {
    error = "Frequência aplicada, mas o perfil de energia anterior não foi "
            "restaurado: " +
            error;
    return false;
  }
  auto final = sysfsClockCaps(g);
  if (!final.contains(domain + "Current") || final[domain + "Current"].toInt() != mhz) {
    error = "Perfil restaurado, mas frequência final não confirmada";
    return false;
  }
  return true;
}
