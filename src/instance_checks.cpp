#include "single_instance.h"
#include <cstdio>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("LinuxGPUControl");
  auto args = app.arguments();
  if (args.size() == 3) {
    SingleInstance instance(args[2], "fixture");
    auto result = instance.acquire();
    if (args[1] == "client")
      return result == SingleInstance::ActivatedExisting ? 0 : 4;
    if (result != SingleInstance::Primary) {
      qCritical() << instance.error();
      return 5;
    }
    std::puts("READY");
    std::fflush(stdout);
    QObject::connect(&instance, &SingleInstance::activationRequested, &app, [&] {
      std::puts("ACTIVATED");
      std::fflush(stdout);
    });
    QTimer::singleShot(15000, &app, &QCoreApplication::quit);
    return app.exec();
  }
  QTemporaryDir temp;
  QString path = temp.path() + "/session";
  QProcess owner;
  owner.start(app.applicationFilePath(), {"owner", path});
  if (!owner.waitForReadyRead(3000) || !owner.readAllStandardOutput().contains("READY")) {
    qCritical() << owner.readAllStandardError() << owner.exitCode();
    return 1;
  }
  QProcess client;
  client.start(app.applicationFilePath(), {"client", path});
  if (!client.waitForFinished(4000) || client.exitCode() != 0)
    return 2;
  if (!owner.waitForReadyRead(2000) || !owner.readAllStandardOutput().contains("ACTIVATED"))
    return 3;
  // A killed owner must not permanently block reopening the panel.
  owner.kill();
  owner.waitForFinished();
  SingleInstance recovered(path, "fixture");
  if (recovered.acquire() != SingleInstance::Primary)
    return 6;
  // Sessions are isolated, while portable and installed copies share the same identity.
  SingleInstance other(path, "other-session");
  if (other.acquire() != SingleInstance::Primary)
    return 7;
  return 0;
}
