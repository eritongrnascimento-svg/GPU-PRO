// Linux helper: validated clocks, power, independent AMD voltage offset and fan controls.
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
    return fail("Esperado: PCI core|mem|trial-core|power|voltageOffset|fanN|fans-auto valor [PID]");
  if(runningInWsl())return fail("WSL oferece somente leitura; ajustes exigem Linux nativo compatível");
  QString domain = args[2];
  bool tuning=domain=="power"||domain=="voltageOffset"||domain=="fans-auto"||QRegularExpression("^fan(?:[0-9]|1[0-5])$").match(domain).hasMatch();
  if (domain != "core" && domain != "mem" && domain != "trial-core" && !tuning)
    return fail("Operação não reconhecida");
  if((domain=="trial-core")!=(args.size()==5))return fail("Argumentos inválidos para a operação");
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
  if(tuning) {
    if(domain=="fans-auto") {
      if(value!=-1)return fail("Restauração automática exige -1");
      auto caps=gpuTuningCaps(gpu,nv);bool found=false,all=true;QStringList failures;
      for(auto it=caps.begin();it!=caps.end();++it)if(it.key().startsWith("fan")) {found=true;if(!setGpuTuning(gpu,nv,it.key(),-1,error)){all=false;failures<<it.key()+": "+error;}}
      if(!found||!all)return fail("Retorno automático não confirmado: "+failures.join("; "));
    } else {
      auto sample=backend.sample(gpu);
      if(!sample["temp"].isDouble()||sample["temp"].toDouble()>=80)return fail("Temperatura ausente ou >=80 °C; ajuste recusado");
      if(!setGpuTuning(gpu,nv,domain,value,error))return fail(error);
    }
    message("APPLIED",{{"domain",domain},{"value",value},{"voltageWritten",domain=="voltageOffset"},{"powerLimitWritten",domain=="power"}});
    return 0;
  }
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
