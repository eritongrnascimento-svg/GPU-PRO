#include "driver_packages.h"
#include "driver_stack.h"
#include "window.h"
void MainWindow::startNativeTransaction(QStringList command, std::function<void(int)> done) {
  if (offline || testActive || driverBusy || displayBusy ||
      clockProcess.state() != QProcess::NotRunning || command.isEmpty())
    return;
  if (!QStringList{"/usr/bin/pacman", "/usr/bin/apt-get", "/usr/bin/dnf", "/usr/bin/pkcon"}
           .contains(command.first()))
    return;
  QString terminal;
  QStringList args;
  for (auto item :
       QList<QPair<QString, QStringList>>{{"/usr/bin/konsole", {"--separate", "--nofork", "-e"}},
                                          {"/usr/bin/xterm", {"-e"}},
                                          {"/usr/bin/gnome-terminal", {"--wait", "--"}},
                                          {"/usr/bin/xfce4-terminal", {"--disable-server", "-x"}}})
    if (QFileInfo(item.first).isExecutable()) {
      terminal = item.first;
      args = item.second;
      break;
    }
  if (terminal.isEmpty() || !QFileInfo("/usr/bin/pkexec").isExecutable()) {
    notify("É necessário terminal compatível e pkexec", command.join(' '));
    return;
  }
  args << "/usr/bin/pkexec" << "--disable-internal-agent";
  args << command;
  auto *process = new QProcess(this);
  process->setProcessEnvironment(nativeEnvironment());
  driverBusy = true;
  activeKind = "package-maintenance";
  setTesting(true);
  connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, process, done](int code, QProcess::ExitStatus state) {
            driverBusy = false;
            setTesting(false);
            process->deleteLater();
            done(state == QProcess::NormalExit ? code : -1);
          });
  connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      driverBusy = false;
      setTesting(false);
      notify("Não foi possível abrir o gerenciador", process->errorString());
      process->deleteLater();
    }
  });
  process->start(terminal, args);
}
QWidget *MainWindow::driversControls() {
  auto *box = new QGroupBox("Drivers completos · repositórios e restauração");
  auto *layout = new QVBoxLayout(box);
  auto *info = new QLabel(
      "Consulte as versões realmente disponíveis nos repositórios configurados. Open refere-se ao "
      "módulo de kernel NVIDIA; componentes de espaço de usuário mantêm suas licenças.");
  info->setWordWrap(true);
  layout->addWidget(info);
  auto *refresh = new QPushButton("Consultar drivers completos");
  layout->addWidget(refresh);
  auto *available = new QComboBox, *installed = new QComboBox;
  available->setObjectName("completeDriverChoices");
  for (auto *combo : {available, installed}) {
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(28);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }
  installed->setObjectName("installedDriverOnly");
  auto *form = new QFormLayout;
  form->addRow("Drivers disponíveis · antiga → nova", available);
  form->addRow("Somente driver instalado", installed);
  layout->addLayout(form);
  auto *preview = new QPushButton("Simular instalação / mostrar alterações"),
       *install = new QPushButton("Instalar versão selecionada"),
       *rollback = new QPushButton("Restaurar versão anterior do driver");
  layout->addWidget(preview);
  layout->addWidget(install);
  layout->addWidget(rollback);
  install->setEnabled(false);
  preview->setEnabled(false);
  auto *output = new QPlainTextEdit;
  output->setReadOnly(true);
  output->setMinimumHeight(150);
  output->setMaximumHeight(280);
  output->document()->setMaximumBlockCount(2000);
  layout->addWidget(output);
  auto *note = new QLabel(
      "A lista não mistura ferramentas de contêiner ou componentes avulsos. Uma troca pode exigir "
      "reinicialização. No Arch/CachyOS, a prévia inclui atualização completa usando os mesmos "
      "índices locais; não sincroniza índices depois da simulação. A restauração depende de backup "
      "completo e dos pacotes anteriores disponíveis, nunca de um número de versão isolado.");
  note->setWordWrap(true);
  layout->addWidget(note);
  auto candidate = std::make_shared<QJsonObject>(), plan = std::make_shared<QJsonObject>(),
       installedDriver = std::make_shared<QJsonObject>();
  auto queryDevice = std::make_shared<QString>();
  auto worker = new QProcess(this);
  auto fallback = new DriverCatalog(this);
  auto planFile = std::make_shared<std::unique_ptr<QTemporaryFile>>();
  auto operation = std::make_shared<QString>();
  auto resultBytes = std::make_shared<QByteArray>();
  auto limit = new QTimer(worker);
  limit->setSingleShot(true);
  limit->setInterval(60000);
  connect(limit, &QTimer::timeout, worker, &QProcess::kill);
  connect(worker, &QProcess::readyReadStandardOutput, box, [worker, resultBytes] {
    *resultBytes += worker->readAllStandardOutput();
    if (resultBytes->size() > 8 * 1024 * 1024)
      worker->kill();
  });
  auto populate = [this, available, installed, info, preview, candidate, plan, install,
                   installedDriver](QJsonArray choices, QJsonArray current, QString message) {
    QSignalBlocker b(available);
    available->clear();
    installed->clear();
    *plan = {};
    install->setEnabled(false);
    for (auto v : choices) {
      auto row = v.toObject();
      QString architecture = gpus.value(selected).specs["architecture"].toString(),
              name = row["name"].toString();
      if ((architecture == "Ampere" || architecture == "Ada Lovelace" ||
           architecture == "Blackwell") &&
          (name.contains("390xx") || name.contains("470xx")))
        continue;
      if ((architecture == "Maxwell" || architecture == "Pascal" || architecture == "Kepler" ||
           architecture == "Volta") &&
          row["variant"] == "Open")
        continue;
      available->addItem(row["label"].toString(), row);
      available->setItemData(available->count() - 1, row["label"].toString(), Qt::ToolTipRole);
    }
    QString currentVersion, variant;
    QStringList moduleNames;
    *installedDriver = {};
    for (auto v : current) {
      auto row = v.toObject();
      QString name = row["name"].toString();
      if (QRegularExpression("^(?:nvidia(?:-[0-9]+xx)?-utils|nvidia-driver-[0-9]+(?:-server)?(?:-"
                             "open)?|akmod-nvidia(?:-open)?|mesa|Mesa)$")
              .match(name)
              .hasMatch()) {
        currentVersion = row["version"].toString();
        *installedDriver = row;
      }
      if (QRegularExpression("^(?:nvidia(?:-open)?(?:-dkms|-lts)?|nvidia-[0-9]+xx(?:-open)?-dkms|"
                             "linux(?:-[a-z0-9]+)*-nvidia(?:-open)?)$")
              .match(name)
              .hasMatch()) {
        moduleNames << name;
        variant = name.contains("open") ? "Open" : "Proprietário";
      }
    }
    if (!currentVersion.isEmpty())
      installed->addItem(gpus.value(selected).vendor + " " + currentVersion +
                         (variant.isEmpty() ? QString{} : " · " + variant));
    else
      installed->addItem("Versão do pacote principal não identificada");
    installed->setToolTip(moduleNames.join('\n'));
    auto loaded = gpus.value(selected).specs["driverVersion"].toString();
    if (!loaded.isEmpty())
      installed->setToolTip(installed->toolTip() + "\nCarregado nesta sessão: " + loaded);
    *candidate = available->currentData().toJsonObject();
    preview->setEnabled(available->count() > 0 && !offline);
    info->setText(message);
  };
  connect(available, qOverload<int>(&QComboBox::currentIndexChanged), box,
          [candidate, plan, available, install, output] {
            *candidate = available->currentData().toJsonObject();
            *plan = {};
            install->setEnabled(false);
            output->setPlainText(
                "Simule para conferir pacotes adicionados, atualizados e removidos.");
          });
  connect(fallback, &DriverCatalog::listed, box,
          [this, populate, refresh, queryDevice](QJsonArray rows, QString message) {
            if (*queryDevice != gpus.value(selected).pci) {
              refresh->setEnabled(!offline);
              return;
            }
            QJsonArray current;
            for (auto v : rows) {
              auto row = v.toObject();
              if (!row["installed"].toString().isEmpty()) {
                row["version"] = row["installed"];
                current.append(row);
              }
            }
            populate(completeDriverProfiles(rows, current, gpus.value(selected).vendor), current,
                     message);
            refresh->setEnabled(!offline);
          });
  auto query = [this, worker, refresh, install, preview, info, operation, resultBytes, fallback,
                limit, queryDevice] {
    if (offline || testActive || driverBusy || worker->state() != QProcess::NotRunning)
      return;
    *queryDevice = gpus.value(selected).pci;
    install->setEnabled(false);
    preview->setEnabled(false);
    refresh->setEnabled(false);
    info->setText("Consultando drivers completos e pacotes instalados…");
    if (nativePackageManager() != "pacman") {
      fallback->refresh(gpus.value(selected).vendor);
      return;
    }
    *operation = "catalog";
    resultBytes->clear();
    worker->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    worker->start(QCoreApplication::applicationDirPath() + "/gpu-system-worker",
                  {"--driver-catalog", gpus.value(selected).vendor});
    limit->start();
  };
  connect(refresh, &QPushButton::clicked, this, query);
  auto simulate = [this, worker, operation, resultBytes, planFile, candidate, plan, install, output,
                   limit, queryDevice] {
    if (offline || testActive || driverBusy || candidate->isEmpty() ||
        worker->state() != QProcess::NotRunning)
      return;
    *queryDevice = gpus.value(selected).pci;
    *plan = {};
    install->setEnabled(false);
    if (nativePackageManager() != "pacman") {
      auto cmd = driverTransaction(*candidate, true);
      if (cmd.isEmpty()) {
        output->setPlainText("Sem prévia nativa compatível para este conjunto");
        return;
      }
      *operation = "native-preview";
      resultBytes->clear();
      auto env = nativeEnvironment();
      env.insert("LC_ALL", "C");
      worker->setProcessEnvironment(env);
      QString exe = cmd.takeFirst();
      worker->setProcessChannelMode(QProcess::MergedChannels);
      worker->start(exe, cmd);
      limit->start();
      return;
    }
    *planFile = std::make_unique<QTemporaryFile>();
    if (!(*planFile)->open())
      return;
    (*planFile)->write(QJsonDocument(*candidate).toJson());
    (*planFile)->flush();
    *operation = "plan";
    resultBytes->clear();
    worker->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    worker->start(QCoreApplication::applicationDirPath() + "/gpu-system-worker",
                  {"--driver-plan", (*planFile)->fileName()});
    limit->start();
    output->setPlainText("Preparando transação no resolvedor nativo, sem instalar…");
  };
  connect(preview, &QPushButton::clicked, this, simulate);
  connect(
      worker, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), box,
      [this, worker, operation, resultBytes, plan, populate, refresh, install, output, candidate,
       limit, queryDevice](int code, QProcess::ExitStatus state) {
        limit->stop();
        *resultBytes += worker->readAllStandardOutput();
        if (*queryDevice != gpus.value(selected).pci) {
          refresh->setEnabled(!offline);
          return;
        }
        if (*operation == "native-preview") {
          QString text = QString::fromUtf8(*resultBytes);
          bool ok =
              state == QProcess::NormalExit &&
              (code == 0 || (code == 1 && text.contains("Operation aborted", Qt::CaseInsensitive)));
          *plan = {{"valid", ok}, {"candidate", *candidate}, {"nativeText", text}};
          output->setPlainText(
              text + "\nRevise as linhas de adição/remoção e a confirmação final do gerenciador. "
                     "Backups transacionais ainda dependem do backend da distribuição.");
          install->setEnabled(ok);
          return;
        }
        auto r = QJsonDocument::fromJson(*resultBytes).object();
        if (!r["valid"].toBool() || code != 0 || state != QProcess::NormalExit) {
          output->setPlainText(r["error"].toString("Consulta não concluída: ") +
                               QString::fromUtf8(worker->readAllStandardError()).left(1000));
          refresh->setEnabled(!offline);
          return;
        }
        if (*operation == "catalog") {
          populate(r["profiles"].toArray(), r["installed"].toArray(),
                   QString("%1 conjuntos completos disponíveis; somente versões presentes nos "
                           "repositórios atuais.")
                       .arg(r["profiles"].toArray().size()));
          refresh->setEnabled(!offline);
        } else {
          *plan = r;
          output->setPlainText(driverPlanDescription(r));
          install->setText(candidate->value("restore").toBool() ? "Restaurar conjunto selecionado"
                                                                : "Instalar versão selecionada");
          install->setEnabled(!offline);
        }
      });
  connect(worker, &QProcess::errorOccurred, box,
          [worker, refresh, output, limit](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
              limit->stop();
              refresh->setEnabled(true);
              output->setPlainText("Worker ou gerenciador indisponível: " + worker->errorString());
            }
          });
  connect(rollback, &QPushButton::clicked, this, [this, candidate, plan, simulate, output] {
    if (offline || driverBusy || testActive)
      return;
    QJsonArray choices;
    QStringList labels;
    for (auto v : savedDriverSnapshots(configRoot() + "/driver-backups")) {
      auto s = v.toObject();
      if ((s["complete"].toBool() || s["repositoryReference"].toBool()) &&
          s["vendor"] == gpus.value(selected).vendor) {
        choices.append(s);
        labels << s["label"].toString();
      }
    }
    if (choices.isEmpty()) {
      output->setPlainText(
          "Ainda não há backup completo desta GPU. Antes de uma troca pelo painel, o aplicativo "
          "registra a instalação e preserva os pacotes anteriores presentes no cache. Backups "
          "incompletos não habilitam restauração.");
      return;
    }
    bool ok = false;
    auto label = QInputDialog::getItem(this, "Restaurar driver anterior",
                                       "Conjunto anterior preservado", labels, 0, false, &ok);
    if (!ok)
      return;
    *candidate = choices[labels.indexOf(label)].toObject();
    *plan = {};
    simulate();
  });
  connect(
      install, &QPushButton::clicked, this,
      [this, candidate, plan, output, install, query, installedDriver] {
        if (offline || testActive || driverBusy || !plan->value("valid").toBool() ||
            plan->value("candidate").toObject() != *candidate)
          return;
        QStringList command;
        if (nativePackageManager() == "pacman") {
          auto checked = nativeDriverPlan(*candidate);
          if (!checked["valid"].toBool() || checked["add"] != plan->value("add") ||
              checked["remove"] != plan->value("remove")) {
            output->appendPlainText("Os pacotes mudaram. Simule novamente antes de instalar.");
            install->setEnabled(false);
            return;
          }
          if (!candidate->value("restore").toBool()) {
            QString error;
            auto snapshot = saveDriverSnapshot(checked, configRoot() + "/driver-backups", &error);
            if (!snapshot["complete"].toBool() &&
                QMessageBox::warning(
                    this, "Backup de driver incompleto",
                    "Não foi possível preservar todos os pacotes anteriores. A restauração "
                    "automática desta troca não estará disponível.\n" +
                        error + "\nPacotes ausentes: " +
                        QString::fromUtf8(QJsonDocument(snapshot["missing"].toArray()).toJson()) +
                        "\nContinuar mesmo assim?",
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
              return;
          }
          command << checked["program"].toString();
          for (auto v : checked["arguments"].toArray())
            command << v.toString();
        } else {
          command = driverTransaction(*candidate, false);
          if (!candidate->value("restore").toBool() && !installedDriver->isEmpty()) {
            auto previous = *installedDriver;
            previous["schema"] = "gpu-pro-driver-backup/1";
            previous["restore"] = true;
            previous["repositoryReference"] = true;
            previous["vendor"] = gpus.value(selected).vendor;
            previous["label"] = QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss") +
                                " · " + gpus.value(selected).vendor + " " +
                                previous["version"].toString() +
                                " · exige disponibilidade no repositório";
            previous["complete"] = false;
            QString e;
            saveJson(configRoot() + "/driver-backups/" +
                         QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz") +
                         "/snapshot.json",
                     previous, &e);
          }
        }
        if (command.isEmpty())
          return;
        if (QMessageBox::question(this, "Confirmar troca do driver",
                                  output->toPlainText().left(14000) +
                                      "\n\nAbrir autenticação e gerenciador nativo para confirmar "
                                      "esta transação? Pode exigir reinicialização.",
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) != QMessageBox::Yes)
          return;
        install->setEnabled(false);
        startNativeTransaction(command, [this, output, query](int code) {
          output->appendPlainText(
              QString("Terminal encerrado (%1). Confira o gerenciador e reinicie quando "
                      "necessário; o driver carregado não é trocado ao vivo.")
                  .arg(code));
          query();
        });
      });
  connect(this, &MainWindow::selectGpu, box,
          [candidate, plan, available, installed, install, preview] {
            *candidate = {};
            *plan = {};
            available->clear();
            installed->clear();
            install->setEnabled(false);
            preview->setEnabled(false);
          });
  if (offline) {
    refresh->setEnabled(false);
    rollback->setEnabled(false);
  }
  return box;
}
