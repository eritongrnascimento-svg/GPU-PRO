#include "gpu.h"
#include "repository.h"
#include <cstdio>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationVersion("0.6.1");
  GpuBackend backend;
  QJsonArray devices;
  for (auto gpu : backend.enumerate()) {
    auto metrics = backend.sample(gpu);
    devices.append(QJsonObject{{"gpu", publicGpu(gpu)},
                               {"pci", gpu.pci},
                               {"driver", gpu.driver},
                               {"renderNode", gpu.render},
                               {"metrics", metrics}});
  }
  QJsonObject report{{"version", "0.6.1"},
                     {"devices", devices},
                     {"voltageWritten", false},
                     {"powerLimitWritten", false}};
  auto bytes = QJsonDocument(report).toJson();
  if (argc == 2) {
    QSaveFile file(QString::fromLocal8Bit(argv[1]));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
      return 1;
  } else
    printf("%s", bytes.constData());
  return devices.isEmpty() ? 2 : 0;
}
