#pragma once
#include "gpu.h"
class CodecRunner : public QObject {
  Q_OBJECT
  QProcess process;
  QTimer timeout;
  std::unique_ptr<QTemporaryDir> temp;
  QElapsedTimer elapsed;
  Gpu gpu;
  QString codec, mode, stage, errorLog, stdoutBuffer, ffmpegVersion;
  int frames = 0;
  bool active = false, cancelled = false;
  QStringList runArgs;
  void run(QStringList args, QString phase);
  void measured();
  void finished(int, QProcess::ExitStatus);
  void fail(QString);

public:
  explicit CodecRunner(QObject *p = nullptr);
  ~CodecRunner();
  bool busy() const { return active; }
  void start(const Gpu &, QString codec, QString mode);
  void cancel();
signals:
  void progress(int, QString);
  void result(QJsonObject);
};
