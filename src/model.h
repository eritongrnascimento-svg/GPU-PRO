#pragma once
#include <QtCore>
#include <optional>
struct Gpu {
  QString pci, vendor, name, driver, deviceId, sys, render, uuid;
  int nvIndex = -1;
  QJsonObject specs;
  QString accent() const {
    return vendor == "NVIDIA"  ? "#76b900"
           : vendor == "AMD"   ? "#ed1c24"
           : vendor == "Intel" ? "#168ce8"
                               : "#999999";
  }
};
struct Metric {
  QString key, label, unit;
  double divisor;
};
const QList<Metric> &metricDefinitions();
// Leituras do sistema exibidas apenas no HUD (RAM, disco, clock da CPU), igual à versão Windows.
const QList<Metric> &systemMetricDefinitions();
const Metric *findMetric(const QString &key);
QString displayMetric(const QJsonObject &, const QString &key);
struct OverlayConfig {
  bool enabled = false, onlyGame = false, frametimeGraph = false;
  int monitor = 0, x = 24, y = 24, font = 14, width = 360, opacity = 78;
  QString frametimeLog;
  QString anchor = "top-left", layout = "list";
  // "game" = sempre no topo sobre o jogo; "desktop" = abaixo das janelas, sobre o papel de parede.
  QString mode = "game", textColor = "#ebebeb", titleColor = "auto", graphColor = "auto";
  QStringList fields = {"usage", "temp", "vram", "power", "clock", "cpuUsage", "cpuTemp", "fps", "fpsLow", "fpsAvg", "fpsMax"};
  QJsonObject json() const;
  static OverlayConfig fromJson(const QJsonObject &);
};
struct BenchSummary {
  double mean = 0, p99 = 0, low = 0, score = 0;
  int frames = 0;
};
int overlayColumns(const OverlayConfig &, int effectiveWidth);
QRect overlayRect(const OverlayConfig &, const QRect &monitor);
BenchSummary summarize(QList<double> milliseconds);
bool saveJson(const QString &, const QJsonObject &, QString *error = nullptr);
QString configRoot();
QString readText(const QString &path);
std::optional<double> readNumber(const QString &path);
// Synthetic frame-time model; percentages are not measured game FPS loss.
QJsonObject estimateBottleneck(const QJsonObject &baseline, const QJsonObject &target);
QString installedClockHelper();
bool rootHelperTrusted(const QString &path);
