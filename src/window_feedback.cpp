#include "window.h"
QWidget *MainWindow::feedbackPage() {
  auto *root = page("Feedback & Diagnóstico");
  auto *l = qobject_cast<QVBoxLayout *>(root->layout());
  feedbackInfo = new QLabel;
  feedbackInfo->setWordWrap(true);
  l->addWidget(feedbackInfo);
  auto *explain = new QLabel(
      "Gere um arquivo para enviar manualmente ao desenvolvedor. Contém resultados, métodos, "
      "configurações, identificação técnica da GPU e telemetria dos testes. Não inclui "
      "UUID/serial, hostname, usuário, processos, argumentos de jogos ou logs brutos. Não há "
      "servidor, upload automático ou modelo neural treinado nesta versão.");
  explain->setWordWrap(true);
  l->addWidget(explain);
  auto *exportButton = new QPushButton("Gerar feedback · revisar e salvar JSON");
  auto *importButton = new QPushButton("Importar feedback de amigos");
  auto *references = new QPushButton("Consultar banco importado · não verificado");
  l->addWidget(references);
  connect(references, &QPushButton::clicked, this, [this] {
    QDialog dialog(this);
    dialog.resize(1150, 650);
    dialog.setWindowTitle("Referências importadas — não verificadas");
    auto *layout = new QVBoxLayout(&dialog);
    auto *info = new QLabel("Compare grupos com o mesmo método, API, resolução e cobertura. Estes "
                            "arquivos não são perfis homologados e não serão aplicados à GPU.");
    info->setWordWrap(true);
    layout->addWidget(info);
    auto rows = repository->references();
    auto *table = new QTableWidget(rows.size(), 7);
    table->setHorizontalHeaderLabels(
        {"GPU", "Driver", "Categoria", "Método", "API / resolução", "Score / estado", "Cobertura"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(table, 1);
    int index = 0;
    for (auto v : rows) {
      auto r = v.toObject();
      auto gpu = r["gpu"].toObject();
      QStringList values = {gpu["name"].toString(),
                            gpu["driverVersion"].toString(),
                            r["kind"].toString(),
                            r["method"].toString(),
                            r["api"].toString() + " " +
                                (r.contains("width") ? QString::number(r["width"].toInt()) + "×" +
                                                           QString::number(r["height"].toInt())
                                                     : ""),
                            r.contains("score")
                                ? QString::number(r["score"].toDouble(), 'f', 2)
                                : (r["valid"].toBool() ? "concluído" : "incompleto"),
                            r["coverage"].toString()};
      for (int col = 0; col < values.size(); ++col)
        table->setItem(index, col, new QTableWidgetItem(values[col]));
      ++index;
    }
    dialog.exec();
  });
  auto *diagnose = new QPushButton("Verificar dependências e sensores");
  l->addWidget(exportButton);
  l->addWidget(importButton);
  l->addWidget(diagnose);
  auto *description =
      new QLabel("Dados importados são referências não verificadas. O aplicativo não aplica "
                 "overclock recebido de outras pessoas. O banco SQLite mantém os testes locais; "
                 "cada página exibe os três mais recentes da GPU selecionada. Arquivos importados "
                 "são validados, sanitizados e deduplicados por ID.");
  description->setWordWrap(true);
  l->addWidget(description);
  l->addStretch();
  connect(exportButton, &QPushButton::clicked, this, &MainWindow::generateFeedback);
  connect(importButton, &QPushButton::clicked, this, &MainWindow::importFeedback);
  connect(diagnose, &QPushButton::clicked, this, [this] {
    QStringList lines;
    for (QString tool : {"gpu-bench-worker", "ffmpeg", "vainfo", "vulkaninfo", "glxinfo",
                         "mangohud", "gamescope", "pkexec"}) {
      QString path = tool == "gpu-bench-worker"
                         ? QCoreApplication::applicationDirPath() + "/" + tool
                         : QStandardPaths::findExecutable(tool);
      lines << tool + ": " + (QFileInfo::exists(path) ? "encontrado" : "ausente");
    }
    if (!offline) {
      Nvml nv;
      lines << "NVML: " + (nv.ready ? QString("inicializada") : nv.diagnostic);
      lines << "Dispositivo DRM da GPU: " + gpus.value(selected).render;
      lines << "Acesso render node: " + QString(QFileInfo(gpus.value(selected).render).isReadable()
                                                    ? "permitido"
                                                    : "ausente ou sem permissão");
    }
    for (auto key : {"temp", "hotspot", "memoryTemp", "fan", "fanRpm"})
      lines << QString(key) + ": " + displayMetric(latest, key);
    lines
        << "N/D pode significar recurso não exposto; instalar pacotes não cria sensores ausentes.";
    lines << "Distribuição: " + QSysInfo::prettyProductName() + "; kernel " +
                 QSysInfo::kernelVersion();
    QMessageBox::information(this, "Diagnóstico local", lines.join('\n'));
    log(lines.join('\n'));
  });
  if (offline) {
    exportButton->setEnabled(false);
    importButton->setEnabled(false);
  }
  return root;
}
void MainWindow::refreshFeedback() {
  if (!feedbackInfo || !repository)
    return;
  feedbackInfo->setText(
      QString("Banco local: %1 testes próprios · %2 registros importados (não verificados).\n%3")
          .arg(repository->count())
          .arg(repository->count("imported"))
          .arg(repository->error.isEmpty() ? "Tensão e power limit bloqueados para escrita."
                                           : "Banco indisponível: " + repository->error));
}
void MainWindow::generateFeedback() {
  if (offline)
    return;
  auto report = repository->feedback();
  QJsonArray hardware;
  for (const auto &gpu : gpus)
    hardware.append(publicGpu(gpu));
  report["gpus"] = hardware;
  report["kernel"] = QSysInfo::kernelVersion();
  report["sessionType"] = qEnvironmentVariable("XDG_SESSION_TYPE");
  QJsonObject dependencies;
  for (QString name : {"ffmpeg", "vainfo", "vulkaninfo", "glxinfo", "mangohud", "gamescope"})
    dependencies[name] = !QStandardPaths::findExecutable(name).isEmpty();
  dependencies["systemWorker"]=QFileInfo(QCoreApplication::applicationDirPath()+"/gpu-system-worker").isExecutable();
  dependencies["graphicsWorker"] =
      QFileInfo::exists(QCoreApplication::applicationDirPath() + "/gpu-bench-worker");
  report["dependencies"] = dependencies;
  auto bytes = QJsonDocument(report).toJson();
  bool truncated = false;
  auto records = report["records"].toArray();
  while (bytes.size() > 16 * 1024 * 1024 && !records.isEmpty()) {
    records.removeLast();
    report["records"] = records;
    report["truncated"] = true;
    truncated = true;
    bytes = QJsonDocument(report).toJson();
  }
  if (truncated)
    notify("Feedback limitado a 16 MiB; registros antigos permaneceram no banco");
  QDialog review(this);
  review.setWindowTitle("Revisar feedback antes de salvar");
  review.resize(850, 650);
  auto *l = new QVBoxLayout(&review);
  auto *summary = new QLabel(QString("%1 registros · %2 KiB · nenhum envio automático")
                                 .arg(records.size())
                                 .arg(bytes.size() / 1024));
  l->addWidget(summary);
  auto *preview = new QPlainTextEdit;
  preview->setReadOnly(true);
  preview->setPlainText(QString::fromUtf8(bytes));
  l->addWidget(preview, 1);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  l->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &review, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &review, &QDialog::reject);
  if (review.exec() != QDialog::Accepted)
    return;
  auto path = QFileDialog::getSaveFileName(
      this, "Salvar feedback",
      QDir::homePath() + "/gpu-feedback-" +
          QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".json",
      "Feedback JSON (*.json)");
  if (path.isEmpty())
    return;
  QString error;
  if (!saveJson(path, report, &error))
    notify("Falha ao salvar feedback", error);
  else
    notify("Feedback salvo para envio manual", path);
}
void MainWindow::importFeedback() {
  if (offline)
    return;
  auto path = QFileDialog::getOpenFileName(this, "Importar feedback", {}, "Feedback JSON (*.json)");
  if (path.isEmpty())
    return;
  QFile f(path);
  if (f.size() > 16 * 1024 * 1024 || !f.open(QIODevice::ReadOnly)) {
    notify("Arquivo maior que 16 MiB ou ilegível");
    return;
  }
  QJsonParseError parse;
  auto doc = QJsonDocument::fromJson(f.readAll(), &parse);
  if (parse.error != QJsonParseError::NoError) {
    notify("JSON inválido", parse.errorString());
    return;
  }
  QString error;
  int count = repository->importFeedback(doc.object(), error);
  if (count < 0)
    notify("Feedback recusado", error);
  else
    notify(QString("%1 novos registros importados como não verificados").arg(count));
  refreshFeedback();
}

bool MainWindow::viewReport(QString path) {
  if (!offline)
    return false;
  QFile file(path);
  if (file.size() > 16 * 1024 * 1024 || !file.open(QIODevice::ReadOnly))
    return false;
  auto r = QJsonDocument::fromJson(file.readAll()).object();
  QString kind = r["kind"].toString();
  if (!panels.contains(kind) || !r["valid"].isBool())
    return false;
  auto meta = r["gpu"].toObject();
  Gpu gpu;
  gpu.name = meta["name"].toString("GPU do resultado salvo");
  gpu.vendor = meta["vendor"].toString();
  gpu.deviceId = meta["deviceId"].toString();
  gpu.pci = "saved-report";
  gpu.specs = meta;
  setInventory({gpu});
  if (!r["previewFile"].toString().isEmpty() && QFileInfo(r["previewFile"].toString()).isRelative())
    r["previewFile"] = QFileInfo(path).dir().absoluteFilePath(r["previewFile"].toString());
  repository->add(kind, deviceKey(gpu), r);
  if (r["telemetry"].isArray() && !r["telemetry"].toArray().isEmpty())
    updateSnapshot(r["telemetry"].toArray().last().toObject());
  refreshHistories();
  refreshSpecs();
  lastResult = r;
  activeKind = kind;
  status->setText("VISUALIZAÇÃO OFFLINE · resultado salvo, sem acesso à GPU");
  return true;
}
bool MainWindow::exportViewedImage(QString path) {
  return offline && !lastResult.isEmpty() &&
         renderReport(lastResult["kind"].toString(), lastResult).save(path);
}
bool MainWindow::exportViewedFeedback(QString path) {
  if (!offline)
    return false;
  QString error;
  return saveJson(path, repository->feedback(), &error);
}
