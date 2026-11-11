#include "driver_stack.h"
#include "gpu.h"
#include "system_settings.h"
#include <cstdio>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  auto args = app.arguments();
  QJsonObject result;
  if (args.size() == 3 && args[1] == "--driver-catalog")
    result = nativeDriverCatalog(args[2]);
  else if (args.size() == 3 && args[1] == "--driver-plan") {
    QFile f(args[2]);
    if (f.size() < 2 * 1024 * 1024 && f.open(QIODevice::ReadOnly))
      result = nativeDriverPlan(QJsonDocument::fromJson(f.readAll()).object());
  } else if (args.size() == 2 && args[1] == "--audio-inventory")
    result = audioInventory();
  if (result.isEmpty())
    result = {{"valid", false}, {"error", "Operação de leitura inválida"}};
  auto output = QJsonDocument(result).toJson(QJsonDocument::Compact);
  std::puts(output.constData());
  return result["valid"].toBool() ? 0 : 2;
}
