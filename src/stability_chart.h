#pragma once
// Gráfico de estabilidade da aba Overclock: séries de clock, memória, temperatura, potência,
// ventoinha e uso nos últimos 10 minutos, com marcas dos ajustes aplicados e um diagnóstico
// de estabilidade sob carga (variação do clock, quedas e causa provável).
#include <QJsonObject>
#include <QWidget>
#include <QVector>
#include <QColor>
#include <optional>

class StabilityChart : public QWidget {
  Q_OBJECT
public:
  struct Sample {
    qint64 ms = 0;
    std::optional<double> clock, mem, temp, power, fan, usage, powerLimit;
  };
  struct Series { QString key, label, unit; QColor color; bool visible = true; };
  struct Verdict {
    QString state, detail;
    int loadSamples = 0, drops = 0, thermal = 0, power = 0;
    double meanClock = 0, cvPercent = 0, maxTemp = 0, maxPower = 0;
  };
  explicit StabilityChart(QWidget *parent = nullptr);
  void add(const QJsonObject &snapshot);
  void mark(const QString &label);
  void clear();
  void seedDemo();
  void setSeriesVisible(const QString &key, bool on);
  QVector<Series> &series() { return lines; }
  Verdict verdict() const;
  QString csv() const;
  QSize minimumSizeHint() const override { return {420, 250}; }

signals:
  void changed();

protected:
  void paintEvent(QPaintEvent *) override;

private:
  static constexpr qint64 windowMs = 10 * 60 * 1000;
  QVector<Sample> samples;
  QVector<QPair<qint64, QString>> marks;
  QVector<Series> lines;
  std::optional<double> value(const Sample &s, const QString &key) const;
  QPair<double, double> range(const QString &key) const;
};
