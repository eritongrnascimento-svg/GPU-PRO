#include "stability_chart.h"
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace {
std::optional<double> number(const QJsonObject &o, const char *key) {
  return o[key].isDouble() && std::isfinite(o[key].toDouble()) ? std::optional<double>(o[key].toDouble()) : std::nullopt;
}
}

StabilityChart::StabilityChart(QWidget *parent) : QWidget(parent) {
  lines = {{"clock", "Clock GPU", "MHz", QColor("#76b900")},  {"mem", "Clock memória", "MHz", QColor("#3fa7ff")},
           {"temp", "Temperatura", "°C", QColor("#ff5a5f")},   {"power", "Potência", "W", QColor("#ffb020")},
           {"fan", "Ventoinha", "%", QColor("#b07cff")},       {"usage", "Uso GPU", "%", QColor("#d8d8d8")}};
  setMinimumHeight(250);
}

void StabilityChart::add(const QJsonObject &o) {
  Sample s;
  s.ms = QDateTime::currentMSecsSinceEpoch();
  s.clock = number(o, "clock");
  s.mem = number(o, "memClock");
  s.temp = number(o, "temp");
  s.power = number(o, "power");
  s.fan = number(o, "fan");
  s.usage = number(o, "usage");
  if (auto limit = number(o, "powerLimit")) s.powerLimit = *limit / 1000.0; // mW → W
  samples << s;
  while (!samples.isEmpty() && s.ms - samples.first().ms > windowMs) samples.removeFirst();
  while (!marks.isEmpty() && s.ms - marks.first().first > windowMs) marks.removeFirst();
  update();
  emit changed();
}

void StabilityChart::mark(const QString &label) {
  marks << qMakePair(QDateTime::currentMSecsSinceEpoch(), label);
  update();
  emit changed();
}

// Somente para a prévia offline (dados fictícios): 6 minutos de carga com um ajuste aplicado,
// para que a captura mostre o gráfico preenchido. Nunca usado com uma GPU real.
void StabilityChart::seedDemo() {
  samples.clear();
  marks.clear();
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  for (int i = 360; i >= 0; --i) {
    Sample s;
    s.ms = now - qint64(i) * 1000;
    const bool load = i < 330, tuned = i < 180;
    const double wobble = std::sin(i / 7.0) * (tuned ? 22 : 9);
    s.usage = load ? 97 + std::sin(i / 3.0) : 4;
    s.clock = load ? (tuned ? 1965 : 1890) + wobble - (tuned && i % 47 == 0 ? 140 : 0) : 210;
    s.mem = load ? (tuned ? 7500 : 7000) : 405;
    s.temp = load ? 60 + (330 - i) * 0.06 : 41;
    s.power = load ? (tuned ? 214 : 196) + std::sin(i / 5.0) * 6 : 18;
    s.powerLimit = 225;
    s.fan = load ? 48 + (330 - i) * 0.05 : 0;
    samples << s;
  }
  marks << qMakePair(now - 180000, QString("Ajuste aplicado · core +75 MHz · memória +500 MHz"));
  update();
  emit changed();
}

void StabilityChart::clear() {
  samples.clear();
  marks.clear();
  update();
  emit changed();
}

void StabilityChart::setSeriesVisible(const QString &key, bool on) {
  for (auto &l : lines) if (l.key == key) l.visible = on;
  update();
}

std::optional<double> StabilityChart::value(const Sample &s, const QString &key) const {
  if (key == "clock") return s.clock;
  if (key == "mem") return s.mem;
  if (key == "temp") return s.temp;
  if (key == "power") return s.power;
  if (key == "fan") return s.fan;
  return s.usage;
}

// Escala própria de cada série, para que todas caibam no mesmo gráfico sem se achatar.
QPair<double, double> StabilityChart::range(const QString &key) const {
  if (key == "temp") return {20, 100};
  if (key == "fan" || key == "usage") return {0, 100};
  double top = 0;
  for (const auto &s : samples) {
    if (auto v = value(s, key)) top = std::max(top, *v);
    if (key == "power" && s.powerLimit) top = std::max(top, *s.powerLimit);
  }
  return {0, top > 0 ? top * 1.1 : 1};
}

// Estabilidade sob carga (uso ≥ 80 %): coeficiente de variação do clock, quedas abaixo de 93 % da
// média e a causa provável de cada queda (temperatura ≥ 83 °C ou potência ≥ 97 % do limite).
StabilityChart::Verdict StabilityChart::verdict() const {
  Verdict v;
  QVector<double> clocks;
  for (const auto &s : samples) {
    if (s.temp) v.maxTemp = std::max(v.maxTemp, *s.temp);
    if (s.power) v.maxPower = std::max(v.maxPower, *s.power);
    if (s.usage && *s.usage >= 80 && s.clock) clocks << *s.clock;
  }
  v.loadSamples = clocks.size();
  if (v.loadSamples < 10) {
    v.state = "Sem carga suficiente";
    v.detail = "Rode um jogo ou o teste de Cenas 3D por pelo menos 10 s para avaliar a estabilidade.";
    return v;
  }
  double sum = 0;
  for (double c : clocks) sum += c;
  v.meanClock = sum / clocks.size();
  double var = 0;
  for (double c : clocks) var += (c - v.meanClock) * (c - v.meanClock);
  v.cvPercent = v.meanClock > 0 ? 100.0 * std::sqrt(var / clocks.size()) / v.meanClock : 0;
  for (const auto &s : samples) {
    if (!s.usage || *s.usage < 80 || !s.clock || *s.clock >= v.meanClock * 0.93) continue;
    ++v.drops;
    if (s.temp && *s.temp >= 83) ++v.thermal;
    if (s.power && s.powerLimit && *s.power >= *s.powerLimit * 0.97) ++v.power;
  }
  const QString cause = v.thermal > v.power ? "temperatura" : v.power > 0 ? "limite de potência" : "";
  if (v.cvPercent < 1.5 && v.drops == 0) {
    v.state = "Estável";
    v.detail = QString("Clock sob carga constante: média %1 MHz, variação %2 %.").arg(v.meanClock, 0, 'f', 0).arg(v.cvPercent, 0, 'f', 1);
  } else if (v.cvPercent < 4 && v.drops <= 3) {
    v.state = "Oscilação leve";
    v.detail = QString("Variação de %1 % e %2 queda(s) de clock").arg(v.cvPercent, 0, 'f', 1).arg(v.drops) +
               (cause.isEmpty() ? "." : " · causa provável: " + cause + ".");
  } else {
    v.state = "Instável ou limitado";
    v.detail = QString("Variação de %1 % e %2 queda(s) de clock sob carga").arg(v.cvPercent, 0, 'f', 1).arg(v.drops) +
               (cause.isEmpty() ? ". Reduza o ajuste ou restaure o padrão." : " · causa provável: " + cause + ". Reduza o ajuste ou melhore a refrigeração.");
  }
  return v;
}

QString StabilityChart::csv() const {
  QString out = "timestamp,clock_mhz,mem_mhz,temp_c,power_w,power_limit_w,fan_pct,usage_pct\n";
  auto f = [](const std::optional<double> &v) { return v ? QString::number(*v, 'f', 1) : QString(); };
  for (const auto &s : samples)
    out += QDateTime::fromMSecsSinceEpoch(s.ms).toString(Qt::ISODate) + "," + f(s.clock) + "," + f(s.mem) + "," + f(s.temp) + "," +
           f(s.power) + "," + f(s.powerLimit) + "," + f(s.fan) + "," + f(s.usage) + "\n";
  for (const auto &m : marks) out += "# " + QDateTime::fromMSecsSinceEpoch(m.first).toString(Qt::ISODate) + " " + m.second + "\n";
  return out;
}

void StabilityChart::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF area = QRectF(rect()).adjusted(46, 64, -12, -28);
  p.fillRect(rect(), QColor(17, 17, 21, 220));
  p.setPen(QColor(55, 55, 62));
  for (int i = 0; i <= 4; ++i) {
    double y = area.top() + area.height() * i / 4;
    p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    p.setPen(QColor(150, 150, 158));
    p.drawText(QRectF(0, y - 8, 40, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(100 - 25 * i) + "%");
    p.setPen(QColor(55, 55, 62));
  }
  const qint64 now = samples.isEmpty() ? QDateTime::currentMSecsSinceEpoch() : samples.last().ms;
  auto xFor = [&](qint64 ms) { return area.right() - area.width() * double(now - ms) / windowMs; };
  p.setPen(QColor(150, 150, 158));
  for (int m = 0; m <= 10; m += 2) {
    double x = xFor(now - qint64(m) * 60000);
    p.drawText(QRectF(x - 30, area.bottom() + 6, 60, 16), Qt::AlignCenter, m == 0 ? "agora" : QString("-%1 min").arg(m));
  }
  // Marcas de ajustes aplicados/restaurados.
  for (const auto &m : marks) {
    double x = xFor(m.first);
    if (x < area.left()) continue;
    p.setPen(QPen(QColor(255, 255, 255, 150), 1, Qt::DashLine));
    p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    p.setPen(QColor(230, 230, 235));
    const QString label = m.second.left(48);
    const double width = p.fontMetrics().horizontalAdvance(label) + 8;
    if (x + width > area.right()) p.drawText(QRectF(x - width, area.top() + 2, width - 4, 16), Qt::AlignRight, label);
    else p.drawText(QRectF(x + 4, area.top() + 2, width, 16), Qt::AlignLeft, label);
  }
  // Séries normalizadas pela escala de cada uma.
  for (const auto &l : lines) {
    if (!l.visible) continue;
    auto [lo, hi] = range(l.key);
    QPainterPath path;
    bool started = false;
    for (const auto &s : samples) {
      auto v = value(s, l.key);
      if (!v) { started = false; continue; }
      double y = area.bottom() - area.height() * std::clamp((*v - lo) / (hi - lo), 0.0, 1.0);
      QPointF pt(xFor(s.ms), y);
      if (!started) { path.moveTo(pt); started = true; } else path.lineTo(pt);
    }
    p.setPen(QPen(l.color, 2));
    p.drawPath(path);
  }
  // Legenda com o valor atual de cada série; quebra para a linha seguinte quando não cabe.
  double x = area.left(), y = 4;
  for (const auto &l : lines) {
    if (!l.visible) continue;
    QString current = "N/D";
    if (!samples.isEmpty()) if (auto v = value(samples.last(), l.key)) current = QString::number(*v, 'f', 0) + " " + l.unit;
    const QString text = "■ " + l.label + " " + current;
    const double width = p.fontMetrics().horizontalAdvance(text) + 16;
    if (x + width > area.right() && x > area.left()) { x = area.left(); y += 18; }
    p.setPen(l.color);
    p.drawText(QRectF(x, y, width, 18), Qt::AlignLeft | Qt::AlignVCenter, text);
    x += width;
  }
  if (samples.size() < 2) {
    p.setPen(QColor(170, 170, 178));
    p.drawText(area, Qt::AlignCenter, "Aguardando leituras dos sensores");
  }
}
