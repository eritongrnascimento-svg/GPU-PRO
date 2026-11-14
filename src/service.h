#pragma once
#include "gpu.h"
#include "cpu_telemetry.h"
#include "frame_log.h"
#include <QtNetwork>
Q_DECLARE_METATYPE(Gpu)
Q_DECLARE_METATYPE(QList<Gpu>)
class TelemetryService : public QObject {
  Q_OBJECT
  std::unique_ptr<GpuBackend> backend;
  QList<Gpu> gpus;
  int selected = 0, pid = 0, port = 0;
  QString token;
  OverlayConfig config;
  QUdpSocket *udp = nullptr;
  QTimer *timer = nullptr;
  QJsonObject current;
  CpuTelemetry cpu;
  FrameLog frameLog;
  QJsonArray frameTimes;
  QElapsedTimer lastFrame;
  int tick = 0;
  quint64 lastCpu = 0;
  QElapsedTimer cpuTime;
public slots:
  void start();
  void refresh() {
    if (!backend)
      return;
    gpus = backend->enumerate();
    selected = 0;
    current = {};
    tick = 0;
    emit inventory(gpus);
  }
  void stop();
  void choose(int index) {
    selected = index;
    current = {};
    tick = 0;
  }
  void overlay(QJsonObject c) {
    auto next=OverlayConfig::fromJson(c);
    if(next.frametimeLog!=config.frametimeLog) {frameTimes={};lastFrame.invalidate();frameLog.reset();}
    config=next;
    publish();
  }
  void endpoint(int p, QString t) {
    port = p;
    token = t;
  }
  void process(int p) {
    if(pid!=p) {frameTimes={};lastFrame.invalidate();frameLog.reset();}
    pid = p;
    lastCpu = 0;
    cpuTime.invalidate();
    tick = 0;
  }
signals:
  void inventory(QList<Gpu>);
  void snapshot(QJsonObject);

private:
  void publish();
};
int overlayChild(int argc, char **argv);
bool renderOverlayPreview(const QString &path, const QString &layout);
