#include "codec_suite.h"
#include "repository.h"
#include <cstdio>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("gpu-video-worker");
  QCoreApplication::setApplicationVersion("0.2.0");
  QCommandLineParser p;
  p.addHelpOption();
  p.addOption({"pci", "GPU PCI", "id"});
  p.addOption({"output", "JSON de resultado", "path"});
  p.process(app);
  GpuBackend backend;
  Gpu gpu;
  bool found = false;
  for (auto g : backend.enumerate())
    if (g.pci == p.value("pci")) {
      gpu = g;
      found = true;
      break;
    }
  if (!found) {
    fprintf(stderr, "GPU não encontrada\n");
    return 2;
  }
  CodecSuite suite;
  QJsonArray telemetry;
  QTimer monitor;
  monitor.setInterval(500);
  QObject::connect(&monitor, &QTimer::timeout, &app, [&] {
    auto m = backend.sample(gpu);
    telemetry.append(m);
    if (m["temp"].isDouble() && m["temp"].toDouble() >= 78)
      suite.cancel();
  });
  QObject::connect(&suite, &CodecSuite::progress, &app, [](int value, QString text) {
    printf("PROGRESS %d %s\n", value, text.toUtf8().constData());
    fflush(stdout);
  });
  QObject::connect(&suite, &CodecSuite::result, &app, [&](QJsonObject r) {
    monitor.stop();
    r["gpu"] = publicGpu(gpu);
    r["telemetry"] = telemetry;
    r["voltageWritten"] = false;
    r["powerLimitWritten"] = false;
    QString error;
    if (!p.value("output").isEmpty() && !saveJson(p.value("output"), r, &error))
      fprintf(stderr, "%s\n", error.toUtf8().constData());
    printf("RESULT %s\n", QJsonDocument(r).toJson(QJsonDocument::Compact).constData());
    app.exit(r["valid"].toBool() ? 0 : 1);
  });
  monitor.start();
  QTimer::singleShot(0, &app, [&] { suite.start(gpu); });
  return app.exec();
}
