#include "model.h"
#include <algorithm>
#include <cmath>
const QList<Metric> &metricDefinitions() {
  static const QList<Metric> d = {{"usage", "Uso GPU", "%", 1},
                                  {"temp", "Temperatura", "°C", 1},
                                  {"hotspot", "Hotspot", "°C", 1},
                                  {"memoryTemp", "Temperatura VRAM", "°C", 1},
                                  {"vram", "VRAM usada", "MiB", 1048576},
                                  {"vramTotal", "VRAM total", "MiB", 1048576},
                                  {"power", "Consumo", "W", 1},
                                  {"clock", "Clock GPU", "MHz", 1},
                                  {"memClock", "Clock memória", "MHz", 1},
                                  {"fan", "Ventoinha", "%", 1},
                                  {"fanRpm", "Ventoinha RPM", "RPM", 1},
                                  {"encoder", "Encoder", "%", 1},
                                  {"decoder", "Decoder", "%", 1}};
  return d;
}
QString displayMetric(const QJsonObject &obj, const QString &key) {
  for (const auto &m : metricDefinitions())
    if (m.key == key) {
      if (!obj.value(key).isDouble())
        return "N/D";
      return QString::number(obj.value(key).toDouble() / m.divisor, 'f', key == "power" ? 1 : 0) +
             " " + m.unit;
    }
  return "N/D";
}
QJsonObject OverlayConfig::json() const {
  return {{"enabled", enabled},
          {"onlyGame", onlyGame},
          {"monitor", monitor},
          {"x", x},
          {"y", y},
          {"font", font},
          {"width", width},
          {"opacity", opacity},
          {"anchor", anchor},
          {"layout", layout},
          {"fields", QJsonArray::fromStringList(fields)}};
}
OverlayConfig OverlayConfig::fromJson(const QJsonObject &o) {
  OverlayConfig c;
  c.enabled = o["enabled"].toBool();
  c.onlyGame = o["onlyGame"].toBool();
  c.monitor = qBound(0, o["monitor"].toInt(), 31);
  c.x = qBound(-16384, o["x"].toInt(24), 16384);
  c.y = qBound(-16384, o["y"].toInt(24), 16384);
  c.font = qBound(9, o["font"].toInt(14), 40);
  c.width = qBound(220, o["width"].toInt(360), 1600);
  c.opacity = qBound(0, o["opacity"].toInt(78), 100);
  const QString a = o["anchor"].toString();
  if (QStringList{"top-left", "top-right", "bottom-left", "bottom-right", "top-center", "center",
                  "custom"}
          .contains(a))
    c.anchor = a;
  const QString l = o["layout"].toString();
  if (QStringList{"list", "compact", "columns"}.contains(l))
    c.layout = l;
  if (o.contains("fields")) {
    c.fields.clear();
    for (auto v : o["fields"].toArray())
      for (const auto &m : metricDefinitions())
        if (v.toString() == m.key && !c.fields.contains(m.key))
          c.fields << m.key;
  }
  return c;
}
BenchSummary summarize(QList<double> v) {
  v.erase(std::remove_if(v.begin(), v.end(), [](double n) { return !std::isfinite(n) || n <= 0; }),
          v.end());
  BenchSummary s;
  if (v.isEmpty())
    return s;
  double total = 0;
  for (double n : v)
    total += n;
  s.frames = v.size();
  s.mean = 1000.0 * v.size() / total;
  std::sort(v.begin(), v.end());
  s.p99 = v[qMin(v.size() - 1, qsizetype(std::ceil(v.size() * .99) - 1))];
  int count = qMax(1, int(std::ceil(v.size() * .01)));
  double worst = 0;
  for (int i = 0; i < count; ++i)
    worst += v[v.size() - 1 - i];
  s.low = 1000.0 * count / worst;
  // Provisional v0.1: 60 FPS -> 1000 points. Not calibrated to other benchmark
  // products.
  s.score = 1000 * s.mean / 60;
  return s;
}
bool saveJson(const QString &p, const QJsonObject &o, QString *error) {
  QDir().mkpath(QFileInfo(p).absolutePath());
  QSaveFile f(p);
  if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(o).toJson()) < 0 || !f.commit()) {
    if (error)
      *error = f.errorString();
    return false;
  }
  return true;
}
QString configRoot() { return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); }
int overlayColumns(const OverlayConfig &c, int width) {
  return c.layout == "columns" ? 2 : c.layout == "compact" ? qMax(1, (width - 20) / 170) : 1;
}
QRect overlayRect(const OverlayConfig &c, const QRect &r) {
  int width = qMin(c.width, r.width()), columns = overlayColumns(c, width);
  int height = qMin(r.height(),
                    2 * c.font + 38 + ((c.fields.size() + columns - 1) / columns) * (c.font + 12));
  int x = r.x() + c.x, y = r.y() + c.y;
  if (c.anchor.contains("right"))
    x = r.x() + r.width() - width - c.x;
  if (c.anchor.contains("bottom"))
    y = r.y() + r.height() - height - c.y;
  if (c.anchor == "top-center")
    x = r.x() + (r.width() - width) / 2 + c.x;
  if (c.anchor == "center") {
    x = r.x() + (r.width() - width) / 2 + c.x;
    y = r.y() + (r.height() - height) / 2 + c.y;
  }
  x = qBound(r.x(), x, r.x() + r.width() - width);
  y = qBound(r.y(), y, r.y() + r.height() - height);
  return {x, y, width, height};
}
QString readText(const QString &p) {
  QFile f(p);
  return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
}
std::optional<double> readNumber(const QString &p) {
  bool ok = false;
  double v = readText(p).toDouble(&ok);
  return ok ? std::optional<double>(v) : std::nullopt;
}
