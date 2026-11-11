#pragma once
#include "benchmark.h"
#include "codec_suite.h"
#include "repository.h"
#include "service.h"
#include "neon_card.h"
#include <QtWidgets>
class HistoryPlot : public QWidget {
  QList<double> samples;

public:
  using QWidget::QWidget;
  void add(double n) {
    samples << n;
    if (samples.size() > 300)
      samples.removeFirst();
    update();
  }
  void clear() {
    samples.clear();
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override;
};
class MainWindow : public QMainWindow {
  Q_OBJECT
  bool offline = false, closing = false, testActive = false;
  QThread telemetryThread;
  TelemetryService *service = nullptr;
  QList<Gpu> gpus;
  QJsonObject latest;
  QJsonArray telemetryHistory;
  int selected = 0;
  QComboBox *gpuSelect;
  QStackedWidget *pages;
  QList<QPushButton *> navigation;
  QListWidget *notifications;
  QPlainTextEdit *logs;
  QLabel *status;
  QMap<QString, QLabel *> values;
  QGridLayout *specGrid = nullptr;
  QWidget *specHost = nullptr, *specContent = nullptr, *specViewport = nullptr;
  QStringList specOrder, arrangedSpecs;
  int specColumns = 0;
  bool specAdaptPending = false;
  QMap<QString, QLabel *> specValues;
  QJsonObject cachedVulkan;
  QTabWidget *gpuTabs = nullptr, *benchmarkTabs = nullptr;
  QMap<QString, NeonCard *> metricCards;
  QTableWidget *processTable;
  QComboBox *processSelect;
  QLabel *processInfo;
  OverlayConfig overlayConfig;
  QProcess hud;
  QByteArray hudOutput;
  QString hudToken;
  bool hudReady = false;
  int hudFailures = 0;
  QElapsedTimer restartClock;
  QTimer supervise;
  CodecSuite codecSuite;
  std::unique_ptr<Repository> repository;
  struct TestPanel {
    QWidget *root = nullptr;
    HistoryPlot *plot = nullptr;
    QProgressBar *progress = nullptr;
    QLabel *result = nullptr;
    QListWidget *history = nullptr;
    QLabel *preview = nullptr;
    QPushButton *start = nullptr;
    QPushButton *cancel = nullptr;
  };
  QMap<QString, TestPanel> panels;
  QComboBox *graphicsApi, *graphicsResolution, *graphicsDuration;
  QComboBox *bottleneckResolution = nullptr;
  QSize bottleneckTarget{2560, 1440};
  QProcess graphicsProcess;
  QByteArray graphicsOutput;
  std::unique_ptr<QTemporaryDir> graphicsFiles;
  QTimer graphicsWatchdog;
  QString activeKind;
  QString clockCheckFile, benchmarkCheckFile;
  int liveFrameCount = 0;
  QJsonObject graphicsResult;
  QJsonArray bottleneckStages;
  int bottleneckStage = 0;
  QJsonObject trialBaseline, trialGraphics, trialLease;
  bool trialActive = false, trialRestoring = false, trialCancelled = false;
  QByteArray clockOutput;
  QPushButton *trialButton;
  QPushButton *stopTrial;
  QComboBox *clockPresets;
  QLabel *coreValue, *memValue, *feedbackInfo;
  QMap<QString, QJsonObject> sessionClocks;
  QCheckBox *hudEnabled, *onlyGame, *profileHud, *profileFilters, *profileFsr;
  QComboBox *layoutChoice, *anchorChoice, *monitorChoice, *filterAA, *profileResolution;
  QSpinBox *hudX, *hudY, *hudWidth, *hudFont, *hudOpacity, *fpsLimit;
  QDoubleSpinBox *sharp;
  QMap<QString, QCheckBox *> hudFields;
  QLineEdit *profileName, *profileExe, *profileArgs;
  QPlainTextEdit *launchText;
  QJsonObject lastResult;
  QString savedProfile;
  QProcess clockProcess;
  QProcess driverTerminal;
  bool driverBusy = false;
  QWidget *driversControls();
  QWidget *systemControls();
  void startNativeTransaction(QStringList,std::function<void(int)>);
  QSlider *coreOffset, *memOffset;
  QLineEdit *coreEntry, *memEntry;
  QPushButton *applyCore, *applyMem;
  QLabel *clockInfo;
  QLabel *helperInfo;
  QComboBox *displayChoice;
  QComboBox *modeChoice;
  QLabel *displayInfo;
  QJsonArray displayOutputs;
  bool displayBusy = false;
  QSystemTrayIcon *tray = nullptr;
  QCheckBox *keepInTray = nullptr;
  bool explicitQuit = false;
  bool overclockAccepted = false;
  bool confirmOverclock();
  QTimer hudEditTimer;
  QCheckBox *hudFrametime = nullptr;
  QLineEdit *hudLogFile = nullptr;
  void setupTray();
  void showFromTray();
  void adaptSpecs();
  void queueSpecAdapt();
  QWidget *page(QString title);
  QWidget *dashboard();
  QWidget *scrollPage(QWidget *content);
  QWidget *updatesPage();
  QWidget *profiles();
  QWidget *feedbackPage();
  QWidget *videoPage();
  QWidget *bottleneckPage();
  QWidget *monitor();
  QWidget *displays();
  QWidget *energy();
  QWidget *benchmarks();
  QWidget *specs();
  void setInventory(QList<Gpu>);
  void chooseGpu(int);
  void updateSnapshot(QJsonObject);
  void notify(QString, QString = {});
  void log(QString);
  void applyTheme();
  void saveOverlay();
  void startHud();
  void refreshProcesses();
  void refreshSpecs(bool vulkan = false);
  void saveProfile();
  void launchProfile();
  TestPanel makeTestPanel(QString title, QString kind);
  void runBenchmark();
  void startGraphics(QString kind, QString api, int width, int height, int seconds,
                     int cpuIterations = 0);
  void handleGraphics(QJsonObject);
  void refreshHistories();
  void presentRecord(QString, const QJsonObject &);
  void exportResultImage(QString kind);
  QImage renderReport(QString, const QJsonObject &);
  void generateFeedback();
  void importFeedback();
  void refreshFeedback();
  void refreshClockPresets();
  void startClockTrial();
  void handleClockLine(QByteArray);
  void finishClockTrial();

  void finishTest(QJsonObject);
  void setTesting(bool);
  void refreshDisplays();
  void applyDisplay();

public:
  explicit MainWindow(bool preview = false);
  ~MainWindow();
  void clockCheck(QString path);
  void benchmarkCheck(QString path);
  bool checkInterface();
  bool checkInteractions();
  void previewScrollBottom();
  bool viewReport(QString path);
  bool exportViewedImage(QString path);
  bool exportViewedFeedback(QString path);
  void previewSubtab(int index) {
    if (offline)
      if (auto *tabs = pages->currentIndex() == 0 ? gpuTabs : benchmarkTabs)
        tabs->setCurrentIndex(qBound(0, index, tabs->count() - 1));
  }

  void previewPage(int index) {
    if (offline)
      pages->setCurrentIndex(qBound(0, index, pages->count() - 1));
  }
  void previewGpu(int index) {
    if (offline)
      gpuSelect->setCurrentIndex(qBound(0, index, gpuSelect->count() - 1));
  }
signals:
  void selectGpu(int);
  void configureOverlay(QJsonObject);
  void overlayEndpoint(int, QString);
  void trackProcess(int);
  void refreshInventory();

protected:
  void closeEvent(QCloseEvent *) override;
  bool eventFilter(QObject *, QEvent *) override;
};
