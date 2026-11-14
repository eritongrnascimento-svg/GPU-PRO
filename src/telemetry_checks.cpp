#include "service.h"
#include <cstdio>
int main(int argc,char **argv) {
  QCoreApplication app(argc,argv);int failures=0;
  auto check=[&](bool ok,const char *why){if(!ok){++failures;fprintf(stderr,"FAIL %s\n",why);}};
  QTemporaryDir dir;auto write=[&](QString name,QByteArray bytes){QFile f(dir.filePath(name));check(f.open(QIODevice::WriteOnly),"fixture open");f.write(bytes);};
  write("frames.csv","fps,frametime\n100,10\n50,20\n");
  QJsonObject current;TelemetryService service;
  QObject::connect(&service,&TelemetryService::snapshot,[&](QJsonObject sample){current=sample;});
  service.start();OverlayConfig config;config.frametimeLog=dir.filePath("frames.csv");config.frametimeGraph=false;
  service.overlay(config.json());
  check(current["fps"].toDouble()==50.&&current["frameTimes"].toArray().size()==2,"panel FPS and graph collect while HUD graph disabled");
  QEventLoop loop;QTimer::singleShot(3500,&loop,&QEventLoop::quit);loop.exec();
  check(!current.contains("fps")&&!current.contains("fpsAvg")&&current["frameTimes"].toArray().isEmpty(),"stale CSV clears metrics and graph after three seconds");
  write("other.csv","fps,frametime\n25,40\n");config.frametimeLog=dir.filePath("other.csv");service.overlay(config.json());
  check(current["fps"].toDouble()==25.&&current["fpsMax"].toDouble()==25.&&current["frameTimes"].toArray().size()==1,"switch source resets graph and statistics");
  config.frametimeLog.clear();service.overlay(config.json());
  check(!current.contains("fps")&&current["frameTimes"].toArray().isEmpty(),"clear source resets immediately");
  service.stop();fprintf(stdout,"Telemetry checks: %d failure(s)\n",failures);return failures?1:0;
}
