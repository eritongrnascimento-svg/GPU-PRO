#include "service.h"
#include <unistd.h>
void TelemetryService::start() {
  backend = std::make_unique<GpuBackend>();
  gpus = backend->enumerate();
  udp = new QUdpSocket(this);
  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &TelemetryService::publish);
  emit inventory(gpus);
  timer->start(250);
  publish();
}
void TelemetryService::stop() {
  if (timer)
    timer->stop();
  backend.reset();
}
void TelemetryService::publish() {
  if(!backend)return;
  if (tick++ % 4 == 0) {
    current = selected >= 0 && selected < gpus.size()
                  ? backend->sample(gpus[selected])
                  : QJsonObject{{"name", "Nenhuma GPU DRM encontrada"}};
    const auto cpuSample = cpu.sample();
    for(auto it=cpuSample.begin();it!=cpuSample.end();++it) current[it.key()]=it.value();
    bool alive = pid > 0 && QFileInfo::exists(QString("/proc/%1").arg(pid));
    current["processAlive"] = alive;
    current["processPid"] = pid;
    if (alive) {
      QString root = QString("/proc/%1/").arg(pid);
      current["processName"] = readText(root + "comm");
      QString stat = readText(root + "stat");
      int end = stat.lastIndexOf(')');
      auto fields = stat.mid(end + 2).split(' ');
      if (fields.size() > 21) {
        quint64 cpu = fields[11].toULongLong() + fields[12].toULongLong();
        if (cpuTime.isValid() && cpu >= lastCpu) {
          double sec = cpuTime.nsecsElapsed() / 1e9;
          if (sec > 0)
            current["processCpu"] = (cpu - lastCpu) * 100.0 / (sysconf(_SC_CLK_TCK) * sec);
        }
        lastCpu = cpu;
        cpuTime.restart();
        current["processRamMiB"] = fields[21].toDouble() * sysconf(_SC_PAGESIZE) / 1048576.;
      }
    } else {
      lastCpu = 0;
      cpuTime.invalidate();
    }
  }
  if(!config.frametimeLog.isEmpty()) {
    const auto incoming=frameLog.poll(config.frametimeLog);
    if(frameLog.resetDetected) {frameTimes={};lastFrame.invalidate();}
    const qint64 age=QFileInfo(config.frametimeLog).lastModified().msecsTo(QDateTime::currentDateTime());
    if(!incoming.isEmpty() && age>=0 && age<=3000) {
      for(auto v:incoming) {frameTimes.append(v);if(frameTimes.size()>240)frameTimes.removeFirst();}
      lastFrame.restart();
    }
    if(!lastFrame.isValid() || lastFrame.elapsed()>3000) frameTimes={};
  }
  for(auto key:{"fps","fpsLow","fpsAvg","fpsMax"}) current.remove(key);
  const auto fps=frameMetrics(frameTimes);
  for(auto it=fps.begin();it!=fps.end();++it) current[it.key()]=it.value();
  current["frameTimes"]=frameTimes;
  emit snapshot(current);
  if (port > 0 && udp) {
    QJsonObject state = {
        {"version", 1}, {"token", token}, {"metrics", current}, {"config", config.json()}, {"frameTimes",frameTimes}};
    udp->writeDatagram(QJsonDocument(state).toJson(QJsonDocument::Compact), QHostAddress::LocalHost,
                       quint16(port));
  }
}
