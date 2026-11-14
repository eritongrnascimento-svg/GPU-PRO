#include "window.h"
#include <cmath>
#include <sys/stat.h>
namespace {
QLabel *text(QString s, int size = 12) {
  auto *l = new QLabel(s);
  l->setWordWrap(true);
  l->setStyleSheet(QString("font-size:%1px;").arg(size));
  l->setTextFormat(Qt::PlainText);
  return l;
}
QPushButton *action(QString s, QBoxLayout *layout) {
  auto *b = new QPushButton(s);
  layout->addWidget(b);
  return b;
}
QString helperPath() { return installedClockHelper(); }
bool helperReady() { return rootHelperTrusted(helperPath()); }
double meanScene(const QJsonObject &r, QString key) {
  auto a = r["sceneResults"].toArray();
  if (a.isEmpty())
    return 0;
  double n = 0;
  for (auto v : a)
    n += v.toObject()[key].toDouble();
  return n / a.size();
}
QString stateLabel(QString state) {
  return state == "passed"        ? "concluído"
         : state == "unsupported" ? "não suportado"
         : state == "cancelled"   ? "cancelado"
                                  : "falha";
}
QString codecLabel(QString codec) {
  return codec == "h264" ? "H.264" : codec == "hevc" ? "H.265/HEVC" : "AV1";
}
QString modeLabel(QString mode) { return mode == "encode" ? "encode" : "decode"; }
} // namespace
MainWindow::TestPanel MainWindow::makeTestPanel(QString title, QString kind) {
  TestPanel p;
  p.root = page(title);
  auto *layout = qobject_cast<QVBoxLayout *>(p.root->layout());
  p.plot = new HistoryPlot;
  p.plot->setMinimumHeight(130);
  auto *renderRow = new QHBoxLayout;
  renderRow->addWidget(p.plot, 1);
  p.preview = text("A cena 3D será exibida aqui durante o teste");
  p.preview->setAlignment(Qt::AlignCenter);
  p.preview->setMinimumSize(640, 360);
  p.preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(p.preview);
  layout->addLayout(renderRow);
  if (kind != "graphics" && kind != "bottleneck") {
    p.plot->hide();
    p.preview->hide();
  }

  p.progress = new QProgressBar;
  layout->addWidget(p.progress);
  auto *card = new QGroupBox("Resultado · " + kind);
  auto *cardLayout = new QVBoxLayout(card);
  p.result = text("Aguardando execução", 17);
  p.result->setTextInteractionFlags(Qt::TextSelectableByMouse);
  cardLayout->addWidget(p.result);
  layout->addWidget(card);
  p.history = new QListWidget;
  p.history->setMaximumHeight(105);
  p.history->setMinimumHeight(80);
  layout->addWidget(text("Histórico local · três últimos testes desta GPU"));
  layout->addWidget(p.history);
  auto *buttons = new QHBoxLayout;
  auto *image = action("Salvar teste em PNG/JPEG", buttons);
  auto *data = action("Salvar JSON", buttons);
  layout->addLayout(buttons);
  connect(image, &QPushButton::clicked, this, [this, kind] { exportResultImage(kind); });
  connect(data, &QPushButton::clicked, this, [this, kind] {
    auto rows = repository->recent(kind, deviceKey(gpus.value(selected)));
    int index = panels[kind].history->currentRow();
    if (index < 0)
      index = 0;
    if (rows.isEmpty())
      return;
    auto path = QFileDialog::getSaveFileName(this, "Salvar teste", {}, "JSON (*.json)");
    if (path.isEmpty())
      return;
    QString error;
    if (!saveJson(path, rows[index].toObject(), &error))
      notify("Falha ao salvar", error);
  });
  connect(p.history, &QListWidget::currentRowChanged, this, [this, kind](int row) {
    if (row < 0 || testActive)
      return;
    auto records = repository->recent(kind, deviceKey(gpus.value(selected)));
    if (row < records.size())
      presentRecord(kind, records[row].toObject());
  });
  if (offline) {
    image->setEnabled(false);
    data->setEnabled(false);
  }
  return p;
}
QWidget *MainWindow::benchmarks() {
  auto p = makeTestPanel("Benchmark GPU / 3D / Jogos", "graphics");
  auto *l = qobject_cast<QVBoxLayout *>(p.root->layout());
  auto *controls = new QHBoxLayout;
  graphicsApi = new QComboBox;
  graphicsApi->addItems({"Vulkan", "OpenGL"});
  graphicsResolution = new QComboBox;
  for (auto size :
       QList<QSize>{{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}, {7680, 4320}})
    graphicsResolution->addItem(QString("%1 × %2").arg(size.width()).arg(size.height()), size);
  graphicsDuration = new QComboBox;
  graphicsDuration->addItem("Padrão · 30 s", 30);
  graphicsDuration->addItem("Completo · 60 s", 60);
  controls->addWidget(graphicsApi);
  controls->addWidget(graphicsResolution);
  controls->addWidget(graphicsDuration);
  p.start = action("Iniciar 3D", controls);
  p.cancel = action("Cancelar", controls);
  p.cancel->setEnabled(false);
  l->insertLayout(1, controls);
  l->addWidget(text("Renderização offscreen na resolução física escolhida, mesmo em monitor menor. "
                    "Pontos = média geométrica do throughput das três cenas (MPix/s). Compare "
                    "mesma API, resolução e método; não equivale a FPS de jogos comerciais. "
                    "Interrompe em GPU ≥78 °C ou hotspot ≥95 °C."));
  connect(p.start, &QPushButton::clicked, this, &MainWindow::runBenchmark);
  connect(p.cancel, &QPushButton::clicked, this, [this] {
    if (trialActive) {
      trialCancelled = true;
      trialGraphics = {{"valid", false}, {"errorCode", "USER_CANCELLED"}};
      clockProcess.write("STOP\n");
    }
    graphicsProcess.kill();
  });
  panels["graphics"] = p;
  if (offline)
    p.start->setEnabled(false);
  auto *tabs = benchmarkTabs = new QTabWidget;
  tabs->addTab(scrollPage(p.root), "Cenas 3D / jogos");
  auto compute = makeTestPanel("Computação GPU · memória e operações", "compute");
  auto *cl = qobject_cast<QVBoxLayout *>(compute.root->layout());
  auto *actions = new QHBoxLayout;
  compute.start = action("Medir memória, FP32/64 e inteiros", actions);
  compute.cancel = action("Cancelar", actions);
  compute.cancel->setEnabled(false);
  cl->insertLayout(1, actions);
  cl->addWidget(text(
      "Vulkan compute: 32 MiB por buffer, redução para leitura, escrita, cópia e quatro "
      "acumuladores FMA. Valores são throughput efetivo deste kernel; não são pico teórico. "
      "FP64/inteiros 64 dependem de features. AES, SHA e fractais não são medidos nesta versão."));
  connect(compute.start, &QPushButton::clicked, this, [this] {
    if (!testActive && !offline)
      startGraphics("compute", "Vulkan", 1280, 720, 3);
  });
  connect(compute.cancel, &QPushButton::clicked, this, [this] { graphicsProcess.kill(); });
  if (offline)
    compute.start->setEnabled(false);
  panels["compute"] = compute;
  tabs->addTab(scrollPage(compute.root), "Memória / FLOPS / inteiros");
  tabs->addTab(scrollPage(videoPage()), "Encoder / Decoder");
  tabs->addTab(scrollPage(bottleneckPage()), "Gargalo");
  return tabs;
}
QWidget *MainWindow::videoPage() {
  auto p = makeTestPanel("Encoder + Decoder · suíte sequencial", "video");
  auto *l = qobject_cast<QVBoxLayout *>(p.root->layout());
  auto *actions = new QHBoxLayout;
  p.start = action("Testar H.264 → H.265 → AV1", actions);
  p.cancel = action("Cancelar suíte", actions);
  p.cancel->setEnabled(false);
  l->insertLayout(1, actions);
  l->addWidget(text("Seis etapas: encode + decode de cada codec, em 1080p/8-bit. Preparação fora "
                    "da medição. Sem fallback CPU na fase medida. Codecs incompatíveis são "
                    "identificados; falhas de execução invalidam a pontuação consolidada. Compare "
                    "somente a mesma cobertura de codecs e o mesmo método."));
  connect(p.start, &QPushButton::clicked, this, [this] {
    if (testActive || offline || selected >= gpus.size())
      return;
    activeKind = "video";
    setTesting(true);
    codecSuite.start(gpus[selected]);
  });
  connect(p.cancel, &QPushButton::clicked, &codecSuite, &CodecSuite::cancel);
  panels["video"] = p;
  if (offline)
    p.start->setEnabled(false);
  return p.root;
}
QWidget *MainWindow::bottleneckPage() {
  auto p = makeTestPanel("Teste de gargalo · estimativa da carga sintética", "bottleneck");
  auto *l = qobject_cast<QVBoxLayout *>(p.root->layout());
  auto *controls = new QHBoxLayout;
  bottleneckResolution = new QComboBox;
  for (auto size : QList<QSize>{{1280,720},{1920,1080},{2560,1440},{3840,2160},{7680,4320}})
    bottleneckResolution->addItem(QString("%1 × %2").arg(size.width()).arg(size.height()),size);
  bottleneckResolution->setCurrentIndex(2);
  controls->addWidget(text("Resolução alvo")); controls->addWidget(bottleneckResolution);
  p.start = action("Aferir gargalo", controls);
  p.cancel = action("Cancelar", controls);
  p.cancel->setEnabled(false);
  l->insertLayout(1, controls);
  l->addWidget(text("Vulkan: mesma cena e 150 mil transformações CPU por quadro, em duas "
                    "resoluções: referência 720p e alvo escolhido. Estima o limite de CPU em relação à capacidade "
                    "da GPU e o limite de GPU em relação à CPU. Modelo: tempo de quadro = máximo(CPU, GPU), "
                    "com sobreposição ideal. Os percentuais não são perda real medida em jogos; a carga CPU usa uma thread."));
  connect(p.start, &QPushButton::clicked, this, [this] {
    if (testActive || offline || selected >= gpus.size())
      return;
    bottleneckTarget = bottleneckResolution->currentData().toSize();
    bottleneckStage = 0;
    bottleneckStages = {};
    startGraphics("bottleneck", "Vulkan", 1280, 720, 12, 150000);
  });
  connect(p.cancel, &QPushButton::clicked, this, [this] { graphicsProcess.kill(); });
  panels["bottleneck"] = p;
  if (offline)
    p.start->setEnabled(false);
  return p.root;
}
void MainWindow::setTesting(bool active) {
  testActive = active;
  gpuSelect->setEnabled(!active);
  if (bottleneckResolution) bottleneckResolution->setEnabled(!active);
  if (graphicsApi) {
    graphicsApi->setEnabled(!active);
    graphicsResolution->setEnabled(!active);
    graphicsDuration->setEnabled(!active);
  }
  for (auto it = panels.begin(); it != panels.end(); ++it) {
    if (it->start)
      it->start->setEnabled(!active && !offline);
    if (it->cancel)
      it->cancel->setEnabled(active && (it.key() == activeKind ||
                                        (activeKind == "clock-trial" && it.key() == "graphics")));
  }
  if (stopTrial)
    stopTrial->setEnabled(active && trialActive);
  if (trialButton)
    trialButton->setEnabled(!active && !offline && helperReady() &&
                            gpus.value(selected).vendor == "NVIDIA");
  if (active) {
    for (auto *input : tuningInputs) input->setEnabled(false);
    for (auto *button : tuningButtons) button->setEnabled(false);
    if (automaticFans) automaticFans->setEnabled(false);
    if (restoreTuningButton) restoreTuningButton->setEnabled(false);
    telemetryHistory = {};
    applyCore->setEnabled(false);
    applyMem->setEnabled(false);
  }
}
void MainWindow::runBenchmark() {
  if (testActive || offline || selected >= gpus.size())
    return;
  QSize s = graphicsResolution->currentData().toSize();
  startGraphics("graphics", graphicsApi->currentText(), s.width(), s.height(),
                graphicsDuration->currentData().toInt());
}
void MainWindow::startGraphics(QString kind, QString api, int w, int h, int seconds,
                               int iterations) {
  if (offline || selected >= gpus.size())
    return;
  activeKind = kind;
  setTesting(true);
  auto &panel = panels[kind == "clock-trial" ? "graphics" : kind];
  panel.plot->clear();
  panel.progress->setValue(0);
  panel.result->setText("Preparando contexto " + api + " · " + QString::number(w) + " × " +
                        QString::number(h));
  graphicsFiles = std::make_unique<QTemporaryDir>();
  liveFrameCount = 0;
  graphicsOutput.clear();
  graphicsResult = {};
  QString worker = QCoreApplication::applicationDirPath() + "/gpu-bench-worker";
  auto env = QProcessEnvironment::systemEnvironment();
  const auto &gpu = gpus[selected];
  if (api == "OpenGL") {
    if (gpu.vendor == "AMD" || gpu.vendor == "Intel") {
      QString pci = gpu.pci;
      pci.replace(':', '_');
      pci.replace('.', '_');
      env.insert("DRI_PRIME", "pci-" + pci);
    } else if (gpu.vendor == "NVIDIA") {
      env.insert("__NV_PRIME_RENDER_OFFLOAD", "1");
      env.insert("__GLX_VENDOR_LIBRARY_NAME", "nvidia");
    }
  }
  graphicsProcess.setProcessEnvironment(env);
  graphicsProcess.setProgram(worker);
  graphicsProcess.setArguments(
      {"--pci", gpu.pci, "--api", api, "--width", QString::number(w), "--height",
       QString::number(h), "--seconds", QString::number(seconds), "--cpu-iterations",
       QString::number(iterations), "--preview", graphicsFiles->filePath("scene.png")});
  if (kind == "compute") {
    auto args = graphicsProcess.arguments();
    args << "--compute";
    graphicsProcess.setArguments(args);
  }
  if (kind == "graphics" || kind == "bottleneck") {
    auto args = graphicsProcess.arguments();
    args << "--live-preview" << graphicsFiles->filePath("live.jpg");
    graphicsProcess.setArguments(args);
  }
  graphicsWatchdog.start(30000);
  graphicsProcess.start();
}
void MainWindow::handleGraphics(QJsonObject r) {
  if (closing)
    return;
  r["kind"] = activeKind;
  if (trialActive) {
    if (trialCancelled && trialBaseline.isEmpty()) {
      trialLease = {{"valid", false},
                    {"restored", true},
                    {"error", "Cancelado no baseline; nenhum clock aplicado"}};
      finishClockTrial();
      return;
    }
    if (trialBaseline.isEmpty()) {
      trialBaseline = r;
      if (!r["valid"].toBool()) {
        trialLease = {{"valid", false},
                      {"restored", true},
                      {"error", "Baseline não concluído; nenhum ajuste solicitado"}};
        finishClockTrial();
        return;
      }
      clockInfo->setText(
          "Baseline concluído; aguardando GPU ociosa e abaixo de 60 °C antes do teste temporário");
      auto *cooldown = new QTimer(this);
      cooldown->setInterval(500);
      auto tries = std::make_shared<int>(0);
      connect(cooldown, &QTimer::timeout, this, [this, cooldown, tries] {
        if (!trialActive) {
          cooldown->stop();
          cooldown->deleteLater();
          return;
        }
        if (trialCancelled || ++*tries >= 30) {
          cooldown->stop();
          cooldown->deleteLater();
          trialLease = {
              {"valid", false},
              {"restored", true},
              {"error", "Cancelado ou sem condições térmicas/ociosidade; nenhum clock aplicado"}};
          finishClockTrial();
          return;
        }
        if (latest["temp"].isDouble() && latest["temp"].toDouble() < 60 &&
            latest["usage"].isDouble() && latest["usage"].toDouble() <= 40) {
          cooldown->stop();
          cooldown->deleteLater();
          int current = latest["coreCurrent"].toInt();
          clockOutput.clear();
          clockProcess.start("pkexec",
                             {"--disable-internal-agent", helperPath(), gpus[selected].pci,
                              "trial-core", QString::number(current + 15),
                              QString::number(QCoreApplication::applicationPid())});
        }
      });
      cooldown->start();
      return;
    }
    trialGraphics = r;
    trialRestoring = true;
    if (clockProcess.state() != QProcess::NotRunning)
      clockProcess.write("STOP\n");
    else
      finishClockTrial();
    return;
  }
  if (activeKind == "bottleneck" && r["valid"].toBool()) {
    bottleneckStages.append(r);
    if (bottleneckStage++ == 0) {
      startGraphics("bottleneck", "Vulkan", bottleneckTarget.width(), bottleneckTarget.height(), 12, 150000);
      return;
    }
    r = estimateBottleneck(bottleneckStages[0].toObject(), bottleneckStages[1].toObject());
  }
  finishTest(r);
}
void MainWindow::finishTest(QJsonObject r) {
  QString kind = r["kind"].toString(activeKind);
  if (!panels.contains(kind))
    kind = "graphics";
  r["kind"] = kind;
  r["gpu"] = publicGpu(gpus.value(selected));
  if (!r.contains("cpuModel")) {
    QString model;
    for (auto line : readText("/proc/cpuinfo").split('\n'))
      if (line.startsWith("model name")) {
        model = line.section(':', 1).trimmed();
        break;
      }
    r["cpuModel"] = model.left(160);
    r["cpuThreads"] = QThread::idealThreadCount();
  }
  if (!r.contains("telemetry"))
    r["telemetry"] = telemetryHistory;
  QJsonObject parameters;
  for (auto key : {"coreMode", "memMode", "coreCurrent", "memCurrent", "coreMin", "coreMax",
                   "memMin", "memMax"})
    if (latest.contains(key))
      parameters[key] = latest[key];
  r["parameters"] = parameters;

  if (!r.contains("timestamp"))
    r["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  r["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
  if (!offline && graphicsFiles && QFileInfo::exists(graphicsFiles->filePath("scene.png")) &&
      (kind == "graphics" || kind == "bottleneck")) {
    QString dir = configRoot() + "/images";
    QDir().mkpath(dir);
    QString file = dir + "/" + r["id"].toString() + ".png";
    if (QFile::copy(graphicsFiles->filePath("scene.png"), file))
      r["previewFile"] = file;
  }
  lastResult = r;
  setTesting(false);
  auto &panel = panels[kind];
  panel.progress->setValue(r["valid"].toBool() ? 100 : 0);
  presentRecord(kind, r);
  if (!repository->add(kind, deviceKey(gpus.value(selected)), r))
    notify("Falha ao persistir histórico", repository->error);
  else
    notify(r["valid"].toBool() ? "Teste concluído e salvo" : "Teste incompleto registrado");
  refreshHistories();
  refreshFeedback();
  if (!benchmarkCheckFile.isEmpty()) {
    auto checked = r; checked["liveFramesReceived"] = liveFrameCount;
    bool ok = r["valid"].toBool() && liveFrameCount >= 10;
    checked["uiPreviewVerified"] = ok;
    QString error; ok = saveJson(benchmarkCheckFile, checked, &error) && ok;
    grab().save(benchmarkCheckFile + ".result.png");
    QTimer::singleShot(100, this, [ok] { QCoreApplication::exit(ok ? 0 : 6); });
  }
}
void MainWindow::presentRecord(QString kind, const QJsonObject &r) {
  auto &panel = panels[kind];
  QStringList lines;
  panel.plot->clear();
  panel.preview->clear();
  if (r.isEmpty()) {
    panel.result->setText("Nenhum teste salvo para esta GPU");
    panel.progress->setValue(0);
    return;
  }
  panel.progress->setValue(r["valid"].toBool() ? 100 : 0);
  if (!r["valid"].toBool())
    lines << "Sem pontuação válida · " + r["error"].toString("teste incompleto");
  else if (kind == "bottleneck") {
    if (r.contains("cpuLossPct")) {
      lines << r["summary"].toString()
        << QString("CPU: %1 ms · GPU: %2 ms · limite CPU: %3% da capacidade GPU; limite GPU: %4% da capacidade CPU.")
          .arg(r["highCpuMs"].toDouble(),0,'f',2).arg(r["highGpuMs"].toDouble(),0,'f',2)
          .arg(r["cpuLossPct"].toDouble(),0,'f',1).arg(r["gpuLossPct"].toDouble(),0,'f',1)
        << "Estimativa com sobreposição ideal; não representa perda real de FPS em um jogo.";
    } else lines << r["classification"].toString()
        << QString("Registro anterior: GPU 720p / 1440p %1 / %2 ms · CPU %3 ms")
          .arg(r["lowGpuMs"].toDouble(),0,'f',2).arg(r["highGpuMs"].toDouble(),0,'f',2).arg(r["highCpuMs"].toDouble(),0,'f',2);
  }
  else if (kind != "compute")
    lines << QString("%1 pontos · MPix/s geométrico%2")
                 .arg(r["score"].toDouble(), 0, 'f', 1)
                 .arg(r["partial"].toBool() ? " · cobertura parcial" : "");
  for (auto v : r["codecResults"].toArray()) {
    auto c = v.toObject();
    lines << QString("%1 %2: %3%4")
                 .arg(codecLabel(c["codec"].toString()), modeLabel(c["mode"].toString()),
                      stateLabel(c["status"].toString()),
                      c["valid"].toBool() ? QString(" · %1 FPS").arg(c["fps"].toDouble(), 0, 'f', 1)
                                          : "");
  }
  for (auto v : r["computeResults"].toArray()) {
    auto c = v.toObject();
    lines << c["metric"].toString() + ": " +
                 (c["status"].toString() == "passed"
                      ? QString::number(c["value"].toDouble(), 'f', 2) + " " + c["unit"].toString()
                      : "não suportado");
  }
  if (kind == "graphics" && r["valid"].toBool())
    lines << QString("%1 · %2 × %3 · GPU média %4 ms")
                 .arg(r["api"].toString())
                 .arg(r["width"].toInt())
                 .arg(r["height"].toInt())
                 .arg(meanScene(r, "gpuMeanMs"), 0, 'f', 2);
  panel.result->setText(lines.join('\n'));
  for (auto v : r["samples"].toArray())
    panel.plot->add(v.toObject()["gpuMeanMs"].toDouble());
  QPixmap preview;
  if (!r["previewFile"].toString().isEmpty())
    preview.load(r["previewFile"].toString());
  if (!preview.isNull())
    panel.preview->setPixmap(
        preview.scaled(panel.preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
void MainWindow::refreshHistories() {
  if (!repository)
    return;
  for (auto it = panels.begin(); it != panels.end(); ++it) {
    QSignalBlocker block(it->history);
    it->history->clear();
    for (auto v : repository->recent(it.key(), deviceKey(gpus.value(selected)))) {
      auto r = v.toObject();
      QString score = r["valid"].toBool()
                          ? (r.contains("score")
                                 ? QString::number(r["score"].toDouble(), 'f', 1) + " pts"
                                 : (it.key() == "compute"
                                        ? QString("Medições computacionais · unidades individuais")
                                        : r["classification"].toString()))
                          : "inválido/incompleto";
      QString configuration =
          it.key() == "graphics"
              ? r["api"].toString() + " · " + QString::number(r["width"].toInt()) + "×" +
                    QString::number(r["height"].toInt())
          : it.key() == "video"   ? r["coverage"].toString()
          : it.key() == "compute" ? QString::number(r["supported"].toInt()) + " medições"
                                  : r["classification"].toString();
      it->history->addItem(r["timestamp"].toString() + " · " + score + " · " + configuration);
    }
    auto records = repository->recent(it.key(), deviceKey(gpus.value(selected)));
    presentRecord(it.key(), records.isEmpty() ? QJsonObject{} : records[0].toObject());
    if (!records.isEmpty())
      it->history->setCurrentRow(0);
  }
}
void MainWindow::exportResultImage(QString kind) {
  auto rows = repository->recent(kind, deviceKey(gpus.value(selected)));
  if (rows.isEmpty()) {
    notify("Nenhum teste para exportar");
    return;
  }
  int index = qMax(0, panels[kind].history->currentRow());
  auto r = rows[index].toObject();
  QString path = QFileDialog::getSaveFileName(this, "Salvar relatório do teste", {},
                                              "PNG (*.png);;JPEG (*.jpg *.jpeg)");
  if (path.isEmpty())
    return;
  if (!path.endsWith(".png", Qt::CaseInsensitive) && !path.endsWith(".jpg", Qt::CaseInsensitive) &&
      !path.endsWith(".jpeg", Qt::CaseInsensitive))
    path += ".png";
  if (!renderReport(kind, r).save(path))
    notify("Falha ao salvar imagem");
  else
    notify("Relatório salvo", path);
}
QImage MainWindow::renderReport(QString kind, const QJsonObject &r) {
  QImage image(1600, 1000, QImage::Format_RGB32);
  image.fill(QColor("#101014"));
  QPainter p(&image);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QColor(gpus.value(selected).accent()));
  p.setFont(QFont("Sans", 25, QFont::Bold));
  p.drawText(45, 65, "GPU CONTROL · " + kind.toUpper());
  p.setPen(Qt::white);
  p.setFont(QFont("Sans", 14));
  p.drawText(45, 110, r["gpu"].toObject()["name"].toString());
  p.drawText(45, 145, r["timestamp"].toString() + " · " + r["method"].toString());
  p.drawText(
      45, 185,
      r["valid"].toBool()
          ? (r.contains("score")
                 ? QString("Pontuação: %1 · MPix/s geométrico")
                       .arg(r["score"].toDouble(), 0, 'f', 2)
                 : (kind == "compute" ? QString("Medições computacionais · unidades individuais")
                                      : r["classification"].toString()))
          : "Resultado inválido / incompleto");
  QString configuration = kind == "graphics"
                              ? r["api"].toString() + " · " + QString::number(r["width"].toInt()) +
                                    " × " + QString::number(r["height"].toInt())
                          : kind == "video"   ? "1080p · cobertura " + r["coverage"].toString()
                          : kind == "compute" ? "Kernels Vulkan · unidades independentes"
                                              : "720p × 1440p · carga CPU fixa";
  p.drawText(45, 225, configuration);
  p.setFont(QFont("Sans", 11));
  p.drawText(45, 250,
             "Driver " + r["gpu"].toObject()["driverVersion"].toString() + " · " +
                 r["gpu"].toObject()["architecture"].toString());
  p.setFont(QFont("Sans", 14));
  int y = 275;
  for (auto v : r["codecResults"].toArray()) {
    auto c = v.toObject();
    p.drawText(45, y,
               QString("%1 / %2: %3%4")
                   .arg(codecLabel(c["codec"].toString()), modeLabel(c["mode"].toString()),
                        stateLabel(c["status"].toString()),
                        c["valid"].toBool()
                            ? QString(" · %1 FPS").arg(c["fps"].toDouble(), 0, 'f', 2)
                            : QString()));
    y += 36;
  }
  for (auto v : r["computeResults"].toArray()) {
    auto c = v.toObject();
    p.drawText(
        45, y,
        c["metric"].toString() + ": " +
            (c["status"].toString() == "passed"
                 ? QString::number(c["value"].toDouble(), 'f', 2) + " " + c["unit"].toString()
                 : "não suportado"));
    y += 36;
  }
  for (auto v : r["sceneResults"].toArray()) {
    auto c = v.toObject();
    p.drawText(45, y,
               QString("Cena %1: GPU %2 ms · p99 %3 ms · throughput %4 MPix/s")
                   .arg(c["scene"].toInt() + 1)
                   .arg(c["gpuMeanMs"].toDouble(), 0, 'f', 3)
                   .arg(c["gpuP99Ms"].toDouble(), 0, 'f', 3)
                   .arg(c["score"].toDouble(), 0, 'f', 1));
    y += 36;
  }
  if (kind == "bottleneck") {
    QString summary = r.contains("cpuLossPct") ? r["summary"].toString() + "\nEstimativa sintética, não perda real em jogos."
      : QString("Registro anterior · GPU 720p / 1440p: %1 / %2 ms; CPU: %3 ms").arg(r["lowGpuMs"].toDouble()).arg(r["highGpuMs"].toDouble()).arg(r["highCpuMs"].toDouble());
    p.drawText(QRect(45,y,1510,110),Qt::TextWordWrap,summary); y += 120;
  }
  if (kind == "video") {
    auto codecs = r["codecResults"].toArray();
    double max = 1;
    for (auto v : codecs)
      max = qMax(max, v.toObject()["fps"].toDouble());
    int row = 0;
    for (auto v : codecs) {
      auto c = v.toObject();
      int yy = 535 + row * 45;
      p.setPen(QColor("#ddd"));
      p.drawText(45, yy + 22,
                 codecLabel(c["codec"].toString()) + " " + modeLabel(c["mode"].toString()));
      if (c["valid"].toBool()) {
        p.fillRect(QRectF(260, yy, c["fps"].toDouble() / max * 1150, 26),
                   QColor(gpus.value(selected).accent()));
        p.drawText(1430, yy + 22, QString::number(c["fps"].toDouble(), 'f', 1));
      } else
        p.drawText(270, yy + 22, "Não suportado / não concluído");
      ++row;
    }
  }
  auto samples = r["samples"].toArray();
  if (samples.size() > 1) {
    QRect chart(45, qMax(430, y + 20), 1510, 330);
    p.setPen(QColor("#44444c"));
    p.drawRect(chart);
    double max = 1;
    for (auto v : samples)
      max = qMax(max, v.toObject()["gpuMeanMs"].toDouble());
    QPainterPath line;
    for (int i = 0; i < samples.size(); ++i) {
      QPointF pt(chart.left() + i * double(chart.width()) / (samples.size() - 1),
                 chart.bottom() -
                     samples[i].toObject()["gpuMeanMs"].toDouble() / max * chart.height());
      if (i)
        line.lineTo(pt);
      else
        line.moveTo(pt);
    }
    p.setPen(QPen(QColor(gpus.value(selected).accent()), 2));
    p.drawPath(line);
  }
  p.setPen(QColor("#aaaaaa"));
  p.setFont(QFont("Sans", 12));
  p.drawText(QRect(45, 880, 1510, 80), Qt::TextWordWrap,
             "Pontuação própria. Compare método, API, preset e cobertura equivalentes. Resultado "
             "de teste não garante estabilidade prolongada. Tensão e limite de potência não são "
             "alterados pelos benchmarks.");
  p.end();
  return image;
}
QWidget *MainWindow::energy() {
  auto *root = page("Overclock · ajustes manuais");
  auto *l = qobject_cast<QVBoxLayout *>(root->layout());
  l->addWidget(text("Ajustes manuais conforme as capacidades reais da GPU e do driver. "
                    "Consumo é uma leitura; power limit é o limite de potência. Undervolt reduz a tensão.",
                    15));
  helperInfo = text("Consultando capacidades e helper de clocks");
  l->addWidget(helperInfo);
  clockInfo = text("Nenhuma alteração aplicada nesta sessão.");
  l->addWidget(clockInfo);
  auto *card = new QGroupBox("Clocks da GPU selecionada");
  auto *cl = new QVBoxLayout(card);
  coreOffset = new QSlider(Qt::Horizontal);
  memOffset = new QSlider(Qt::Horizontal);
  coreEntry = new QLineEdit;
  memEntry = new QLineEdit;
  coreEntry->setMaximumWidth(160);
  memEntry->setMaximumWidth(160);
  coreEntry->setPlaceholderText("MHz");
  memEntry->setPlaceholderText("MHz");
  coreValue = text("Core · indisponível", 16);
  memValue = text("Memória · indisponível", 16);
  applyCore = new QPushButton("Aplicar clock do core");
  applyMem = new QPushButton("Aplicar clock da memória");
  for (auto *w : {coreOffset, memOffset}) {
    w->setEnabled(false);
    w->setMinimumHeight(30);
  }
  applyCore->setEnabled(false);
  applyMem->setEnabled(false);
  cl->addWidget(coreValue);
  auto *coreRow = new QHBoxLayout;
  coreRow->addWidget(coreOffset, 1);
  coreRow->addWidget(coreEntry);
  cl->addLayout(coreRow);
  cl->addWidget(applyCore);
  cl->addSpacing(18);
  cl->addWidget(memValue);
  auto *memRow = new QHBoxLayout;
  memRow->addWidget(memOffset, 1);
  memRow->addWidget(memEntry);
  cl->addLayout(memRow);
  cl->addWidget(applyMem);
  l->addWidget(card);
  for (QString domain : {"core", "mem"}) {
    auto *slider = domain == "core" ? coreOffset : memOffset;
    auto *display = domain == "core" ? coreValue : memValue;
    auto *entry = domain == "core" ? coreEntry : memEntry;
    auto *validator = new QIntValidator(-10000, 100000, entry);
    auto locale = QLocale::c();
    locale.setNumberOptions(QLocale::RejectGroupSeparator);
    validator->setLocale(locale);
    entry->setValidator(validator);
    entry->setEnabled(false);
    connect(entry, &QLineEdit::editingFinished, this, [entry, slider] {
      bool ok = false;
      int value = entry->text().toInt(&ok);
      if (ok)
        slider->setValue(value);
      entry->setText(QString::number(slider->value()));
    });
    connect(slider, &QSlider::valueChanged, entry,
            [entry](int n) { entry->setText(QString::number(n)); });
    connect(slider, &QSlider::valueChanged, this, [display, domain](int value) {
      display->setText(
          QString("%1 selecionado: %2 MHz").arg(domain == "core" ? "Core" : "Memória").arg(value));
    });
    auto *apply = domain == "core" ? applyCore : applyMem;
    connect(apply, &QPushButton::clicked, this, [this, domain, slider, entry] {
      bool valid=false;int typed=entry->text().toInt(&valid);
      if(valid&&entry->hasAcceptableInput())slider->setValue(typed);
      if (!entry->hasAcceptableInput()) {
        notify("Informe uma frequência dentro da faixa mostrada");
        return;
      }
      if (testActive || offline || !helperReady() || !latest["temp"].isDouble() || !confirmOverclock())
        return;
      if (QMessageBox::question(
              this, "Aplicar clock",
              QString(
                  "Aplicar %1 MHz em %2? NVIDIA usa offset; AMD/Intel usam teto absoluto. Tensão e "
                  "potência não serão alteradas. O ajuste manual permanece até ser restaurado.")
                  .arg(slider->value())
                  .arg(domain)) != QMessageBox::Yes)
        return;
      activeKind = "manual-clock";
      setTesting(true);
      clockOutput.clear();
      clockProcess.start("pkexec", {"--disable-internal-agent", helperPath(), gpus[selected].pci,
                                    domain, QString::number(slider->value())});
    });
  }
  buildTuningControls(l);
  trialButton = new QPushButton("Validar candidato: baseline → core +15 MHz → restaurar",root);
  trialButton->setEnabled(false);
  trialButton->hide(); // Legacy diagnostic remains CLI-only; manual controls in this page.
  stopTrial = new QPushButton("Interromper teste e restaurar clock",root);
  stopTrial->setEnabled(false);
  stopTrial->hide();
  connect(stopTrial, &QPushButton::clicked, this, [this] {
    if (!trialActive)
      return;
    trialCancelled = true;
    trialGraphics = {{"valid", false}, {"errorCode", "USER_CANCELLED"}};
    if (clockProcess.state() != QProcess::NotRunning) {
      clockProcess.write("STOP\n");
      clockProcess.closeWriteChannel();
    }
    if (graphicsProcess.state() != QProcess::NotRunning)
      graphicsProcess.kill();
  });
  connect(trialButton, &QPushButton::clicked, this, &MainWindow::startClockTrial);
  l->addStretch();
  return root;
}
void MainWindow::startClockTrial() {
  if (offline || testActive || !helperReady() || selected >= gpus.size() || !confirmOverclock())
    return;
  if (gpus[selected].vendor != "NVIDIA" || !latest["coreCurrent"].isDouble() ||
      !latest["memCurrent"].isDouble() || !latest["temp"].isDouble() ||
      latest["temp"].toDouble() >= 60) {
    notify("Telemetria ou condições insuficientes para o teste");
    return;
  }
  trialCancelled = false;
  trialActive = true;
  trialRestoring = false;
  trialBaseline = {};
  trialGraphics = {};
  trialLease = {};
  startGraphics("clock-trial", "Vulkan", 1280, 720, 6);
}
void MainWindow::handleClockLine(QByteArray line) {
  if (line.startsWith("TRIAL ")) {
    auto data = QJsonDocument::fromJson(line.mid(6)).object();
    trialLease = data;
    clockInfo->setText(
        "Core +15 MHz temporário · watchdog independente ativo · tensão e potência preservadas");
    startGraphics("clock-trial", "Vulkan", 1280, 720, 6);
  } else if (line.startsWith("SAMPLE ")) {
    auto data = QJsonDocument::fromJson(line.mid(7)).object();
    clockInfo->setText(QString("Teste temporário · %1 °C · %2 MHz · %3 W")
                           .arg(data["temp"].toDouble())
                           .arg(data["clock"].toDouble())
                           .arg(data["power"].toDouble()));
  } else if (line.startsWith("RESTORED "))
    trialLease = QJsonDocument::fromJson(line.mid(9)).object();
  else
    log(QString::fromUtf8(line));
}
void MainWindow::finishClockTrial() {
  if (!trialActive)
    return;
  QJsonObject r = trialLease;
  r["kind"] = "clock-trial";
  r["method"] = "core-plus15-short-trial-v1";
  bool observed = !trialGraphics["samples"].toArray().isEmpty();
  for (auto v : trialGraphics["samples"].toArray()) {
    auto sample = v.toObject();
    observed = observed && sample["coreCurrent"].isDouble() &&
               sample["coreCurrent"].toInt() == trialLease["coreTested"].toInt() &&
               sample["memCurrent"].isDouble() &&
               sample["memCurrent"].toInt() == trialLease["memoryBefore"].toInt();
  }
  r["valid"] = observed && !trialCancelled && trialLease["valid"].toBool() &&
               trialLease["restored"].toBool() && trialBaseline["valid"].toBool() &&
               trialGraphics["valid"].toBool();
  r["status"] = r["valid"].toBool() ? "passed-short-trial" : "not-validated";
  r["comparison"] = QJsonArray{trialBaseline, trialGraphics};
  r["gpu"] = publicGpu(gpus.value(selected));
  r["voltageWritten"] = false;
  r["powerLimitWritten"] = false;
  r["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  r["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
  repository->add("clock-trial", deviceKey(gpus.value(selected)), r);
  trialActive = false;
  trialRestoring = false;
  setTesting(false);
  clockInfo->setText(
      r["valid"].toBool()
          ? "Teste curto concluído; offset original restaurado. Resultado registrado, sem garantia "
            "de estabilidade prolongada."
          : "Teste não validado. Consulte restauração e logs; não há perfil estável aprovado.");
  notify(r["valid"].toBool() ? "Candidato passou no teste curto e foi restaurado"
                             : "Teste de clocks não validado",
         r["restored"].toBool() ? "Restauração confirmada"
                                : "Restauração não confirmada: verifique a GPU antes de continuar");

  refreshFeedback();
  if (!clockCheckFile.isEmpty()) {
    QString error;
    bool saved = saveJson(clockCheckFile, r, &error);
    QTimer::singleShot(500, qApp, [valid = r["valid"].toBool(), saved] {
      QCoreApplication::exit(saved ? (valid ? 0 : 1) : 3);
    });
  }
}


void MainWindow::clockCheck(QString path) {
  if (offline)
    return;
  clockCheckFile = path;
  pages->setCurrentIndex(4);
  auto *ready = new QTimer(this);
  ready->setInterval(250);
  auto attempts = std::make_shared<int>(0);
  connect(ready, &QTimer::timeout, this, [this, ready, attempts] {
    if (gpus.size() > 0 && gpus[selected].vendor == "NVIDIA" && latest["temp"].isDouble() &&
        latest["temp"].toDouble() < 60 && latest["coreCurrent"].isDouble() &&
        latest["memCurrent"].isDouble() && helperReady()) {
      ready->stop();
      ready->deleteLater();
      startClockTrial();
    } else if (++*attempts >= 40) {
      ready->stop();
      ready->deleteLater();
      QString error;
      saveJson(clockCheckFile, {{"valid", false}, {"errorCode", "CLOCK_CHECK_PRECONDITION"}},
               &error);
      QCoreApplication::exit(2);
    }
  });
  ready->start();
}


bool MainWindow::confirmOverclock() {
  if(overclockAccepted) return true;
  QDialog dialog(this);
  dialog.setObjectName("overclockDisclaimer");
  dialog.setWindowTitle("Overclock — aviso de riscos e responsabilidade");
  dialog.resize(660,540);
  auto *layout=new QVBoxLayout(&dialog);
  auto *body=new QTextBrowser;
  body->setMarkdown(readText(":/docs/AVISO-DE-RESPONSABILIDADE.md"));
  layout->addWidget(body,1);
  auto *accept=new QCheckBox("Li, compreendi os riscos e assumo a responsabilidade pelas alterações que realizar.");
  accept->setObjectName("acceptOverclockRisk");
  layout->addWidget(accept);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
  auto *ok=buttons->button(QDialogButtonBox::Ok);
  ok->setText("Aceitar e acessar Overclock");
  buttons->button(QDialogButtonBox::Cancel)->setText("Voltar");
  ok->setEnabled(false);
  connect(accept,&QCheckBox::toggled,ok,&QPushButton::setEnabled);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  layout->addWidget(buttons);
  overclockAccepted=dialog.exec()==QDialog::Accepted && accept->isChecked();
  return overclockAccepted;
}
