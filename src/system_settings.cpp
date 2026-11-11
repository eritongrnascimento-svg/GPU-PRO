#include "system_settings.h"
#include "driver_packages.h"
namespace {
QByteArray run(QString program, QStringList args, QString *error = nullptr) {
  QProcess p;
  auto env = nativeEnvironment();
  env.insert("LC_ALL", "C");
  p.setProcessEnvironment(env);
  p.start(program, args);
  if (!p.waitForFinished(12000) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
    if (error) {
      *error = QString::fromUtf8(p.readAllStandardError()).left(1500);
      if (error->isEmpty())
        *error = p.errorString();
    }
    return {};
  }
  return p.readAllStandardOutput();
}
QJsonObject failure(QString s) { return {{"valid", false}, {"error", s}}; }
QString settingsHome() {
  QString p = qEnvironmentVariable("XDG_CONFIG_HOME");
  return p.isEmpty() ? QDir::homePath() + "/.config" : p;
}
bool atomic(QString path, QByteArray data) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(data) == data.size() && f.commit();
}
QString digest(QByteArray data) {
  return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
} // namespace
QJsonObject audioInventory() {
  QString error;
  auto sinks =
      QJsonDocument::fromJson(run("/usr/bin/pactl", {"-f", "json", "list", "sinks"}, &error))
          .array();
  if (!error.isEmpty())
    return failure("Servidor PulseAudio/PipeWire indisponível: " + error);
  auto sources =
      QJsonDocument::fromJson(run("/usr/bin/pactl", {"-f", "json", "list", "sources"}, &error))
          .array();
  return parseAudioInventory(sinks, sources,
                             QString::fromUtf8(run("/usr/bin/pactl", {"info"}, &error)),
                             readText("/proc/asound/cards"));
}
QJsonObject parseAudioInventory(QJsonArray sinks, QJsonArray sources, QString info,
                                QString alsaCards) {
  QString output, input;
  for (auto line : info.split('\n')) {
    if (line.startsWith("Default Sink: "))
      output = line.mid(14).trimmed();
    if (line.startsWith("Default Source: "))
      input = line.mid(16).trimmed();
  }
  auto clean = [](QJsonArray nodes, bool source) {
    QJsonArray a;
    for (auto v : nodes) {
      auto n = v.toObject();
      QString name = n["name"].toString();
      if (name.isEmpty() || (source && name.endsWith(".monitor")))
        continue;
      auto props = n["properties"].toObject();
      bool virtualNode =
          props["device.class"] == "filter" || props["device.api"].toString().isEmpty();
      a.append(
          QJsonObject{{"name", name}, {"description", n["description"]}, {"virtual", virtualNode}});
    }
    return a;
  };
  return {{"valid", true},
          {"outputs", clean(sinks, false)},
          {"inputs", clean(sources, true)},
          {"output", output},
          {"input", input},
          {"alsaCards", alsaCards},
          {"notice", "Alinhar muda os destinos padrão para novas conexões. Não reinicia serviços, "
                     "não move a captura atual do OBS nem recupera codecs ausentes no ALSA."}};
}
QJsonObject alignAudio(QString output, QString input, QString storage) {
  auto inventory = audioInventory();
  if (!inventory["valid"].toBool())
    return inventory;
  auto exists = [&](QString key, QString n) {
    if (n.isEmpty())
      return true;
    for (auto v : inventory[key].toArray())
      if (v.toObject()["name"] == n)
        return true;
    return false;
  };
  if (!exists("outputs", output) || !exists("inputs", input))
    return failure("O dispositivo selecionado não está mais disponível");
  if ((output.isEmpty() || output == inventory["output"].toString()) &&
      (input.isEmpty() || input == inventory["input"].toString()))
    return {{"valid", true}, {"message", "Destinos já alinhados; nenhuma alteração necessária."}};
  QString error;
  QJsonObject backup{{"schema", "gpu-pro-audio-backup/1"},
                     {"output", inventory["output"]},
                     {"input", inventory["input"]}};
  if (!saveJson(storage + "/audio-before.json", backup, &error))
    return failure(error);
  if (!output.isEmpty()) {
    run("/usr/bin/pactl", {"set-default-sink", output}, &error);
    if (!error.isEmpty())
      return failure(error);
  }
  if (!input.isEmpty()) {
    run("/usr/bin/pactl", {"set-default-source", input}, &error);
    if (!error.isEmpty()) {
      QString ignored;
      run("/usr/bin/pactl", {"set-default-sink", inventory["output"].toString()}, &ignored);
      return failure(error + " · Tentativa de restaurar a saída anterior: " +
                     (ignored.isEmpty() ? "concluída" : ignored));
    }
  }
  auto after = audioInventory();
  if ((!output.isEmpty() && after["output"] != output) ||
      (!input.isEmpty() && after["input"] != input))
    return failure("O servidor não confirmou os destinos; confira o áudio antes de gravar");
  return {{"valid", true},
          {"message",
           "Destinos padrão alinhados; volumes, filtros e conexões existentes preservados."}};
}
QJsonObject restoreAudio(QString storage) {
  auto b = QJsonDocument::fromJson(readText(storage + "/audio-before.json").toUtf8()).object();
  if (b["schema"] != "gpu-pro-audio-backup/1")
    return failure("Nenhum backup de áudio disponível");
  // Keep the original backup intact, rather than overwriting it with the undo.
  return alignAudio(b["output"].toString(), b["input"].toString(), storage + "/undo");
}
QJsonObject gpuSessionPlan(const QList<Gpu> &gpus, QString pci, QString home) {
  if (home.isEmpty())
    home = settingsHome();
  if (!QDir::isAbsolutePath(home) || home.contains(':'))
    return failure("Diretório de configuração não suportado para preferência de sessão");
  if(gpus.size()<2)return failure("Somente uma GPU ativa: a preferência por aplicativo está disponível, mas habilite ambas antes de configurar a lista da sessão gráfica.");
  bool found = false;
  for (auto g : gpus)
    if (g.pci == pci)
      found = true;
  if (!found)
    return failure("GPU selecionada não está disponível");
  if (!qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("KDE", Qt::CaseInsensitive) ||
      qEnvironmentVariable("XDG_SESSION_TYPE") != "wayland")
    return failure("Preferência de sessão disponível para KDE Wayland; outros ambientes mantêm "
                   "seleção por aplicativo");
  QStringList paths;
  QJsonArray links;
  auto add = [&](const Gpu &g) {
    QString source = "/dev/dri/by-path/pci-" + g.pci + "-card";
    QString alias = home + "/gpu-pro/drm/" + QString(g.pci).replace(':', '_').replace('.', '_');
    paths << alias;
    links.append(QJsonObject{{"path", alias}, {"target", source}});
  };
  for (auto g : gpus)
    if (g.pci == pci)
      add(g);
  for (auto g : gpus)
    if (g.pci != pci)
      add(g);
  QString script = "# GPU-PRO managed GPU preference\n# Requer nova sessão; todos os dispositivos "
                   "permanecem habilitados.\nexport KWIN_DRM_DEVICES='" +
                   paths.join(':') + "'\n";
  if (home.contains('\'') || home.contains('\n'))
    return failure("Caminho de configuração não suportado para script de sessão");
  return {
      {"valid", true},
      {"target", home + "/plasma-workspace/env/gpu-pro-gpu.sh"},
      {"content", script},
      {"links", links},
      {"preferred", pci},
      {"message", "Preferência do KWin para a próxima sessão. Não altera BIOS, cabos ou processos "
                  "em execução; a segunda GPU permanece na lista. Exige sair e entrar na sessão."}};
}
QJsonObject applyGpuSession(const QJsonObject &plan, QString storage) {
  if (!plan["valid"].toBool())
    return failure("Plano inválido");
  QString target = plan["target"].toString();
  if (target != settingsHome() + "/plasma-workspace/env/gpu-pro-gpu.sh" ||
      QFileInfo(target).isSymLink())
    return failure("Destino de preferência inválido");
  QByteArray old;
  QFile previous(target);
  bool existed = QFileInfo::exists(target);
  if (existed) {
    if (!previous.open(QIODevice::ReadOnly))
      return failure("Não foi possível ler a preferência anterior");
    old = previous.readAll();
  }
  if (existed && !old.startsWith("# GPU-PRO managed GPU preference"))
    return failure("Arquivo de sessão existente não pertence ao GPU-PRO; preservado");
  QByteArray content = plan["content"].toString().toUtf8();
  if (!content.startsWith("# GPU-PRO managed GPU preference") || !plan["links"].isArray())
    return failure("Conteúdo da preferência inválido");
  QString error;
  QJsonObject backup{{"schema", "gpu-pro-session-backup/1"},
                     {"existed", existed},
                     {"old", QString::fromLatin1(old.toBase64())},
                     {"installedSha256", digest(content)}};
  if (!saveJson(storage + "/gpu-session-before.json", backup, &error))
    return failure(error);
  for (auto v : plan["links"].toArray()) {
    auto link = v.toObject();
    QString path = link["path"].toString(), source = link["target"].toString();
    if (!path.startsWith(settingsHome() + "/gpu-pro/drm/") || !QFileInfo::exists(source))
      return failure("Conector DRM estável indisponível para a GPU selecionada");
    if (QFileInfo::exists(path) || QFileInfo(path).isSymLink()) {
      if (QFileInfo(path).symLinkTarget() != source)
        return failure("Alias DRM existente pertence a outra configuração");
    } else {
      QDir().mkpath(QFileInfo(path).absolutePath());
      if (!QFile::link(source, path))
        return failure("Não foi possível preparar alias DRM");
    }
  }
  if (!atomic(target, content))
    return failure("Falha ao gravar preferência de sessão");
  QFile::setPermissions(target,
                        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
  return {{"valid", true}, {"message", plan["message"]}};
}
QJsonObject restoreGpuSession(QString storage) {
  auto b =
      QJsonDocument::fromJson(readText(storage + "/gpu-session-before.json").toUtf8()).object();
  QString target = settingsHome() + "/plasma-workspace/env/gpu-pro-gpu.sh";
  QFile file(target);
  if (b["schema"] != "gpu-pro-session-backup/1" || !file.open(QIODevice::ReadOnly) ||
      QFileInfo(target).isSymLink())
    return failure("Backup de sessão indisponível");
  auto current = file.readAll();
  file.close();
  if (digest(current) != b["installedSha256"].toString())
    return failure("Preferência foi alterada externamente; arquivo preservado");
  bool ok = b["existed"].toBool()
                ? atomic(target, QByteArray::fromBase64(b["old"].toString().toLatin1()))
                : QFile::remove(target);
  return ok ? QJsonObject{{"valid", true},
                          {"message", "Preferência anterior restaurada; exige nova sessão."}}
            : failure("Falha ao restaurar preferência");
}
