#include "driver_stack.h"
#include "driver_packages.h"
#include "model.h"
#include <functional>
namespace {
struct Result {
  QByteArray bytes;
  QString error;
  int code = -1;
};
Result run(QString program, QStringList args) {
  QProcess p;
  auto env = nativeEnvironment();
  env.insert("LC_ALL", "C");
  env.insert("COLUMNS", "240");
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(program, args);
  if (!p.waitForFinished(30000)) {
    p.kill();
    p.waitForFinished();
    return {{}, "Consulta nativa excedeu o prazo", -1};
  }
  auto bytes = p.readAllStandardOutput();
  return {bytes, p.exitCode() == 0 ? QString{} : QString::fromUtf8(bytes).left(2000), p.exitCode()};
}
QString command(QString p, QStringList a) { return QString::fromUtf8(run(p, a).bytes); }
bool kernel(QString n) {
  return QRegularExpression("^linux(?:-[a-z0-9]+)*$").match(n).hasMatch() &&
         !n.contains("firmware") && !n.contains("api") && !n.contains("tools") &&
         !n.contains("nvidia");
}
bool module(QString n) {
  return QRegularExpression("^(?:nvidia(?:-open)?(?:-dkms|-lts)?|nvidia-[0-9]+xx(?:-open)?-dkms|"
                            "linux(?:-[a-z0-9]+)*-nvidia(?:-open)?)$")
      .match(n)
      .hasMatch();
}
bool stack(QString n, QString vendor) {
  if (vendor == "NVIDIA")
    return module(n) || QRegularExpression("^(?:lib32-)?(?:nvidia(?:-[0-9]+xx)?-(?:utils|settings)|"
                                           "opencl-nvidia(?:-[0-9]+xx)?)$")
                            .match(n)
                            .hasMatch();
  return driverPackageAllowed(n, vendor);
}
QJsonObject failure(QString s) { return {{"valid", false}, {"error", s}}; }
QJsonArray packageInfo(QString text) {
  QJsonArray rows;
  QMap<QString, QString> fields;
  QString last;
  auto flush = [&] {
    if (fields.contains("Name") && fields.contains("Version")) {
      QJsonArray depends;
      for (auto d : fields["Depends On"].split(' ', Qt::SkipEmptyParts))
        if (d != "None")
          depends.append(d);
      rows.append(QJsonObject{{"name", fields["Name"]},
                              {"version", fields["Version"]},
                              {"repo", fields.value("Repository", "instalado")},
                              {"architecture", fields["Architecture"]},
                              {"depends", depends},
                              {"manager", "pacman"}});
    }
    fields.clear();
    last.clear();
  };
  for (auto line : text.split('\n')) {
    if (line.trimmed().isEmpty()) {
      flush();
      continue;
    }
    auto match = QRegularExpression("^([^ :][^:]*?)\\s+:\\s*(.*)$").match(line);
    if (match.hasMatch()) {
      last = match.captured(1).trimmed();
      fields[last] = match.captured(2).trimmed();
    } else if (line.startsWith(' ') && !last.isEmpty())
      fields[last] += ' ' + line.trimmed();
  }
  flush();
  return rows;
}
QJsonArray installedPackages() { return packageInfo(command("/usr/bin/pacman", {"-Qi"})); }
QJsonObject readPackage(QString file) {
  auto list = packageInfo(command("/usr/bin/pacman", {"-Qip", file}));
  return list.isEmpty() ? QJsonObject{} : list.first().toObject();
}
} // namespace
QJsonArray completeDriverProfiles(const QJsonArray &packages, const QJsonArray &installed,
                                  QString vendor) {
  QSet<QString> kernels;
  for (auto v : installed) {
    auto n = v.toObject()["name"].toString();
    if (kernel(n) && !n.endsWith("-headers"))
      kernels.insert(n);
  }
  QJsonArray profiles;
  for (auto v : packages) {
    auto row = v.toObject();
    if (row["installedOnly"].toBool())
      continue;
    QString name = row["name"].toString(), manager = row["manager"].toString();
    bool complete =
        vendor == "NVIDIA"
            ? (manager == "apt" ? QRegularExpression("^nvidia-driver-[0-9]+(?:-server)?(?:-open)?$")
                                      .match(name)
                                      .hasMatch()
               : manager == "dnf" ? (name == "akmod-nvidia" || name == "akmod-nvidia-open")
                                  : module(name))
            : name == "mesa" || name == "mesa-vulkan-drivers" || name == "Mesa";
    if (!complete)
      continue;
    if (manager == "pacman" && name.startsWith("linux")) {
      QString k = name;
      k.remove(QRegularExpression("-nvidia(?:-open)?$"));
      if (!kernels.contains(k))
        continue;
    }
    if (manager == "pacman" &&
        ((name == "nvidia" || name == "nvidia-open") && !kernels.contains("linux")))
      continue;
    if (manager == "pacman" &&
        ((name == "nvidia-lts" || name == "nvidia-open-lts") && !kernels.contains("linux-lts")))
      continue;
    QJsonArray targets{row};
    QString version = row["version"].toString();
    if (vendor == "NVIDIA" && manager == "pacman") {
      QString dependency;
      for (auto d : row["depends"].toArray())
        if (d.toString().startsWith("nvidia-utils=") ||
            QRegularExpression("^nvidia-[0-9]+xx-utils=").match(d.toString()).hasMatch())
          dependency = d.toString();
      QString wantedUtility = "nvidia-utils";
      auto legacy = QRegularExpression("^nvidia-([0-9]+xx)").match(name);
      if (legacy.hasMatch())
        wantedUtility = "nvidia-" + legacy.captured(1) + "-utils";
      bool found = false;
      for (auto u : packages) {
        auto utility = u.toObject();
        QString n = utility["name"].toString(), ver = utility["version"].toString();
        if ((dependency.isEmpty() && n == wantedUtility &&
             ver.section('-', 0, -2) == row["version"].toString().section('-', 0, -2)) ||
            dependency == n + '=' + ver || dependency == n + '=' + ver.section('-', 0, -2)) {
          if (!dependency.isEmpty() && utility["repo"] != row["repo"])
            continue;
          targets.append(utility);
          version = ver;
          found = true;
          break;
        }
      }
      if (!found)
        continue; // A module alone is not a complete driver choice.
      auto normalized = [](QString n) { return n.remove(QRegularExpression("-[0-9]+xx")); };
      for (auto old : installed) {
        QString wanted = normalized(old.toObject()["name"].toString());
        if (module(wanted) || (!wanted.contains("utils") && !wanted.contains("opencl") &&
                               !wanted.contains("settings")))
          continue;
        for (auto part : packages) {
          auto component = part.toObject();
          if (normalized(component["name"].toString()) == wanted &&
              component["version"].toString().section('-', 0, -2) == version.section('-', 0, -2) &&
              component["repo"] == row["repo"] && !targets.contains(component)) {
            targets.append(component);
            break;
          }
        }
      }
      if (name.contains("dkms"))
        for (auto k : kernels) {
          for (auto part : packages) {
            auto header = part.toObject();
            if (header["name"] == k + "-headers" && !targets.contains(header)) {
              targets.append(header);
              break;
            }
          }
        }
    }
    if (vendor != "NVIDIA") {
      for (QString wanted : vendor == "Intel" ? QStringList{"vulkan-intel", "intel-media-driver"}
                                              : QStringList{"vulkan-radeon", "libva-mesa-driver"})
        for (auto u : packages) {
          auto component = u.toObject();
          if (component["name"] == wanted && component["repo"] == row["repo"]) {
            targets.append(component);
            break;
          }
        }
    }
    QString variant = vendor == "NVIDIA" ? (name.contains("open") ? "Open" : "Proprietário")
                                         : "Mesa / kernel nativo";
    QString label =
        vendor + " " + version + " · " + variant + " · " + name + " · " + row["repo"].toString();
    row["label"] = label;
    row["driverVersion"] = version;
    row["variant"] = variant;
    row["vendor"] = vendor;
    row["targets"] = targets;
    profiles.append(row);
  }
  QList<QJsonObject> sorted;
  for (auto p : profiles)
    sorted << p.toObject();
  QCollator collator;
  collator.setNumericMode(true);
  std::stable_sort(sorted.begin(), sorted.end(), [&](const auto &a, const auto &b) {
    int c = collator.compare(a["driverVersion"].toString(), b["driverVersion"].toString());
    return c < 0;
  });
  profiles = {};
  for (auto p : sorted)
    profiles.append(p);
  return profiles;
}
QJsonObject nativeDriverCatalog(QString vendor) {
  auto listing = run("/usr/bin/pacman", {"-Sl"});
  if (listing.code != 0)
    return failure(listing.error);
  QStringList targets;
  for (auto line : QString::fromUtf8(listing.bytes).split('\n')) {
    auto p = line.simplified().split(' ');
    if (p.size() >= 3 && (stack(p[1], vendor) || kernel(p[1])))
      targets << p[0] + '/' + p[1];
  }
  if (targets.isEmpty())
    return failure("Nenhum conjunto de driver encontrado nos índices locais");
  auto metadata = run("/usr/bin/pacman", QStringList{"-Si"} + targets);
  if (metadata.code != 0)
    return failure(metadata.error);
  auto packages = packageInfo(QString::fromUtf8(metadata.bytes));
  QJsonArray installed;
  QMap<QString, QString> current;
  for (auto v : installedPackages()) {
    auto r = v.toObject();
    current[r["name"].toString()] = r["version"].toString();
    if (stack(r["name"].toString(), vendor) || kernel(r["name"].toString()))
      installed.append(r);
  }
  for (int n = 0; n < packages.size(); ++n) {
    auto r = packages[n].toObject();
    r["installed"] = current.value(r["name"].toString());
    packages[n] = r;
  }
  return {{"valid", true},
          {"vendor", vendor},
          {"manager", "pacman"},
          {"profiles", completeDriverProfiles(packages, installed, vendor)},
          {"installed", installed}};
}
QJsonObject parsePacmanPreview(QString text, const QJsonArray &installed) {
  QMap<QString, QJsonObject> current;
  for (auto v : installed) {
    auto r = v.toObject();
    current[r["name"].toString()] = r;
  }
  QJsonArray additions, removals;
  int expected = -1, oldColumn = -1, newColumn = -1, netColumn = -1;
  bool table = false;
  for (auto line : text.split('\n')) {
    auto header = QRegularExpression("^Package \\(([0-9]+)\\)").match(line);
    if (header.hasMatch() && line.contains("Old Version") && line.contains("New Version") &&
        line.contains("Net Change")) {
      expected = header.captured(1).toInt();
      oldColumn = line.indexOf("Old Version");
      newColumn = line.indexOf("New Version");
      netColumn = line.indexOf("Net Change");
      table = true;
      continue;
    }
    if (!table)
      continue;
    if (line.startsWith("Total ") || line.startsWith("Net Upgrade")) {
      table = false;
      continue;
    }
    if (line.trimmed().isEmpty())
      continue;
    QString label = line.left(oldColumn).trimmed(),
            old = line.mid(oldColumn, newColumn - oldColumn).trimmed(),
            version = line.mid(newColumn, netColumn - newColumn).trimmed();
    QString name = label.section('/', -1),
            repo = label.contains('/') ? label.section('/', 0, 0) : QString{};
    if (!QRegularExpression("^[a-zA-Z0-9][a-zA-Z0-9+._-]*$").match(name).hasMatch())
      return failure("Formato de tabela nativa não reconhecido");
    if (version.isEmpty()) {
      if (!current.contains(name) || current[name]["version"] != old)
        return failure("Remoção não corresponde à instalação atual");
      removals.append(current[name]);
    } else {
      if (!QRegularExpression("^[A-Za-z0-9+._:~\\-]+$").match(version).hasMatch())
        return failure("Versão não reconhecida na prévia");
      additions.append(QJsonObject{{"name", name},
                                   {"version", version},
                                   {"repo", repo},
                                   {"installed", old},
                                   {"manager", "pacman"}});
    }
  }
  if (expected < 0 || expected != additions.size() + removals.size())
    return failure("Não foi possível conferir toda a tabela de alterações; instalação bloqueada");
  return {{"valid", true}, {"add", additions}, {"remove", removals}};
}
QJsonObject nativeDriverPlan(const QJsonObject &candidate) {
  QString vendor = candidate["vendor"].toString();
  if (!QStringList{"NVIDIA", "AMD", "Intel"}.contains(vendor))
    return failure("Fabricante inválido");
  auto targets = candidate["targets"].toArray();
  if (targets.isEmpty() || targets.size() > 2000)
    return failure("Conjunto de pacotes inválido");
  if (!QFileInfo("/usr/bin/bwrap").isExecutable())
    return failure("Instale bubblewrap pelo gerenciador da distro para habilitar a simulação "
                   "isolada; nenhum pacote foi alterado");
  QStringList args;
  bool restore = candidate["restore"].toBool();
  for (auto v : targets) {
    auto t = v.toObject();
    QString name = t["name"].toString(), version = t["version"].toString();
    QJsonObject actual;
    if (restore) {
      QString path = t["file"].toString();
      QFile f(path);
      if (QFileInfo(path).isSymLink() || !f.open(QIODevice::ReadOnly))
        return failure("Pacote anterior ausente");
      QCryptographicHash h(QCryptographicHash::Sha256);
      h.addData(&f);
      if (h.result().toHex() != t["sha256"].toString().toLatin1())
        return failure("Checksum do pacote anterior não confere");
      actual = readPackage(path);
      args << QFileInfo(path).absoluteFilePath();
    } else {
      if (!stack(name, vendor) && !kernel(name))
        return failure("Componente fora do conjunto reconhecido");
      QString target = t["repo"].toString() + '/' + name;
      auto rows = packageInfo(command("/usr/bin/pacman", {"-Si", target}));
      if (rows.size() == 1)
        actual = rows.first().toObject();
      args << target;
    }
    if (actual["name"] != name || actual["version"] != version)
      return failure("O pacote ou os índices mudaram. Consulte e simule novamente.");
  }
  QTemporaryDir sandbox;
  if (!sandbox.isValid())
    return failure("Sem diretório temporário para simulação");
  QString path = sandbox.path();
  for (auto sub : {"db", "cache", "hooks"})
    QDir().mkpath(path + '/' + sub);
  QString database = command("/usr/bin/pacman-conf", {"DBPath"}).trimmed();
  if (database.isEmpty())
    database = "/var/lib/pacman";
  for (auto sub : {"local", "sync"})
    if (!QFile::link(QDir(database).filePath(sub), path + "/db/" + sub))
      return failure("Não foi possível preparar base de consulta isolada");
  QString config = command("/usr/bin/pacman-conf", {});
  if (!config.startsWith("[options]"))
    return failure("Configuração pacman não reconhecida");
  config.replace("[options]", "[options]\nVerbosePkgLists");
  QFile conf(path + "/pacman.conf");
  if (!conf.open(QIODevice::WriteOnly) || conf.write(config.toUtf8()) < 0)
    return failure("Não foi possível preparar configuração de prévia");
  conf.close();
  QStringList launch{"--die-with-parent",
                     "--ro-bind",
                     "/",
                     "/",
                     "--dev",
                     "/dev",
                     "--proc",
                     "/proc",
                     "--unshare-user",
                     "--uid",
                     "0",
                     "--gid",
                     "0",
                     "--unshare-net",
                     "--bind",
                     path,
                     path,
                     "--setenv",
                     "HOME",
                     path,
                     "--setenv",
                     "COLUMNS",
                     "240",
                     "--setenv",
                     "TERM",
                     "dumb",
                     "/usr/bin/pacman",
                     "--config",
                     path + "/pacman.conf",
                     "--dbpath",
                     path + "/db",
                     "--cachedir",
                     path + "/cache",
                     "--logfile",
                     path + "/preview.log",
                     "--hookdir",
                     path + "/hooks",
                     "--color",
                     "never",
                     "--noprogressbar",
                     restore ? "-U" : "-Su"};
  launch << args;
  QProcess p;
  auto env = nativeEnvironment();
  env.insert("LC_ALL", "C");
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start("/usr/bin/bwrap", launch);
  QByteArray output;
  int answered = 0;
  bool refusedFinal = false;
  QElapsedTimer clock;
  clock.start();
  while (clock.elapsed() < 45000 && p.state() != QProcess::NotRunning) {
    p.waitForReadyRead(100);
    output += p.readAllStandardOutput();
    if (output.size() > 8 * 1024 * 1024)
      break;
    auto pending = output.mid(answered);
    if (pending.contains("Proceed with installation?")) {
      p.write("n\n");
      p.waitForBytesWritten();
      refusedFinal = true;
      answered = output.size();
    } else if ((pending.contains("Remove ") || pending.contains("Replace ")) &&
               pending.contains('?')) {
      p.write("y\n");
      p.waitForBytesWritten();
      answered = output.size();
    } else if (pending.contains("Enter a selection")) {
      p.write("1\n");
      p.waitForBytesWritten();
      answered = output.size();
    } else if (pending.contains("IgnorePkg/IgnoreGroup") && pending.contains('?')) {
      p.write("n\n");
      p.waitForBytesWritten();
      answered = output.size();
    }
  }
  if (p.state() != QProcess::NotRunning) {
    p.kill();
    p.waitForFinished();
  }
  output += p.readAllStandardOutput();
  if (!refusedFinal || p.exitStatus() != QProcess::NormalExit)
    return failure("Prévia isolada não concluída; nada instalado. " +
                   QString::fromUtf8(output).right(2200));
  auto installed = installedPackages();
  auto plan = parsePacmanPreview(QString::fromUtf8(output), installed);
  if (!plan["valid"].toBool())
    return plan;
  for (auto v : targets) {
    auto t = v.toObject();
    bool found = false;
    for (auto n : plan["add"].toArray())
      if (n.toObject()["name"] == t["name"] && n.toObject()["version"] == t["version"])
        found = true;
    if (!found)
      return failure("O resolvedor não incluiu todo o conjunto solicitado");
  }
  QSet<QString> names;
  for (auto v : plan["add"].toArray())
    names.insert(v.toObject()["name"].toString());
  for (auto v : plan["remove"].toArray())
    names.insert(v.toObject()["name"].toString());
  QJsonArray baseline;
  for (auto v : installed) {
    auto r = v.toObject();
    QString name = r["name"].toString();
    if (names.contains(name) || stack(name, vendor) || kernel(name))
      baseline.append(r);
  }
  QStringList actual{restore ? "-U" : "-Su"};
  actual << args;
  plan["vendor"] = vendor;
  plan["candidate"] = candidate;
  plan["baseline"] = baseline;
  plan["program"] = "/usr/bin/pacman";
  plan["arguments"] = QJsonArray::fromStringList(actual);
  plan["notice"] = "Prévia pelo pacman externo, em namespace sem rede e com o sistema somente para "
                   "leitura. A confirmação de instalação é recusada automaticamente. A transação "
                   "real exige autenticação e confirmação nativas; confira os mesmos "
                   "índices/versões. Dependências adicionais podem permanecer após a restauração.";
  return plan;
}
QString driverPlanDescription(const QJsonObject &p) {
  if (!p["valid"].toBool())
    return p["error"].toString();
  QString text = "SERÃO ADICIONADOS / ATUALIZADOS / RESTAURADOS:\n";
  for (auto v : p["add"].toArray()) {
    auto r = v.toObject();
    text += r["name"].toString() + "  " + r["installed"].toString() + " → " +
            r["version"].toString() + '\n';
  }
  text += "\nSERÃO REMOVIDOS:\n";
  auto removed = p["remove"].toArray();
  if (removed.isEmpty())
    text += "Nenhum pacote.\n";
  for (auto v : removed) {
    auto r = v.toObject();
    text += r["name"].toString() + " " + r["version"].toString() + '\n';
  }
  return text + '\n' + p["notice"].toString();
}
QJsonObject saveDriverSnapshot(const QJsonObject &plan, QString storage, QString *errorOut) {
  auto fail = [&](QString e) {
    if (errorOut)
      *errorOut = e;
    return QJsonObject{};
  };
  if (!plan["valid"].toBool() || plan["candidate"].toObject()["restore"].toBool())
    return fail("Plano de backup inválido");
  QMap<QString, QJsonObject> wanted;
  for (auto v : plan["baseline"].toArray()) {
    auto r = v.toObject();
    wanted[r["name"].toString() + '=' + r["version"].toString()] = r;
  }
  QString folder = storage + '/' + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
  QJsonArray files;
  auto dirs = command("/usr/bin/pacman-conf", {"CacheDir"}).split('\n', Qt::SkipEmptyParts);
  for (auto dir : dirs)
    for (auto name :
         QDir(dir).entryList({"*.pkg.tar.zst", "*.pkg.tar.xz", "*.pkg.tar.gz"}, QDir::Files)) {
      bool relevant = false;
      for (auto row : wanted)
        if (name.startsWith(row["name"].toString() + '-'))
          relevant = true;
      if (!relevant)
        continue;
      QString path = QDir(dir).filePath(name);
      auto meta = readPackage(path);
      QString key = meta["name"].toString() + '=' + meta["version"].toString();
      if (!wanted.contains(key) || meta["architecture"] != wanted[key]["architecture"])
        continue;
      auto row = wanted.take(key);
      QDir().mkpath(folder + "/packages");
      QString target = folder + "/packages/" + name;
      if (!QFile::copy(path, target))
        return fail("Falha ao preservar pacote anterior");
      if (QFileInfo::exists(path + ".sig") && !QFile::copy(path + ".sig", target + ".sig"))
        return fail("Falha ao preservar assinatura do pacote");
      QFile f(target);
      if (!f.open(QIODevice::ReadOnly))
        return fail("Falha ao conferir pacote anterior");
      QCryptographicHash h(QCryptographicHash::Sha256);
      h.addData(&f);
      row["file"] = QFileInfo(target).absoluteFilePath();
      row["sha256"] = QString::fromLatin1(h.result().toHex());
      files.append(row);
    }
  QStringList missing;
  for (auto it = wanted.begin(); it != wanted.end(); ++it)
    missing << it.key();
  QJsonObject snapshot{{"schema", "gpu-pro-driver-backup/1"},
                       {"vendor", plan["vendor"]},
                       {"restore", true},
                       {"complete", missing.isEmpty() && !files.isEmpty()},
                       {"targets", files},
                       {"missing", QJsonArray::fromStringList(missing)},
                       {"label", QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss") +
                                     " · " + plan["vendor"].toString()}};
  QString error;
  if (!saveJson(folder + "/snapshot.json", snapshot, &error))
    return fail(error);
  return snapshot;
}
QJsonArray savedDriverSnapshots(QString storage) {
  QJsonArray rows;
  QDir dir(storage);
  for (auto folder :
       dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed)) {
    auto o = QJsonDocument::fromJson(readText(dir.filePath(folder + "/snapshot.json")).toUtf8())
                 .object();
    if (o["schema"] == "gpu-pro-driver-backup/1")
      rows.append(o);
  }
  return rows;
}
