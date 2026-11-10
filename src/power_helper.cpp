// Clock-only helper. There are no voltage or power-limit writes in this executable.
#include "gpu.h"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <poll.h>
#include <unistd.h>
static volatile sig_atomic_t interrupted = 0;
static void stop(int) { interrupted = 1; }
static void message(QString tag, QJsonObject o) {
  printf("%s %s\n", tag.toLatin1().constData(),
         QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
  fflush(stdout);
}
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  auto args = app.arguments();
  auto fail = [](QString s) {
    fprintf(stderr, "%s\n", s.toUtf8().constData());
    return 1;
  };
  if (args.size() < 4 || args.size() > 5 ||
      !QRegularExpression("^[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\\.[0-7]$")
           .match(args[1])
           .hasMatch())
    return fail("Esperado: PCI core|mem|trial-core MHz [PID]");
  QString domain = args[2];
  if (domain != "core" && domain != "mem" && domain != "trial-core")
    return fail("Operação proibida: somente clocks. Tensão e potência bloqueadas.");
  if (geteuid() != 0)
    return fail("Autorize o helper de clocks pelo pkexec do sistema");
  bool ok = false;
  int value = args[3].toInt(&ok);
  if (!ok || value < -10000 || value > 100000)
    return fail("Frequência inválida");
  GpuBackend backend;
  Gpu gpu;
  bool found = false;
  for (auto g : backend.enumerate())
    if (g.pci == args[1]) {
      gpu = g;
      found = true;
      break;
    }
  if (!found)
    return fail("GPU não encontrada");
  QLockFile lock("/run/lock/linux-gpu-control-clock-" + gpu.pci + ".lock");
  if (!lock.tryLock(0))
    return fail("Outro ajuste está em andamento nesta GPU");
  Nvml nv;
  QString error;
  if (domain != "trial-core") {
    bool applied = gpu.vendor == "NVIDIA" ? nv.setOffset(gpu.pci, domain, value, error)
                                          : setSysfsClock(gpu, domain, value, error);
    if (!applied)
      return fail(error);
    message("APPLIED", {{"domain", domain},
                        {"value", value},
                        {"voltageWritten", false},
                        {"powerLimitWritten", false}});
    return 0;
  }
  if (gpu.vendor != "NVIDIA" || args.size() != 5)
    return fail("Teste curto automatizado disponível somente no backend NVIDIA NVML validado");
  pid_t owner = args[4].toInt(&ok);
  if (!ok || owner <= 1 || kill(owner, 0) != 0)
    return fail("Processo proprietário inválido");
  auto before = backend.sample(gpu);
  auto caps = nv.offsets(nv.handle(gpu.pci));
  if (!before["temp"].isDouble() || before["temp"].toDouble() >= 60 ||
      !caps["coreCurrent"].isDouble() || !caps["memCurrent"].isDouble() ||
      !before["powerLimit"].isDouble())
    return fail("Telemetria incompleta ou GPU >= 60 °C; teste recusado");
  if (before["usage"].toDouble(100) > 40)
    return fail("GPU ocupada; encerre outras cargas antes do teste de clocks");
  int original = caps["coreCurrent"].toInt(), memory = caps["memCurrent"].toInt();
  if (value <= original || value > original + 15)
    return fail("Teste limitado a incremento máximo de 15 MHz no core");
  pollfd pending{STDIN_FILENO, POLLIN | POLLHUP, 0};
  if (poll(&pending, 1, 0) > 0 && (pending.revents & (POLLIN | POLLHUP))) {
    char input[64];
    ssize_t n = read(STDIN_FILENO, input, sizeof input);
    if (n <= 0 || QByteArray(input, int(n)).contains("STOP")) {
      message("RESTORED", {{"kind", "clock-trial"},
                           {"valid", false},
                           {"restored", true},
                           {"status", "cancelled-before-apply"},
                           {"coreBefore", original},
                           {"coreRestored", original},
                           {"voltageWritten", false},
                           {"powerLimitWritten", false}});
      return 1;
    }
  }
  std::signal(SIGTERM, stop);
  std::signal(SIGINT, stop);
  if (!nv.setOffset(gpu.pci, "core", value, error)) {
    QString originalError = error, restoreError;
    bool rollback = nv.setOffset(gpu.pci, "core", original, restoreError);
    return fail(
        originalError +
        QString("; restauração após falha: %1").arg(rollback ? "confirmada" : restoreError));
  }

  QElapsedTimer timer;
  timer.start();
  QJsonArray telemetry;
  message("TRIAL", {{"phase", "ready"},
                    {"coreBefore", original},
                    {"coreTested", value},
                    {"memoryBefore", memory},
                    {"powerBefore", before["powerLimit"]},
                    {"voltageWritten", false},
                    {"powerLimitWritten", false}});
  QString reason = "deadline";
  bool thermal = false;
  while (timer.elapsed() < 20000 && !interrupted) {
    pollfd input{STDIN_FILENO, POLLIN | POLLHUP, 0};
    int rc = poll(&input, 1, 250);
    if (rc > 0 && (input.revents & (POLLIN | POLLHUP))) {
      char buffer[64];
      ssize_t n = read(STDIN_FILENO, buffer, sizeof buffer);
      if (n <= 0 || QByteArray(buffer, int(n)).contains("STOP")) {
        reason = "stopped";
        break;
      }
    }
    if (kill(owner, 0) != 0 && errno == ESRCH) {
      reason = "owner-exited";
      break;
    }
    auto reading = backend.sample(gpu);
    QJsonObject sample{
        {"elapsedMs", timer.elapsed()},       {"temp", reading["temp"]},
        {"hotspot", reading["hotspot"]},      {"clock", reading["clock"]},
        {"power", reading["power"]},          {"coreCurrent", reading["coreCurrent"]},
        {"memCurrent", reading["memCurrent"]}};
    telemetry.append(sample);
    message("SAMPLE", sample);
    if (!reading["temp"].isDouble() || reading["temp"].toDouble() >= 75 ||
        (reading["hotspot"].isDouble() && reading["hotspot"].toDouble() >= 95)) {
      thermal = true;
      reason = "telemetry-or-thermal-stop";
      break;
    }
    if (reading["powerLimit"] != before["powerLimit"] ||
        reading["memCurrent"].toInt(memory) != memory) {
      thermal = true;
      reason = "external-settings-changed";
      break;
    }
  }
  bool restored = nv.setOffset(gpu.pci, "core", original, error);
  auto after = backend.sample(gpu);
  bool unchanged = after["powerLimit"] == before["powerLimit"] && after["memCurrent"].isDouble() &&
                   after["memCurrent"].toInt() == memory;
  message("RESTORED", {{"kind", "clock-trial"},
                       {"valid", restored && !thermal && unchanged && !interrupted},
                       {"restored", restored},
                       {"status", reason},
                       {"coreBefore", original},
                       {"coreTested", value},
                       {"coreRestored", after["coreCurrent"]},
                       {"memoryBefore", memory},
                       {"memoryAfter", after["memCurrent"]},
                       {"powerBefore", before["powerLimit"]},
                       {"powerAfter", after["powerLimit"]},
                       {"voltageWritten", false},
                       {"powerLimitWritten", false},
                       {"telemetry", telemetry},
                       {"originalLimitsPreserved", unchanged}});
  return restored && !thermal && unchanged && !interrupted ? 0 : 1;
}
