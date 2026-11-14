#include "window.h"
#include "branding.h"
#include "driver_packages.h"
#include <cmath>
#include <sys/stat.h>
#include <unistd.h>
namespace {
QLabel *label(QString s, int size = 0) {
  auto *l = new QLabel(s);
  l->setWordWrap(true);
  if (size)
    l->setStyleSheet(QString("font-size:%1px;font-weight:700;").arg(size));
  return l;
}
QPushButton *button(QString s, QBoxLayout *l = nullptr) {
  s.replace("&", "&&");
  auto *b = new QPushButton(s);
  b->setCursor(Qt::PointingHandCursor);
  if (l)
    l->addWidget(b);
  return b;
}
QGroupBox *group(QString title, QGridLayout *grid, int row, int col, int span = 1) {
  auto *g = new QGroupBox(title);
  if (grid)
    grid->addWidget(g, row, col, 1, span);
  return g;
}
QString quote(QString s) {
  s.replace("'", "'\\''");
  return "'" + s + "'";
}
bool writeFile(QString p, QByteArray b, QString &error) {
  QSaveFile f(p);
  if (!f.open(QIODevice::WriteOnly) || f.write(b) != b.size() || !f.commit()) {
    error = f.errorString();
    return false;
  }
  return true;
}
bool trustedHelper() { return rootHelperTrusted(installedClockHelper()); }

} // namespace
void HistoryPlot::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor("#111115"));
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QColor("#25252b"));
  for (int i = 1; i < 5; ++i)
    p.drawLine(0, height() * i / 5, width(), height() * i / 5);
  p.setPen(QColor("#999999"));
  p.drawText(12, 22, samples.isEmpty() ? caption + " · aguardando dados" : caption);
  if (samples.size() < 2)
    return;
  double hi = qMax(1., *std::max_element(samples.begin(), samples.end()) * 1.2);
  QPainterPath path;
  for (int i = 0; i < samples.size(); ++i) {
    QPointF pt(10. + i * (width() - 20.) / (samples.size() - 1),
               height() - 12. - samples[i] / hi * (height() - 45));
    if (i == 0)
      path.moveTo(pt);
    else
      path.lineTo(pt);
  }
  p.setPen(QPen(palette().color(QPalette::Highlight), 2));
  p.drawPath(path);
}
QWidget *MainWindow::scrollPage(QWidget *content) {
  auto *scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  if (content->layout()) content->layout()->setSizeConstraint(QLayout::SetMinimumSize);
  scroll->setWidget(content);
  return scroll;
}
QWidget *MainWindow::page(QString title) {
  auto *w = new QWidget;
  w->setObjectName("contentPage");
  auto *l = new QVBoxLayout(w);
  l->setContentsMargins(24, 18, 24, 20);
  l->setSpacing(16);
  l->addWidget(label(title, 21));
  return w;
}
MainWindow::MainWindow(bool preview) : offline(preview), codecSuite(this) {
  repository = std::make_unique<Repository>(offline ? QString(":memory:")
                                                    : configRoot() + "/results.sqlite");
  resize(1240, 900);
  setWindowIcon(Branding::windowIcon(QColor("#76b900")));
  setMinimumSize(980, 700);
  setWindowTitle("GPU-PRO · 0.7.0");
  if (!offline)
    overlayConfig = OverlayConfig::fromJson(
        QJsonDocument::fromJson(readText(configRoot() + "/overlay.json").toUtf8()).object());
  // Starting the panel never restores a previously visible HUD automatically.
  overlayConfig.enabled = false;
  auto *central = new QWidget;
  setCentralWidget(central);
  auto *main = new QHBoxLayout(central);
  main->setContentsMargins(0, 0, 0, 0);
  main->setSpacing(0);
  auto *sidebar = new QFrame;
  sidebar->setObjectName("sidebar");
  sidebar->setFixedWidth(240);
  auto *sb = new QVBoxLayout(sidebar);
  sb->setContentsMargins(0, 20, 0, 16);
  auto *brand = label("  ◈  GPU CONTROL", 18);
  brand->setObjectName("brand");
  sb->addWidget(brand);
  sb->addSpacing(20);
  pages = new QStackedWidget;
  auto *right = new QVBoxLayout;
  right->setContentsMargins(0, 0, 0, 0);
  auto *header = new QHBoxLayout;
  header->setContentsMargins(24, 14, 24, 0);
  header->addWidget(label("GPU ativa"));
  gpuSelect = new QComboBox;
  gpuSelect->setMinimumWidth(340);
  header->addWidget(gpuSelect, 1);
  auto *inventoryRefresh = button("↻", header);
  inventoryRefresh->setToolTip("Atualizar inventário de GPUs");
  inventoryRefresh->setEnabled(!offline);
  connect(inventoryRefresh, &QPushButton::clicked, this, [this] {
    if (testActive || codecSuite.busy() || clockProcess.state() != QProcess::NotRunning) {
      notify("Aguarde a operação em andamento");
      return;
    }
    emit refreshInventory();
  });
  auto *notif = button("Notificações", header);
  connect(notif, &QPushButton::clicked, this, [this] { pages->setCurrentIndex(7); });
  right->addLayout(header);
  right->addWidget(pages, 1);
  status = label(offline ? "PRÉVIA · dados fictícios · nenhum acesso à GPU"
                         : "Aguardando inventário de dispositivos");
  status->setContentsMargins(24, 0, 24, 12);
  right->addWidget(status);
  notifications = new QListWidget;
  logs = new QPlainTextEdit;
  logs->setReadOnly(true);
  logs->document()->setMaximumBlockCount(2000);
  gpuTabs = new QTabWidget;
  gpuTabs->addTab(scrollPage(dashboard()), "Monitor de recursos");
  auto *specScroll = qobject_cast<QScrollArea *>(scrollPage(specs()));
  specViewport = specScroll->viewport();
  specViewport->installEventFilter(this);
  gpuTabs->addTab(specScroll, "Especificações");
  pages->addWidget(gpuTabs);
  pages->addWidget(scrollPage(profiles()));
  pages->addWidget(scrollPage(monitor()));
  pages->addWidget(scrollPage(displays()));
  pages->addWidget(scrollPage(energy()));
  pages->addWidget(benchmarks());
  pages->addWidget(scrollPage(feedbackPage()));
  auto *np = page("Central de Notificações");
  auto *nl = qobject_cast<QVBoxLayout *>(np->layout());
  auto *clear = button("Limpar notificações", nl);
  nl->addWidget(notifications, 1);
  connect(clear, &QPushButton::clicked, notifications, &QListWidget::clear);
  pages->addWidget(scrollPage(np));
  auto *lp = page("Logs de operações");
  auto *ll = qobject_cast<QVBoxLayout *>(lp->layout());
  ll->addWidget(logs, 1);
  auto *exportLogs = button("Exportar logs", ll);
  connect(exportLogs, &QPushButton::clicked, this, [this] {
    auto path =
        QFileDialog::getSaveFileName(this, "Salvar logs", QDir::homePath() + "/gpu-control.log");
    if (path.isEmpty())
      return;
    QString e;
    if (!writeFile(path, logs->toPlainText().toUtf8(), e))
      notify("Falha ao exportar", e);
  });
  pages->addWidget(scrollPage(lp));
  pages->addWidget(scrollPage(updatesPage()));
  QStringList names = {
      "GPU · Monitor e especificações", "Configurações 3D & Filtros", "GamerMonitor & Overlay",
      "Monitores & Telas", "Overclock · Manual", "Benchmark",
      "Feedback & Diagnóstico", "Central de Notificações", "Logs de operações", "Atualizações"};
  for (int i = 0; i < names.size(); ++i) {
    auto *b = button(names[i], sb);
    b->setObjectName("nav");
    navigation << b;
    connect(b, &QPushButton::clicked, this, [this, i] {
      if(i==4 && !confirmOverclock()) return;
      pages->setCurrentIndex(i);
    });
  }
  sb->addStretch();
  sb->addWidget(label("   Linux · MultiGPU\n   NVIDIA / AMD / Intel\n   v0.7 · "
                      "validação comunitária"));
  main->addWidget(sidebar);
  main->addLayout(right, 1);
  connect(pages, &QStackedWidget::currentChanged, this, [this](int index) {
    for (int i = 0; i < navigation.size(); ++i) {
      navigation[i]->setProperty("active", index == i);
      navigation[i]->style()->unpolish(navigation[i]);
      navigation[i]->style()->polish(navigation[i]);
    }
  });
  connect(gpuSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::chooseGpu);
  connect(&hud, &QProcess::readyReadStandardOutput, this, [this] {
    hudOutput += hud.readAllStandardOutput();
    while (hudOutput.contains('\n')) {
      int pos = hudOutput.indexOf('\n');
      QByteArray line = hudOutput.left(pos);
      hudOutput.remove(0, pos + 1);
      if (line == "TOGGLE") {
        hudEnabled->setChecked(!hudEnabled->isChecked());
      } else if (line.startsWith("READY ")) {
        bool ok;
        int port = line.mid(6).toInt(&ok);
        if (ok && port > 0 && port < 65536) {
          hudReady = true;
          hudFailures = 0;
          emit overlayEndpoint(port, hudToken);
          log("Overlay GTK pronto; UDP em 127.0.0.1:" + QString::number(port));
        }
      }
    }
  });
  connect(&hud, &QProcess::readyReadStandardError, this,
          [this] { log("HUD: " + QString::fromUtf8(hud.readAllStandardError()).trimmed()); });
  connect(&hud, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this](int code, QProcess::ExitStatus) {
            emit overlayEndpoint(0, {});
            if (!closing && overlayConfig.enabled) {
              ++hudFailures;
              if (hudFailures >= 3) {
                hudEnabled->setChecked(false);
                notify("HUD indisponível após três tentativas",
                       "Verifique X11/XWayland e consulte os logs. Reative Exibir "
                       "HUD para tentar novamente.");
              } else
                notify("Overlay encerrado",
                       QString("Código %1; nova tentativa supervisionada").arg(code));
            }
          });
  supervise.setInterval(3000);
  connect(&supervise, &QTimer::timeout, this, [this] {
    if (overlayConfig.enabled && hud.state() == QProcess::NotRunning)
      startHud();
  });
  if (!offline)
    supervise.start();
  connect(&clockProcess, &QProcess::readyReadStandardOutput, this, [this] {
    clockOutput += clockProcess.readAllStandardOutput();
    while (clockOutput.contains('\n')) {
      int pos = clockOutput.indexOf('\n');
      auto line = clockOutput.left(pos);
      clockOutput.remove(0, pos + 1);
      handleClockLine(line);
    }
  });
  connect(&clockProcess, &QProcess::readyReadStandardError, this, [this] {
    log("Helper de clocks: " + QString::fromUtf8(clockProcess.readAllStandardError()));
  });
  connect(&clockProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this](int code, QProcess::ExitStatus) {
            if (trialActive) {
              if (!trialLease.contains("restored")) {
                trialLease = {{"valid", false},
                              {"restored", false},
                              {"error", "Helper encerrou sem confirmar restauração"}};
              }
              if (graphicsProcess.state() != QProcess::NotRunning) {
                graphicsProcess.kill();
              } else
                finishClockTrial();
            } else {
              setTesting(false);
              notify(code == 0 ? "Ajuste confirmado pelo driver"
                               : "Ajuste não confirmado; consulte os logs");
            }
          });
  connect(&clockProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      if (trialActive) {
        trialLease = {{"valid", false},
                      {"restored", true},
                      {"error", "Não foi possível iniciar autorização"}};
        finishClockTrial();
      } else
        setTesting(false);
      notify("Não foi possível iniciar o helper", clockProcess.errorString());
    }
  });
  if (offline) {
    Gpu a;
    a.name = "NVIDIA GeForce · exemplo visual";
    a.vendor = "NVIDIA";
    a.pci = "0000:01:00.0";
    a.deviceId = "0x0000";
    a.specs = {{"Origem", "Dados fictícios para verificar layout"}};
    Gpu b = a;
    b.vendor = "AMD";
    b.name = "AMD Radeon · exemplo visual";
    b.pci = "0000:02:00.0";
    Gpu c = a;
    c.vendor = "Intel";
    c.name = "Intel Arc · exemplo visual";
    c.pci = "0000:03:00.0";
    setInventory({a, b, c});
    updateSnapshot({{"name", a.name},
                    {"pci", a.pci},
                    {"usage", 42},
                    {"temp", 58},
                    {"vram", 4294967296.},
                    {"vramTotal", 12884901888.},
                    {"power", 125.4},
                    {"clock", 1800},
                    {"memClock", 7000},
                    {"encoder", 0},
                    {"decoder", 0},
                    {"fan", 35},
                    {"fps", 120.0}, {"fpsLow", 74.8}, {"fpsAvg", 112.5}, {"fpsMax", 144.0},
                    {"frameTimes", QJsonArray{8.3,8.5,8.1,9.0,8.7,12.0,8.2,8.4,9.1,8.6,8.3}}});
  } else {
    qRegisterMetaType<QList<Gpu>>();
    service = new TelemetryService;
    service->moveToThread(&telemetryThread);
    connect(&telemetryThread, &QThread::started, service, &TelemetryService::start);
    connect(&telemetryThread, &QThread::finished, service, &QObject::deleteLater);
    connect(service, &TelemetryService::inventory, this, &MainWindow::setInventory);
    connect(service, &TelemetryService::snapshot, this, &MainWindow::updateSnapshot);
    connect(this, &MainWindow::selectGpu, service, &TelemetryService::choose);
    connect(this, &MainWindow::configureOverlay, service, &TelemetryService::overlay);
    connect(this, &MainWindow::overlayEndpoint, service, &TelemetryService::endpoint);
    connect(this, &MainWindow::trackProcess, service, &TelemetryService::process);
    connect(this, &MainWindow::refreshInventory, service, &TelemetryService::refresh);
    telemetryThread.start();
  }
  connect(&codecSuite, &CodecSuite::progress, this, [this](int value, QString text) {
    panels["video"].progress->setValue(value);
    panels["video"].result->setText(text);
  });
  connect(&codecSuite, &CodecSuite::result, this, &MainWindow::finishTest);
  connect(&graphicsProcess, &QProcess::readyReadStandardOutput, this, [this] {
    graphicsOutput += graphicsProcess.readAllStandardOutput();
    while (graphicsOutput.contains('\n')) {
      int n = graphicsOutput.indexOf('\n');
      auto line = graphicsOutput.left(n);
      graphicsOutput.remove(0, n + 1);
      if (line.startsWith("PROGRESS ")) {
        graphicsWatchdog.start(15000);
        auto p = QJsonDocument::fromJson(line.mid(9)).object();
        auto &panel = panels[activeKind == "clock-trial" ? "graphics" : activeKind];
        panel.progress->setValue(p["progress"].toInt());
        panel.plot->add(p["gpuMeanMs"].toDouble());
        panel.result->setText(QString("Cena %1 · GPU %2 ms")
                                  .arg(p["scene"].toInt() + 1)
                                  .arg(p["gpuMeanMs"].toDouble(), 0, 'f', 2));
      } else if (line.startsWith("FRAME ") && graphicsFiles) {
        auto &panel = panels[activeKind == "clock-trial" ? "graphics" : activeKind];
        QPixmap frame(graphicsFiles->filePath("live.jpg"));
        if (!frame.isNull()) {
          panel.preview->setPixmap(frame.scaled(panel.preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
          ++liveFrameCount;
          if (!benchmarkCheckFile.isEmpty() && liveFrameCount == 30) grab().save(benchmarkCheckFile + ".live.png");
        }
      } else if (line.startsWith("RESULT "))
        graphicsResult = QJsonDocument::fromJson(line.mid(7)).object();
    }
  });
  connect(&graphicsProcess, &QProcess::readyReadStandardError, this, [this] {
    log("Renderizador: " + QString::fromUtf8(graphicsProcess.readAllStandardError()));
  });
  connect(&graphicsProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this](int code, QProcess::ExitStatus state) {
            graphicsWatchdog.stop();
            auto r = graphicsResult;
            if (r.isEmpty() || state != QProcess::NormalExit) {
              r = {{"valid", false},
                   {"error", "Processo gráfico interrompido ou sem resultado"},
                   {"errorCode", "WORKER_INTERRUPTED"}};
            }
            if (code != 0)
              r["valid"] = false;
            handleGraphics(r);
          });
  connect(&graphicsProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
    if (e == QProcess::FailedToStart) {
      graphicsWatchdog.stop();
      handleGraphics({{"valid", false},
                      {"error", graphicsProcess.errorString()},
                      {"errorCode", "WORKER_UNAVAILABLE"}});
    }
  });
  graphicsWatchdog.setSingleShot(true);
  connect(&graphicsWatchdog, &QTimer::timeout, this, [this] {
    graphicsProcess.kill();
    notify("Teste interrompido por ausência de progresso");
  });
  auto *neonTimer = new QTimer(this);
  neonTimer->setInterval(40);
  connect(neonTimer, &QTimer::timeout, this, [this] {
    for (auto *card : findChildren<NeonCard *>())
      if (isVisible() && qApp->property("gpuProNeonMotion").toBool() && card->isVisible()) card->update();
  });
  neonTimer->start();
  clockProcess.setProcessEnvironment(nativeEnvironment());
  setupTray();
  applyTheme();
  navigation[0]->setProperty("active", true);
}
MainWindow::~MainWindow() {
  closing = true;
  supervise.stop();
  hud.terminate();
  if (!hud.waitForFinished(1000)) {
    hud.kill();
    hud.waitForFinished(1000);
  }
  if (service) {
    QMetaObject::invokeMethod(service, &TelemetryService::stop, Qt::BlockingQueuedConnection);
    telemetryThread.quit();
    telemetryThread.wait();
  }
  graphicsProcess.disconnect(this);
  graphicsProcess.kill();
  graphicsProcess.waitForFinished(1500);
}
void MainWindow::closeEvent(QCloseEvent *e) {
  if (driverBusy) { notify("Aguarde o gerenciador de pacotes terminar antes de encerrar"); e->ignore(); return; }
  if (clockProcess.state() != QProcess::NotRunning) {
    notify("Aguarde a operação de energia terminar");
    e->ignore();
    return;
  }
  if (displayBusy) {
    notify("Aguarde a operação de tela terminar");
    e->ignore();
    return;
  }
  if (!explicitQuit && keepInTray && keepInTray->isChecked() && tray && tray->isVisible() && QSystemTrayIcon::isSystemTrayAvailable()) {
    hide(); e->ignore(); return;
  }
  closing = true;
  if (graphicsProcess.state() != QProcess::NotRunning)
    graphicsProcess.kill();
  codecSuite.cancel();
  QMainWindow::closeEvent(e);
}
void MainWindow::log(QString s) {
  if (logs)
    logs->appendPlainText(QDateTime::currentDateTime().toString("[HH:mm:ss] ") + s);
}
void MainWindow::notify(QString title, QString detail) {
  log(title + (detail.isEmpty() ? "" : "\n" + detail));
  if (notifications) {
    notifications->insertItem(0, QDateTime::currentDateTime().toString("HH:mm ") + title + "\n" +
                                     detail);
    if (notifications->count() > 200)
      delete notifications->takeItem(200);
  }
  status->setText(title);
}
void MainWindow::applyTheme() {
  QString vendor = selected >= 0 && selected < gpus.size() ? gpus[selected].vendor : "GPU";
  if (auto *brand = findChild<QLabel *>("brand"))
    brand->setText("  ◈  GPU-PRO");
  Q_UNUSED(vendor);

  QString c = selected >= 0 && selected < gpus.size() ? gpus[selected].accent() : "#76b900";
  updateBranding(QColor(c));
  setStyleSheet(QString(R"(
 QWidget#contentPage{background:#0a0a0e;} QScrollBar:vertical{background:#14141b;width:12px;margin:0;} QScrollBar::handle:vertical{background:#464657;min-height:32px;border-radius:5px;} QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;} QTableCornerButton::section{background:#1c1c22;border:0;} QMainWindow,QStackedWidget{background:#0a0a0e;} QWidget{color:#e0e0e0;font-family:'Noto Sans',sans-serif;font-size:12px;} QFrame#sidebar{background:#121214;border-right:1px solid #242426;} QLabel#brand{color:%1;} QPushButton#nav{background:transparent;color:#aaa;text-align:left;padding:11px 14px;border:0;border-left:3px solid transparent;font-weight:600;} QPushButton#nav:hover{background:#1b1b20;color:white;} QPushButton#nav[active=true]{background:#1b1b20;color:white;border-left:3px solid %1;} QGroupBox{background:#141418;border:1px solid #29292d;border-radius:8px;margin-top:10px;padding:18px 10px 10px;font-weight:700;color:%1;} QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 5px;} QComboBox,QSpinBox,QDoubleSpinBox,QLineEdit{background:#1e1e24;border:1px solid #38383e;border-radius:4px;padding:7px;color:white;min-height:18px;} QComboBox QAbstractItemView{background:#202024;selection-background-color:%1;} QPushButton{background:%1;color:#090909;font-weight:700;border:0;border-radius:4px;padding:9px 13px;} QPushButton:disabled{background:#29292d;color:#888;} QPushButton:hover{border:1px solid #aaa;} QPlainTextEdit,QListWidget,QTableWidget{background:#111115;border:1px solid #29292d;color:#ddd;selection-background-color:#343440;} QHeaderView::section{background:#1c1c22;color:#ccc;border:0;padding:7px;} QProgressBar{background:#16161b;border:1px solid #303036;border-radius:4px;text-align:center;min-height:20px;} QProgressBar::chunk{background:%1;} QScrollArea{border:0;background:transparent;} QSlider::groove:horizontal{background:#303039;height:10px;border-radius:5px;} QSlider::sub-page:horizontal{background:%1;border-radius:5px;} QSlider::handle:horizontal{background:#eeeeee;border:2px solid %1;width:20px;margin:-6px 0;border-radius:7px;} QTabWidget::pane{border:1px solid #29292d;} QTabBar::tab{background:#1e1e24;color:#aaa;padding:10px 16px;} QTabBar::tab:selected{color:#fff;border-bottom:3px solid %1;} QCheckBox{spacing:7px;} QToolTip{background:#202024;color:white;border:1px solid #555;}
 )")
                    .arg(c));
  QPalette p = palette();
  p.setColor(QPalette::Highlight, QColor(c));
  setPalette(p);
}
void MainWindow::setInventory(QList<Gpu> list) {
  gpus = list;
  QSignalBlocker block(gpuSelect);
  gpuSelect->clear();
  for (const auto &g : gpus)
    gpuSelect->addItem(g.name + " · " + g.pci);
  if (gpus.isEmpty())
    gpuSelect->addItem("Nenhuma GPU DRM disponível");
  int preferred=0;QString saved=QSettings().value("graphics/preferredPci").toString();
  for(int n=0;n<gpus.size();++n)if(gpus[n].pci==saved)preferred=n;
  gpuSelect->setCurrentIndex(preferred);chooseGpu(preferred);
  emit configureOverlay(overlayConfig.json());
}
void MainWindow::chooseGpu(int index) {
  selected = index;
  latest = {};
  cachedVulkan = {};
  telemetryHistory = {};
  for (auto *v : values)
    v->setText("N/D");
  emit selectGpu(index);
  applyTheme();
  refreshSpecs();
  refreshHistories();
  refreshFeedback();

  for (auto *input : tuningInputs) input->setEnabled(false);
  for (auto *button : tuningButtons) button->setEnabled(false);
  if (automaticFans) automaticFans->setEnabled(false);
  if (restoreTuningButton) restoreTuningButton->setEnabled(false);
  coreOffset->setEnabled(false);
  memOffset->setEnabled(false);
  applyCore->setEnabled(false);
  applyMem->setEnabled(false);

  if (index >= 0 && index < gpus.size())
    status->setText(offline
                        ? "PRÉVIA · dados fictícios"
                        : gpus[index].name + " · " + gpus[index].driver + " · " + gpus[index].pci);
}
QWidget *MainWindow::dashboard() {
  auto *p = page("GPU · Monitor de recursos");
  auto *l = qobject_cast<QVBoxLayout *>(p->layout());
  auto *grid = new QGridLayout;
  int i = 0;
  for (const auto &m : metricDefinitions()) {
    auto *card = new NeonCard(m.label);
    grid->addWidget(card, i / 3, i % 3);
    metricCards[m.key] = card;
    auto *cl = card->body();
    auto *value = label("N/D", 23);
    values[m.key] = value;
    cl->addWidget(value);
    card->setMinimumHeight(110);
    ++i;
  }
  l->addLayout(grid);
  l->addWidget(label("Leituras do driver da GPU selecionada. N/D indica sensor indisponível; "
                     "uso do sistema não equivale ao uso exclusivo de um jogo."));
  l->addStretch();
  return p;
}
void MainWindow::updateSnapshot(QJsonObject o) {
  if (!gpus.isEmpty() && o["pci"].toString() != gpus.value(selected).pci && !offline)
    return;
  latest = o;
  if(gameFramePlot) gameFramePlot->setSamples(o["frameTimes"].toArray());
  helperInfo->setText(
      offline ? "Visualização offline: ajustes indisponíveis"
      : o["wslReadOnly"].toBool() ? "WSL: sensores reais via NVML; ajustes disponíveis somente em Linux nativo"
      : !trustedHelper()
          ? "Ajustes bloqueados: helper não instalado ou permissões inadequadas; consulte README"
      : !o["temp"].isDouble() ? "Ajustes bloqueados: temperatura confiável indisponível"
                              : "Helper de clocks instalado · autenticação do sistema exigida · "
                                "ajustes conforme suporte do driver");
  refreshTuningControls(o);
  if (values.contains("fanRpm")) {
    bool hwmon = false;
    for (auto fan : o["fans"].toArray())
      if (fan.toObject()["source"].toString() == "hwmon")
        hwmon = true;
    values["fanRpm"]->setToolTip(hwmon ? "Tacômetro exposto pelo hwmon"
                                       : "NVML: velocidade pretendida reportada pelo driver; pode "
                                         "diferir da velocidade física");
  }

  values["usage"]->setToolTip(o["usageSource"].toString("Leitura global do driver"));
  for (auto it = values.begin(); it != values.end(); ++it)
    it.value()->setText(displayMetric(o, it.key()));
  for (auto it = metricCards.begin(); it != metricCards.end(); ++it)
    it.value()->setReading(o, it.key(), QColor(gpus.value(selected).accent()));
  refreshSpecs();
  processInfo->setText(o["processAlive"].toBool()
                           ? QString("%1 · PID %2 · CPU %3% · RAM %4 MiB")
                                 .arg(o["processName"].toString())
                                 .arg(o["processPid"].toInt())
                                 .arg(o.contains("processCpu")
                                          ? QString::number(o["processCpu"].toDouble(), 'f', 1)
                                          : "N/D")
                                 .arg(o.contains("processRamMiB")
                                          ? QString::number(o["processRamMiB"].toDouble(), 'f', 0)
                                          : "N/D")
                           : "Nenhum processo acompanhado · métricas GPU são "
                             "do dispositivo inteiro");
  int row = 0;
  for (const auto &m : metricDefinitions()) {
    processTable->setRowHidden(row,m.key.startsWith("fps")&&!overlayConfig.fields.contains(m.key));
    processTable->setItem(row, 0, new QTableWidgetItem(m.label));
    processTable->setItem(row++, 1, new QTableWidgetItem(displayMetric(o, m.key)));
  }
  if (!sessionClocks.contains(deviceKey(gpus.value(selected))) && o.contains("coreCurrent"))
    sessionClocks[deviceKey(gpus.value(selected))] = o;
  for (QString domain : {"core", "mem"}) {
    auto *slider = domain == "core" ? coreOffset : memOffset;
    auto *apply = domain == "core" ? applyCore : applyMem;
    auto *display = domain == "core" ? coreValue : memValue;
    bool available = o[domain + "Min"].isDouble() && o[domain + "Max"].isDouble() &&
                     o[domain + "Current"].isDouble();
    bool initial = !slider->isEnabled();
    slider->setEnabled(available && !offline && !testActive);
    auto *entry = domain == "core" ? coreEntry : memEntry;
    entry->setEnabled(available && !offline && !testActive);
    if (available) {
      if (auto *validator = qobject_cast<const QIntValidator *>(entry->validator()))
        const_cast<QIntValidator *>(validator)->setRange(o[domain + "Min"].toInt(),
                                                         o[domain + "Max"].toInt());
      if (initial && !entry->hasFocus())
        entry->setText(QString::number(o[domain + "Current"].toInt()));
    }
    apply->setEnabled(available && !offline && !testActive && trustedHelper() &&
                      o["temp"].isDouble());
    if (available) {
      QSignalBlocker block(slider);
      slider->setRange(o[domain + "Min"].toInt(), o[domain + "Max"].toInt());
      if (initial)
        slider->setValue(o[domain + "Current"].toInt());
      display->setText(QString("%1: %2 MHz · atual: %3 MHz · faixa %4–%5")
                           .arg(o[domain + "Mode"].toString() == "offset" ? "Offset" : "Máximo")
                           .arg(slider->value())
                           .arg(o[domain + "Current"].toInt())
                           .arg(slider->minimum())
                           .arg(slider->maximum()));
    } else
      display->setText("Controle não exposto pelo driver");
  }
  trialButton->setEnabled(!offline && !testActive && trustedHelper() &&
                          gpus.value(selected).vendor == "NVIDIA" && o["temp"].isDouble() &&
                          o["temp"].toDouble() < 60 && o["coreCurrent"].isDouble() &&
                          o["memCurrent"].isDouble());
  if (testActive || codecSuite.busy()) {
    telemetryHistory.append(o);
    if (telemetryHistory.size() > 240)
      telemetryHistory.removeAt(0);
  }
  if (o["error"].isString())
    status->setText(o["error"].toString());
}
QWidget *MainWindow::profiles() {
  auto *p = page("Recursos 3D & Filtros · perfis por jogo");
  auto *l = qobject_cast<QVBoxLayout *>(p->layout());
  auto *grid = new QGridLayout;
  auto *identity = group("Jogo / aplicativo", grid, 0, 0, 2);
  auto *form = new QFormLayout(identity);
  profileName = new QLineEdit("Meu jogo");
  profileExe = new QLineEdit;
  profileArgs = new QLineEdit;
  auto *browse = new QPushButton("Selecionar executável");
  form->addRow("Nome do perfil", profileName);
  form->addRow(browse, profileExe);
  form->addRow("Argumentos", profileArgs);
  connect(browse, &QPushButton::clicked, this, [this] {
    QString f = QFileDialog::getOpenFileName(this, "Executável do jogo");
    if (!f.isEmpty())
      profileExe->setText(f);
  });
  auto *sync = group("Monitoramento & limite", grid, 1, 0);
  auto *sl = new QFormLayout(sync);
  profileHud = new QCheckBox("Usar MangoHud no lançamento");
  profileHud->setChecked(true);
  fpsLimit = new QSpinBox;
  fpsLimit->setRange(0, 500);
  fpsLimit->setSuffix(" FPS (0 = livre)");
  sl->addRow(profileHud);
  sl->addRow("Limite de FPS", fpsLimit);
  auto *filters = group("Pós-processamento Vulkan · vkBasalt", grid, 1, 1);
  auto *fl = new QFormLayout(filters);
  profileFilters = new QCheckBox("Ativar filtros reais no lançamento");
  sharp = new QDoubleSpinBox;
  sharp->setRange(0, 1);
  sharp->setSingleStep(.05);
  sharp->setValue(.5);
  filterAA = new QComboBox;
  filterAA->addItems({"Somente nitidez CAS", "CAS + FXAA", "CAS + SMAA"});
  fl->addRow(profileFilters);
  fl->addRow("Nitidez", sharp);
  fl->addRow("Filtro", filterAA);
  auto *upscale = group("Escalonamento espacial · Gamescope", grid, 2, 0);
  auto *ul = new QFormLayout(upscale);
  profileFsr = new QCheckBox("Usar filtro FSR espacial");
  profileResolution = new QComboBox;
  profileResolution->addItems(
      {"1280x720 → 1920x1080", "1920x1080 → 2560x1440", "2560x1440 → 3840x2160"});
  ul->addRow(profileFsr);
  ul->addRow(profileResolution);
  auto *tech = group("Recursos dependentes do jogo", grid, 2, 1);
  auto *tl = new QVBoxLayout(tech);
  tl->addWidget(label("Ray tracing, DLSS, FSR temporal, XeSS, HDR, MSAA e "
                      "anisotropia dependem do jogo, driver e caminho gráfico. "
                      "Configure no jogo; não há ativação global garantida."));
  l->addLayout(grid);
  auto *actions = new QHBoxLayout;
  auto *save = button("Gravar perfil & comando Steam", actions);
  auto *load = button("Carregar perfil", actions);
  connect(load, &QPushButton::clicked, this, [this] {
    if (offline)
      return;
    QString path = QFileDialog::getOpenFileName(this, "Carregar perfil", configRoot() + "/profiles",
                                                "Perfil (profile.json)");
    if (path.isEmpty())
      return;
    auto p = QJsonDocument::fromJson(readText(path).toUtf8()).object();
    if (!p.contains("name") || !p["arguments"].isArray()) {
      notify("Perfil inválido");
      return;
    }
    profileName->setText(p["name"].toString());
    profileExe->setText(p["executable"].toString());
    QStringList args;
    for (auto v : p["arguments"].toArray())
      args << v.toString();
    // QProcess::splitCommand uses triple quotes for a literal quote inside a quoted argument.
    QStringList quoted;
    for (QString arg : args) {
      arg.replace('"', "\"\"\"");
      quoted << "\"" + arg + "\"";
    }
    profileArgs->setText(quoted.join(' '));
    profileHud->setChecked(p["mangohud"].toBool());
    profileFilters->setChecked(p["vkbasalt"].toBool());
    profileFsr->setChecked(p["gamescopeFsr"].toBool());
    fpsLimit->setValue(p["fpsLimit"].toInt());
    sharp->setValue(p["sharpness"].toDouble(.5));
    filterAA->setCurrentIndex(qBound(0, p["aaIndex"].toInt(), 2));
    profileResolution->setCurrentIndex(qBound(0, p["resolutionIndex"].toInt(), 2));
    launchText->setPlainText("Perfil carregado: " + path +
                             "\nGravar cria uma nova cópia e um novo comando de lançamento.");
    notify("Perfil carregado");
  });
  auto *launch = button("Iniciar jogo com perfil", actions);
  l->addLayout(actions);
  launchText = new QPlainTextEdit;
  launchText->setReadOnly(true);
  launchText->setMaximumHeight(135);
  launchText->setPlaceholderText("O comando de inicialização aparecerá após gravar o perfil.");
  l->addWidget(launchText);
  l->addWidget(label("Perfis são isolados e aplicados ao iniciar o jogo. A GPU do "
                     "painel seleciona monitoramento e benchmarks; a escolha da GPU do "
                     "jogo continua a cargo do driver/launcher."));
  l->addStretch();
  connect(save, &QPushButton::clicked, this, &MainWindow::saveProfile);
  connect(launch, &QPushButton::clicked, this, &MainWindow::launchProfile);
  if (offline) {
    save->setEnabled(false);
    load->setEnabled(false);
    launch->setEnabled(false);
  }
  return p;
}
void MainWindow::saveProfile() {
  savedProfile.clear();
  if (offline)
    return;
  QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  QString dir = configRoot() + "/profiles/" + id;
  QDir().mkpath(dir);
  QString error;
  QString effects = filterAA->currentIndex() == 1   ? "cas:fxaa"
                    : filterAA->currentIndex() == 2 ? "cas:smaa"
                                                    : "cas";
  QByteArray mh = QString("fps\nframetime\ngpu_stats\ngpu_temp\nvram\ncpu_"
                          "stats\nram\nposition=top-left\nfont_size=24\nfps_limit=%1\n")
                      .arg(fpsLimit->value())
                      .toUtf8();
  QByteArray vb = QString("effects = %1\ncasSharpness = %2\n")
                      .arg(effects)
                      .arg(sharp->value(), 0, 'f', 2)
                      .toUtf8();
  if (!writeFile(dir + "/MangoHud.conf", mh, error) ||
      !writeFile(dir + "/vkBasalt.conf", vb, error)) {
    notify("Falha ao gravar perfil", error);
    return;
  }
  QStringList env;
  if (profileHud->isChecked())
    env << "MANGOHUD=1" << "MANGOHUD_CONFIGFILE=" + dir + "/MangoHud.conf";
  if (profileFilters->isChecked())
    env << "ENABLE_VKBASALT=1"
        << "VKBASALT_CONFIG_FILE=" + dir + "/vkBasalt.conf";
  QStringList wrapper;
  if (profileFsr->isChecked()) {
    QStringList dimensions =
        profileResolution->currentIndex() == 0   ? QStringList{"1280", "720", "1920", "1080"}
        : profileResolution->currentIndex() == 1 ? QStringList{"1920", "1080", "2560", "1440"}
                                                 : QStringList{"2560", "1440", "3840", "2160"};
    wrapper = {"gamescope",   "-w", dimensions[0], "-h", dimensions[1], "-W",
               dimensions[2], "-H", dimensions[3], "-F", "fsr",         "--"};
  }
  if (profileHud->isChecked())
    wrapper << "mangohud" << "--dlsym";
  // The wrapper receives literal argv; no user-supplied command is evaluated by
  // a shell.
  QString script = "#!/bin/sh\n";
  for (const auto &variable : env)
    script += "export " + quote(variable) + "\n";
  script += "exec ";
  for (const auto &arg : wrapper)
    script += quote(arg) + " ";
  script += "\"$@\"\n";
  QString launcher = dir + "/launch.sh";
  if (!writeFile(launcher, script.toUtf8(), error)) {
    notify("Falha ao gravar launcher", error);
    return;
  }
  if (!QFile::setPermissions(launcher, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                           QFileDevice::ExeOwner)) {
    notify("Falha ao habilitar launcher");
    return;
  }
  QJsonObject profile = {
      {"name", profileName->text()},
      {"executable", profileExe->text()},
      {"arguments", QJsonArray::fromStringList(QProcess::splitCommand(profileArgs->text()))},
      {"environment", QJsonArray::fromStringList(env)},
      {"wrapper", QJsonArray::fromStringList(wrapper)},
      {"gpuMonitored", gpus.value(selected).pci},
      {"mangohud", profileHud->isChecked()},
      {"fpsLimit", fpsLimit->value()},
      {"sharpness", sharp->value()},
      {"aaIndex", filterAA->currentIndex()},
      {"gamescopeFsr", profileFsr->isChecked()},
      {"resolutionIndex", profileResolution->currentIndex()},
      {"vkbasalt", profileFilters->isChecked()}};
  if (!saveJson(dir + "/profile.json", profile, &error)) {
    notify("Falha ao gravar perfil", error);
    return;
  }
  savedProfile = dir;
  launchText->setPlainText("Steam · opções de inicialização:\n" + quote(launcher) +
                           " %command%\n\nPerfil: " + dir +
                           "\nMangoHud: wrapper --dlsym quando habilitado. Flatpak/Steam Runtime "
                           "podem exigir a instalação correspondente dentro do runtime.");
  notify("Perfil gravado", dir);
}
void MainWindow::launchProfile() {
  if (driverBusy) { notify("Aguarde a atualização de drivers terminar"); return; }
  if (offline)
    return;
  saveProfile();
  if (savedProfile.isEmpty())
    return;
  auto o = QJsonDocument::fromJson(readText(savedProfile + "/profile.json").toUtf8()).object();
  QString exe = o["executable"].toString();
  QFileInfo info(exe);
  if (!info.isAbsolute() || !info.isExecutable() || !info.isFile()) {
    notify("Selecione um executável válido");
    return;
  }
  if (o["mangohud"].toBool() && QStandardPaths::findExecutable("mangohud").isEmpty()) {
    notify("MangoHud não encontrado");
    return;
  }
  if (profileFsr->isChecked() && QStandardPaths::findExecutable("gamescope").isEmpty()) {
    notify("Gamescope não encontrado");
    return;
  }
  if (profileFilters->isChecked()) {
    bool present = false;
    for (QString path :
         QStringList{"/usr/share/vulkan/implicit_layer.d", "/etc/vulkan/implicit_layer.d",
                     QDir::homePath() + "/.local/share/vulkan/implicit_layer.d"})
      if (!QDir(path).entryList({"*vkBasalt*.json", "*vkbasalt*.json"}, QDir::Files).isEmpty())
        present = true;
    if (!present) {
      notify("vkBasalt não encontrado; perfil de filtros não pode ser aplicado");
      return;
    }
  }
  QStringList args;
  for (auto arg : o["arguments"].toArray())
    args << arg.toString();
  args.prepend(exe);
  QProcess game;
  auto gameEnv=nativeEnvironment();QString preferred=QSettings().value("graphics/preferredPci").toString();
  for(auto g:gpus)if(!preferred.isEmpty()&&g.pci==preferred) {
    if(g.vendor=="NVIDIA"){gameEnv.insert("__NV_PRIME_RENDER_OFFLOAD","1");gameEnv.insert("__GLX_VENDOR_LIBRARY_NAME","nvidia");gameEnv.insert("__VK_LAYER_NV_optimus","NVIDIA_only");}
    else {gameEnv.insert("DRI_PRIME","pci-"+QString(g.pci).replace(':','_').replace('.','_'));gameEnv.remove("__NV_PRIME_RENDER_OFFLOAD");gameEnv.remove("__GLX_VENDOR_LIBRARY_NAME");gameEnv.remove("__VK_LAYER_NV_optimus");}
  }
  game.setProcessEnvironment(gameEnv);
  game.setProgram(savedProfile + "/launch.sh");
  game.setArguments(args);
  game.setWorkingDirectory(info.absolutePath());
  qint64 pid = 0;
  if (!game.startDetached(&pid)) {
    notify("Falha ao iniciar jogo");
    return;
  }
  emit trackProcess(int(pid));
  notify("Jogo iniciado com perfil",
         QString("PID inicial %1; selecione o processo do jogo se o launcher "
                 "criar outro PID")
             .arg(pid));
}
QWidget *MainWindow::monitor() {
  auto *p = page("GamerMonitor & Overlay Nativo");
  auto *l = qobject_cast<QVBoxLayout *>(p->layout());
  auto *bar = new QHBoxLayout;
  processSelect = new QComboBox;
  processSelect->setMinimumWidth(220);
  bar->addWidget(processSelect, 1);
  auto *refresh = button("Atualizar processos", bar);
  auto *track = button("Monitorar PID", bar);
  auto *stop = button("Parar", bar);
  l->addLayout(bar);
  processInfo = label("Nenhum processo acompanhado");
  l->addWidget(processInfo);
  connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshProcesses);
  connect(track, &QPushButton::clicked, this, [this] {
    if (!offline)
      emit trackProcess(processSelect->currentData().toInt());
  });
  connect(stop, &QPushButton::clicked, this, [this] { emit trackProcess(0); });
  auto *config = new QGroupBox("Overlay GTK · independente do painel");
  auto *form = new QGridLayout(config);
  hudEnabled = new QCheckBox("Exibir HUD");
  hudEnabled->setChecked(false);
  onlyGame = new QCheckBox("Somente com processo acompanhado ativo");
  onlyGame->setChecked(overlayConfig.onlyGame);
  form->addWidget(hudEnabled, 0, 0);
  form->addWidget(onlyGame, 0, 1, 1, 3);
  layoutChoice = new QComboBox;
  layoutChoice->addItem("Lista vertical", "list");
  layoutChoice->addItem("Faixa compacta", "compact");
  layoutChoice->addItem("Duas colunas", "columns");
  layoutChoice->setCurrentIndex(qMax(0, layoutChoice->findData(overlayConfig.layout)));
  anchorChoice = new QComboBox;
  for (auto item : QList<QPair<QString, QString>>{{"Superior esquerdo", "top-left"},
                                                  {"Superior direito", "top-right"},
                                                  {"Superior centralizado", "top-center"},
                                                  {"Inferior esquerdo", "bottom-left"},
                                                  {"Inferior direito", "bottom-right"},
                                                  {"Centro", "center"},
                                                  {"Coordenadas no monitor", "custom"}})
    anchorChoice->addItem(item.first, item.second);
  anchorChoice->setCurrentIndex(qMax(0, anchorChoice->findData(overlayConfig.anchor)));
  monitorChoice = new QComboBox;
  int n = 0;
  for (auto *s : QGuiApplication::screens())
    monitorChoice->addItem(QString("%1 · %2").arg(n++).arg(s->name()));
  monitorChoice->setCurrentIndex(
      qBound(0, overlayConfig.monitor, qMax(0, monitorChoice->count() - 1)));
  form->addWidget(label("Formato"), 1, 0);
  form->addWidget(layoutChoice, 1, 1);
  form->addWidget(label("Posição"), 1, 2);
  form->addWidget(anchorChoice, 1, 3);
  form->addWidget(label("Monitor"), 2, 0);
  form->addWidget(monitorChoice, 2, 1, 1, 3);
  auto spin = [](int lo, int hi, int v) {
    auto *s = new QSpinBox;
    s->setRange(lo, hi);
    s->setValue(v);
    return s;
  };
  hudX = spin(-16384, 16384, overlayConfig.x);
  hudY = spin(-16384, 16384, overlayConfig.y);
  hudWidth = spin(220, 1600, overlayConfig.width);
  hudFont = spin(9, 40, overlayConfig.font);
  hudOpacity = spin(0, 100, overlayConfig.opacity);
  form->addWidget(label("Deslocamento X"), 3, 0);
  form->addWidget(hudX, 3, 1);
  form->addWidget(label("Deslocamento Y"), 3, 2);
  form->addWidget(hudY, 3, 3);
  form->addWidget(label("Largura (px)"), 4, 0);
  form->addWidget(hudWidth, 4, 1);
  form->addWidget(label("Fonte (px)"), 4, 2);
  form->addWidget(hudFont, 4, 3);
  form->addWidget(label("Fundo (%)"), 5, 0);
  form->addWidget(hudOpacity, 5, 1);
  auto *fieldGrid = new QGridLayout;
  int i = 0;
  for (const auto &m : metricDefinitions()) {
    auto *check = new QCheckBox(m.label);
    check->setChecked(overlayConfig.fields.contains(m.key));
    hudFields[m.key] = check;
    fieldGrid->addWidget(check, i / 4, i % 4);
    ++i;
  }
  form->addLayout(fieldGrid, 6, 0, 1, 4);
  hudFrametime = new QCheckBox("Linha de frametime (ms, fonte CSV real)");
  hudFrametime->setChecked(overlayConfig.frametimeGraph);
  hudLogFile = new QLineEdit(overlayConfig.frametimeLog);
  hudLogFile->setPlaceholderText("FPS e frametime: CSV ativo do jogo, com coluna frametime (ms)");
  auto *browseLog = new QPushButton("Selecionar CSV");
  form->addWidget(hudFrametime,7,0,1,4);
  form->addWidget(hudLogFile,8,0,1,3);
  form->addWidget(browseLog,8,3);
  connect(browseLog,&QPushButton::clicked,this,[this] {
    auto path=QFileDialog::getOpenFileName(this,"Log de frametime do jogo",QDir::homePath(),"CSV (*.csv)");
    if(!path.isEmpty())hudLogFile->setText(path);
  });
  form->addWidget(label("Mudanças são aplicadas em tempo real. Os campos FPS selecionados aparecem no painel e no HUD. FPS/frametime: selecione um CSV em gravação. Média, máximo e 1% low usam até 240 amostras; low exige 100. Logs por intervalo não equivalem a dados por quadro. Sem fonte ativa, N/D."),9,0,1,4);
  auto *apply = new QPushButton("Aplicar formato e posição");
  form->addWidget(apply, 10, 0, 1, 4);
  l->addWidget(config);
  gameFramePlot=new HistoryPlot; gameFramePlot->caption="Frametime do jogo (ms) · últimas 240 amostras CSV";
  gameFramePlot->setMinimumHeight(150); l->addWidget(gameFramePlot);
  l->addWidget(label("X/Y são margens para cantos e deslocamentos para o centro. O HUD não "
                     "captura cliques. Alt+Q alterna sua visibilidade (X11). X11/XWayland é "
                     "necessário; fullscreen e composição dependem do ambiente."));
  processTable = new QTableWidget(metricDefinitions().size(), 2);
  processTable->setMinimumHeight(440);
  processTable->setHorizontalHeaderLabels({"Métrica do dispositivo", "Valor"});
  processTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  processTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  l->addWidget(processTable, 1);
  connect(apply, &QPushButton::clicked, this, &MainWindow::saveOverlay);
  connect(hudEnabled, &QCheckBox::toggled, this, [this] { saveOverlay(); });
  hudEditTimer.setSingleShot(true);
  hudEditTimer.setInterval(120);
  connect(&hudEditTimer,&QTimer::timeout,this,&MainWindow::saveOverlay);
  auto schedule=[this] { hudEditTimer.start(); };
  for(auto *combo:{layoutChoice,anchorChoice,monitorChoice})
    connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),this,schedule);
  for(auto *spin:{hudX,hudY,hudWidth,hudFont,hudOpacity})
    connect(spin,qOverload<int>(&QSpinBox::valueChanged),this,schedule);
  connect(onlyGame,&QCheckBox::toggled,this,schedule);
  connect(hudFrametime,&QCheckBox::toggled,this,schedule);
  connect(hudLogFile,&QLineEdit::textChanged,this,schedule);
  for(auto *field:hudFields)connect(field,&QCheckBox::toggled,this,schedule);
  if (offline) {
    apply->setEnabled(false);
    hudEnabled->setEnabled(false);
    track->setEnabled(false);
    refresh->setEnabled(false);
  }
  return p;
}
void MainWindow::saveOverlay() {
  if (!overlayConfig.enabled && hudEnabled->isChecked())
    hudFailures = 0;
  overlayConfig.enabled = hudEnabled->isChecked();
  overlayConfig.onlyGame = onlyGame->isChecked();
  overlayConfig.monitor = monitorChoice->currentIndex();
  overlayConfig.layout = layoutChoice->currentData().toString();
  overlayConfig.anchor = anchorChoice->currentData().toString();
  overlayConfig.x = hudX->value();
  overlayConfig.y = hudY->value();
  overlayConfig.width = hudWidth->value();
  overlayConfig.font = hudFont->value();
  overlayConfig.opacity = hudOpacity->value();
  overlayConfig.frametimeGraph = hudFrametime->isChecked();
  overlayConfig.frametimeLog = hudLogFile->text().trimmed();
  overlayConfig.fields.clear();
  for (const auto &m : metricDefinitions())
    if (hudFields[m.key]->isChecked())
      overlayConfig.fields << m.key;
  int metricRow=0;
  for(const auto &m:metricDefinitions()) {if(m.key.startsWith("fps"))processTable->setRowHidden(metricRow,!overlayConfig.fields.contains(m.key));++metricRow;}
  QString error;
  if (!offline && !saveJson(configRoot() + "/overlay.json", overlayConfig.json(), &error))
    notify("Configuração não salva", error);
  emit configureOverlay(overlayConfig.json());
  if (overlayConfig.enabled && hud.state() == QProcess::NotRunning)
    startHud();
}
void MainWindow::startHud() {
  if (offline || closing || hud.state() != QProcess::NotRunning)
    return;
  if (restartClock.isValid() && restartClock.elapsed() < 3000)
    return;
  restartClock.restart();
  hudToken.clear();
  for (int i = 0; i < 8; ++i)
    hudToken += QString::number(QRandomGenerator::system()->generate(), 16).rightJustified(8, '0');
  hudReady = false;
  hudOutput.clear();
  hud.setProgram(QCoreApplication::applicationFilePath());
  hud.setArguments({"--overlay-child", "--owner",
                    QString::number(QCoreApplication::applicationPid()), "--token", hudToken});
  hud.start();
  QString generation = hudToken;
  QTimer::singleShot(5000, this, [this, generation] {
    if (hudToken == generation && !hudReady && hud.state() != QProcess::NotRunning) {
      log("HUD não sinalizou prontidão em 5 s");
      hud.kill();
    }
  });
}
void MainWindow::refreshProcesses() {
  if (offline)
    return;
  processSelect->clear();
  for (auto entry : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    bool ok;
    int pid = entry.toInt(&ok);
    if (!ok || pid <= 0)
      continue;
    struct stat st{};
    QString path = "/proc/" + entry;
    if (stat(QFile::encodeName(path).constData(), &st) != 0 || st.st_uid != getuid())
      continue;
    QString name = readText(path + "/comm");
    if (!name.isEmpty())
      processSelect->addItem(name + " · " + entry, pid);
  }
  notify("Lista de processos atualizada");
}
QWidget *MainWindow::displays() {
  auto *p = page("Configurações de Monitores & Telas");
  auto *l = qobject_cast<QVBoxLayout *>(p->layout());
  auto *info = new QGroupBox("Monitores reconhecidos pelo Qt");
  auto *il = new QVBoxLayout(info);
  for (auto *s : QGuiApplication::screens())
    il->addWidget(label(QString("%1 · %2 × %3 · %4 Hz · escala %5")
                            .arg(s->name())
                            .arg(s->size().width())
                            .arg(s->size().height())
                            .arg(s->refreshRate(), 0, 'f', 2)
                            .arg(s->devicePixelRatio(), 0, 'f', 2)));
  l->addWidget(info);
  auto *controls = new QGroupBox("Resolução & taxa de atualização");
  auto *form = new QFormLayout(controls);
  displayChoice = new QComboBox;
  modeChoice = new QComboBox;
  auto *refresh = button("Consultar modos do sistema");
  auto *apply = button("Aplicar modo (reversão em 15 s)");
  displayInfo = label("X11: xrandr. KDE Wayland: kscreen-doctor. Outros compositores: "
                      "use as configurações de tela do ambiente.");
  form->addRow(refresh);
  form->addRow("Saída", displayChoice);
  form->addRow("Modo", modeChoice);
  form->addRow(apply);
  form->addRow(displayInfo);
  l->addWidget(controls);
  l->addWidget(label("Cor, HDR, VRR e áudio não são alterados nesta versão. Gamma não é "
                     "usado como substituto de saturação. A lista de monitores pertence "
                     "à sessão gráfica, não exclusivamente à GPU selecionada."));
  l->addStretch();
  connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshDisplays);
  connect(apply, &QPushButton::clicked, this, &MainWindow::applyDisplay);
  connect(displayChoice, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
    modeChoice->clear();
    if (idx < 0 || idx >= displayOutputs.size())
      return;
    auto modes = displayOutputs[idx].toObject()["modes"].toArray();
    for (auto v : modes) {
      auto m = v.toObject();
      modeChoice->addItem(m["label"].toString(), m);
      if (m["current"].toBool())
        modeChoice->setCurrentIndex(modeChoice->count() - 1);
    }
  });
  if (offline) {
    refresh->setEnabled(false);
    apply->setEnabled(false);
  }
  return p;
}
void MainWindow::refreshDisplays() {
  if (offline || displayBusy)
    return;
  bool wayland = QGuiApplication::platformName().startsWith("wayland");
  QString exe = wayland ? "kscreen-doctor" : "xrandr";
  if (wayland &&
      !qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("KDE", Qt::CaseInsensitive)) {
    notify("Compositor sem backend de modos nesta versão");
    return;
  }
  if (QStandardPaths::findExecutable(exe).isEmpty()) {
    notify("Ferramenta indisponível", exe);
    return;
  }
  auto *query = new QProcess(this);
  displayBusy = true;
  connect(query, &QProcess::errorOccurred, this, [this, query](QProcess::ProcessError e) {
    if (e == QProcess::FailedToStart) {
      displayBusy = false;
      notify("Falha na consulta de telas", query->errorString());
      query->deleteLater();
    }
  });
  connect(query, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, query, wayland](int code, QProcess::ExitStatus s) {
            displayBusy = false;
            QByteArray data = query->readAllStandardOutput();
            QString err = QString::fromUtf8(query->readAllStandardError());
            query->deleteLater();
            if (code != 0 || s != QProcess::NormalExit) {
              notify("Falha na consulta de telas", err);
              return;
            }
            displayOutputs = {};
            if (wayland) {
              auto root = QJsonDocument::fromJson(data).object();
              for (auto v : root["outputs"].toArray()) {
                auto o = v.toObject();
                if (!o["connected"].toBool() || !o["enabled"].toBool())
                  continue;
                QJsonArray modes;
                QString current = o["currentModeId"].toString();
                for (auto mv : o["modes"].toArray()) {
                  auto m = mv.toObject(), size = m["size"].toObject();
                  QString id = m["id"].toString();
                  modes.append(
                      QJsonObject{{"id", id},
                                  {"label", QString("%1 × %2 · %3 Hz")
                                                .arg(size["width"].toInt())
                                                .arg(size["height"].toInt())
                                                .arg(m["refreshRate"].toDouble(), 0, 'f', 2)},
                                  {"current", id == current}});
                }
                displayOutputs.append(QJsonObject{{"name", o["name"]},
                                                  {"id", o["id"]},
                                                  {"backend", "kscreen-doctor"},
                                                  {"modes", modes}});
              }
            } else {
              QJsonObject output;
              QJsonArray modes;
              auto flush = [&] {
                if (!output.isEmpty()) {
                  output["modes"] = modes;
                  displayOutputs.append(output);
                }
                output = {};
                modes = {};
              };
              for (auto line : QString::fromUtf8(data).split('\n')) {
                if (!line.startsWith(' ')) {
                  flush();
                  auto parts = line.split(' ', Qt::SkipEmptyParts);
                  if (parts.size() > 1 && parts[1] == "connected")
                    output = {{"name", parts[0]}, {"backend", "xrandr"}};
                } else if (!output.isEmpty()) {
                  auto parts = line.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                  if (parts.size() > 1 &&
                      QRegularExpression("^[0-9]+x[0-9]+$").match(parts[0]).hasMatch()) {
                    for (int i = 1; i < parts.size(); ++i) {
                      QString rate = parts[i];
                      bool current = rate.contains('*');
                      rate.remove('*');
                      rate.remove('+');
                      bool valid;
                      rate.toDouble(&valid);
                      if (valid)
                        modes.append(QJsonObject{{"id", parts[0]},
                                                 {"rate", rate},
                                                 {"label", parts[0] + " · " + rate + " Hz"},
                                                 {"current", current}});
                    }
                  }
                }
              }
              flush();
            }
            displayChoice->clear();
            for (auto v : displayOutputs)
              displayChoice->addItem(v.toObject()["name"].toString());
            if (displayOutputs.isEmpty())
              notify("Nenhuma saída ativa consultável");
            else
              displayInfo->setText("Modos consultados. Alterações exigem "
                                   "confirmação e revertem em 15 segundos.");
          });
  QTimer::singleShot(5000, query, [query] {
    if (query->state() != QProcess::NotRunning)
      query->kill();
  });
  query->start(exe, wayland ? QStringList{"-j"} : QStringList{"--query"});
}
void MainWindow::applyDisplay() {
  if (offline || displayBusy || displayChoice->currentIndex() < 0)
    return;
  auto o = displayOutputs[displayChoice->currentIndex()].toObject();
  auto mode = modeChoice->currentData().toJsonObject();
  if (mode.isEmpty())
    return;
  QJsonObject old;
  for (auto v : o["modes"].toArray())
    if (v.toObject()["current"].toBool())
      old = v.toObject();
  if (old.isEmpty()) {
    notify("Modo atual desconhecido; alteração recusada para permitir reversão");
    return;
  }
  QString exe = o["backend"].toString();
  auto arguments = [o, exe](QJsonObject m) {
    return exe == "xrandr"
               ? QStringList{"--output", o["name"].toString(), "--mode", m["id"].toString(),
                             "--rate",   m["rate"].toString()}
               : QStringList{"output." + QString::number(o["id"].toInt()) + ".mode." +
                             m["id"].toString()};
  };
  QStringList revert = arguments(old);
  displayBusy = true;
  auto *change = new QProcess(this);
  auto rollback = [this, exe, revert] {
    auto *restore = new QProcess(this);
    connect(restore, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, restore](int code, QProcess::ExitStatus s) {
              displayBusy = false;
              notify(code == 0 && s == QProcess::NormalExit ? "Modo anterior restaurado"
                                                            : "Falha ao restaurar modo",
                     QString::fromUtf8(restore->readAllStandardError()));
              restore->deleteLater();
              refreshDisplays();
            });
    connect(restore, &QProcess::errorOccurred, this, [this, restore](QProcess::ProcessError e) {
      if (e == QProcess::FailedToStart) {
        displayBusy = false;
        notify("Falha ao iniciar restauração", restore->errorString());
        restore->deleteLater();
      }
    });
    QTimer::singleShot(5000, restore, [restore] {
      if (restore->state() != QProcess::NotRunning)
        restore->kill();
    });
    restore->start(exe, revert);
  };
  connect(change, &QProcess::errorOccurred, this, [this, change](QProcess::ProcessError e) {
    if (e == QProcess::FailedToStart) {
      displayBusy = false;
      notify("Falha ao iniciar alteração", change->errorString());
      change->deleteLater();
    }
  });
  connect(change, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, change, rollback](int code, QProcess::ExitStatus s) {
            QString error = QString::fromUtf8(change->readAllStandardError());
            change->deleteLater();
            if (code != 0 || s != QProcess::NormalExit) {
              notify("Falha ao aplicar modo", error);
              rollback();
              return;
            }
            QMessageBox confirm(QMessageBox::Question, "Confirmar modo",
                                "Manter este modo? Reversão automática em 15 segundos.",
                                QMessageBox::Yes | QMessageBox::No, this);
            confirm.setDefaultButton(QMessageBox::No);
            QTimer::singleShot(15000, &confirm, &QDialog::reject);
            if (confirm.exec() == QMessageBox::Yes) {
              displayBusy = false;
              notify("Modo de tela confirmado");
              refreshDisplays();
            } else
              rollback();
          });
  QTimer::singleShot(5000, change, [change] {
    if (change->state() != QProcess::NotRunning)
      change->kill();
  });
  change->start(exe, arguments(mode));
}

void MainWindow::previewScrollBottom() {
  if (!offline) return;
  QWidget *current = pages->currentWidget();
  if (auto *tabs = qobject_cast<QTabWidget *>(current)) current = tabs->currentWidget();
  if (auto *scroll = qobject_cast<QScrollArea *>(current)) {
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    QApplication::processEvents();
  }
}
bool MainWindow::checkInterface() {
  for (auto file : {":/assets/gpu-pro.png", ":/LICENSE", ":/NOTICE", ":/docs/licenses/LGPL-3.0.txt", ":/docs/licenses/GPL-3.0.txt"})
    if (!QFileInfo::exists(file)) return false;
  if (!offline || pages->count() != 10 || navigation.size() != 10 || gpuTabs->count() != 2 || benchmarkTabs->count() != 4) return false;
  resize(980, 700); pages->setCurrentIndex(2); QApplication::processEvents();
  auto *scroll = qobject_cast<QScrollArea *>(pages->currentWidget());
  if (!scroll || scroll->verticalScrollBar()->maximum() <= 0) return false;
  previewScrollBottom();
  QPoint bottom = processTable->mapTo(scroll->viewport(), QPoint(0, processTable->height() - 1));
  if (bottom.y() >= scroll->viewport()->height() || bottom.y() < 0) return false;
  for (int i = 0; i < pages->count(); ++i) { pages->setCurrentIndex(i); QApplication::processEvents(); }
  for (int i = 0; i < benchmarkTabs->count(); ++i) {
    pages->setCurrentIndex(5); benchmarkTabs->setCurrentIndex(i); QApplication::processEvents();
    if (!qobject_cast<QScrollArea *>(benchmarkTabs->currentWidget())) return false;
  }
  resize(1240,900); pages->setCurrentIndex(0); gpuTabs->setCurrentIndex(1);
  QApplication::processEvents(); adaptSpecs(); QApplication::processEvents();
  auto *specScroll = qobject_cast<QScrollArea *>(gpuTabs->currentWidget());
  if (!specScroll || specScroll->verticalScrollBar()->maximum()>1) return false;
  return true;
}

void MainWindow::benchmarkCheck(QString path) {
  if (offline) return;
  benchmarkCheckFile = QFileInfo(path).absoluteFilePath();
  auto *ready = new QTimer(this); ready->setInterval(250);
  auto tries = std::make_shared<int>(0);
  connect(ready, &QTimer::timeout, this, [this, ready, tries] {
    if (++*tries > 40) { ready->stop(); QCoreApplication::exit(6); return; }
    if (gpus.isEmpty()) return;
    ready->stop(); ready->deleteLater(); pages->setCurrentIndex(5); benchmarkTabs->setCurrentIndex(0);
    startGraphics("graphics", "Vulkan", 1280, 720, 6);
  }); ready->start();
}

void MainWindow::showFromTray() {
  if(isMinimized())setWindowState(windowState() & ~Qt::WindowMinimized);
  show();raise();activateWindow();
}
void MainWindow::updateBranding(QColor accent) {
  if(brandingAccent==accent)return;
  brandingAccent=accent;
  auto art=Branding::windowIcon(accent);setWindowIcon(art);QApplication::setWindowIcon(art);
  trayFrames.clear();
  for(int i=0;i<12;++i)trayFrames.append(Branding::trayIcon(accent,.5+.4*std::sin(i*6.283185307/12)));
  trayFrame=0;if(tray)tray->setIcon(trayFrames.first());
}
void MainWindow::setupTray() {
  tray = new QSystemTrayIcon(Branding::trayIcon(QColor("#76b900")), this);
  tray->setToolTip("GPU-PRO · Monitoramento MultiGPU");
  auto *menu = new QMenu(this);
  auto *open = menu->addAction("Mostrar GPU-PRO");
  connect(open, &QAction::triggered, this, &MainWindow::showFromTray);
  auto *hideAction = menu->addAction("Ocultar na bandeja");
  connect(hideAction, &QAction::triggered, this, [this] { if (QSystemTrayIcon::isSystemTrayAvailable()) hide(); });
  trayMotion=menu->addAction("Animar neon do ícone GP");trayMotion->setCheckable(true);
  trayMotion->setChecked(offline || QSettings().value("appearance/trayNeon",true).toBool());
  trayAnimationTimer.setInterval(200);
  connect(&trayAnimationTimer,&QTimer::timeout,this,[this] {
    if(!tray->isVisible() || trayFrames.isEmpty())return;
    trayFrame=(trayFrame+1)%trayFrames.size();tray->setIcon(trayFrames[trayFrame]);
  });
  connect(trayMotion,&QAction::toggled,this,[this](bool enabled) {
    if(!offline)QSettings().setValue("appearance/trayNeon",enabled);
    if(enabled && tray->isVisible())trayAnimationTimer.start();else trayAnimationTimer.stop();
    if(!enabled && !trayFrames.isEmpty())tray->setIcon(trayFrames.first());
  });
  connect(menu,&QMenu::aboutToShow,&trayAnimationTimer,&QTimer::stop);
  connect(menu,&QMenu::aboutToHide,this,[this]{if(trayMotion->isChecked() && tray->isVisible())trayAnimationTimer.start();});
  menu->addSeparator(); auto *quit = menu->addAction("Sair do GPU-PRO");
  quit->setObjectName("quitFromTray");
  connect(quit, &QAction::triggered, this, [this] {
    explicitQuit = true;
    const bool accepted = close();
    explicitQuit = false;
    // Closing an already hidden window need not emit lastWindowClosed.
    // Respect closeEvent guards, then explicitly stop the application loop.
    if (accepted) QCoreApplication::quit();
    else showFromTray();
  });
  tray->setContextMenu(menu);
  connect(tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) showFromTray();
  });
  if (!offline && QSystemTrayIcon::isSystemTrayAvailable()) {tray->show();if(trayMotion->isChecked())trayAnimationTimer.start();}
}


bool MainWindow::checkInteractions() {
  if(!offline || QApplication::style()->objectName().toLower()!="fusion") return false;
  showMaximized();hide();
  if(!QMetaObject::invokeMethod(this,"activateExistingInstance",Qt::DirectConnection) || !isVisible() || !isMaximized())return false;
  showNormal();
  int packets=0;QJsonObject packet;
  auto connection=connect(this,&MainWindow::configureOverlay,this,[&](QJsonObject p){++packets;packet=p;});
  const int width=hudWidth->value()+20;
  hudWidth->setValue(width);
  hudFrametime->setChecked(true);
  hudFields["cpuUsage"]->setChecked(true);
  hudFields["cpuTemp"]->setChecked(true);
  for(QString key:{"fps","fpsLow","fpsAvg","fpsMax"}) {if(!hudFields.contains(key))return false;hudFields[key]->setChecked(true);}
  for(auto *input:tuningInputs)if(input->isEnabled())return false;
  if(!trialButton->isHidden()||!stopTrial->isHidden()||!gameFramePlot)return false;
  QEventLoop loop;QTimer::singleShot(250,&loop,&QEventLoop::quit);loop.exec();
  disconnect(connection);
  if(packets!=1 || packet["width"].toInt()!=width || !packet["frametimeGraph"].toBool() || !packet["fields"].toArray().contains("cpuTemp")) return false;
  for(QString key:{"fps","fpsLow","fpsAvg","fpsMax"})if(!packet["fields"].toArray().contains(key))return false;
  bool initiallyDisabled=false;
  QTimer::singleShot(0,this,[&]{
    auto *d=findChild<QDialog *>("overclockDisclaimer");
    if(!d)return;
    auto *b=d->findChild<QDialogButtonBox *>();
    initiallyDisabled=b && !b->button(QDialogButtonBox::Ok)->isEnabled();
    d->reject();
  });
  if(confirmOverclock() || !initiallyDisabled || overclockAccepted) return false;
  bool explicitlyEnabled=false;
  QTimer::singleShot(0,this,[&]{
    auto *d=findChild<QDialog *>("overclockDisclaimer");if(!d)return;
    auto *check=d->findChild<QCheckBox *>("acceptOverclockRisk");
    auto *buttons=d->findChild<QDialogButtonBox *>();
    if(check && buttons) {check->setChecked(true);explicitlyEnabled=buttons->button(QDialogButtonBox::Ok)->isEnabled();buttons->button(QDialogButtonBox::Ok)->click();}
    else d->reject();
  });
  return confirmOverclock() && explicitlyEnabled;
}
