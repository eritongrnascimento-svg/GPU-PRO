#include "neon_card.h"
#include <cmath>
NeonCard::NeonCard(QString title, QWidget *parent) : QFrame(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setMinimumHeight(112);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  content = new QVBoxLayout(this);
  content->setContentsMargins(17, 14, 17, 15);
  heading = new QLabel(title);
  heading->setWordWrap(true);
  heading->setStyleSheet("color:#aeb4c4;font-size:12px;background:transparent;");
  content->addWidget(heading);
  animation.start();
}
void NeonCard::setReading(const QJsonObject &data, QString key, QColor base) {
  available = data[key].isDouble();
  double x = data[key].toDouble(), ratio = -1;
  if (key == "temp" || key == "cpuTemp") ratio = (x - 40) / 40;
  else if (key == "hotspot" || key == "memoryTemp") ratio = (x - 50) / 45;
  else if (key == "clock") ratio = x / 2400;
  else if (key == "memClock") ratio = x / 12000;
  else if (key == "fanRpm") ratio = x / 3500;
  else if (key == "fan" || key == "usage" || key == "cpuUsage" || key == "encoder" || key == "decoder") ratio = x / 100;
  if (!available) accent = QColor("#596174");
  else if (ratio > .85) accent = QColor("#ff496c");
  else if (ratio > .65) accent = QColor("#ffad42");
  else if (ratio > .4) accent = QColor("#e7df58");
  else accent = base;
  setToolTip(available ? "Cor indica a faixa da leitura; clocks e RPM altos não representam falha."
                      : "Sensor não exposto pelo driver; nenhuma temperatura é estimada.");
  update();
}
void NeonCard::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor("#101014"));
  p.setRenderHint(QPainter::Antialiasing);
  QRectF box = QRectF(rect()).adjusted(3, 3, -3, -3);
  p.setPen(QPen(QColor("#2b3040"), 1));
  p.setBrush(QColor("#13161e"));
  p.drawRoundedRect(box, 11, 11);
  double angle = qApp->property("gpuProNeonMotion").toBool() ? std::fmod(animation.elapsed() * .035, 360.) : 0.;
  QConicalGradient glow(box.center(), angle);
  QColor dark = accent; dark.setAlpha(22);
  QColor light = accent; light.setAlpha(available ? 230 : 90);
  glow.setColorAt(0, dark); glow.setColorAt(.16, dark);
  glow.setColorAt(.25, light); glow.setColorAt(.31, dark);
  glow.setColorAt(.66, dark); glow.setColorAt(.75, light);
  glow.setColorAt(.81, dark); glow.setColorAt(1, dark);
  p.setBrush(Qt::NoBrush);
  for (int i = 6; i >= 1; --i) {
    p.setOpacity(.07); p.setPen(QPen(QBrush(glow), i + 3));
    p.drawRoundedRect(box, 11, 11);
  }
  p.setOpacity(1); p.setPen(QPen(QBrush(glow), 1.8));
  p.drawRoundedRect(box, 11, 11);
}

void NeonCard::setCompact(bool compact) {
  content->setContentsMargins(compact ? 10 : 17, compact ? 8 : 14,
                              compact ? 10 : 17, compact ? 8 : 15);
  content->setSpacing(compact ? 4 : 6);
  heading->setStyleSheet(QString("color:#aeb4c4;font-size:%1px;background:transparent;").arg(compact ? 11 : 12));
  setMinimumHeight(compact ? 64 : 112);
}
