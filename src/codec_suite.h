#pragma once
#include "codec.h"
#include <atomic>
// Suíte de codecs: vídeo por hardware (H.264/HEVC/AV1, codificação e decodificação em quatro
// resoluções) e imagem pelo processador (JPEG/PNG/WebP), com pontuação simplificada.
class CodecSuite : public QObject {
  Q_OBJECT
  struct Job {
    QString codec, mode;
    QSize size;
  };
  CodecRunner runner;
  Gpu gpu;
  QList<Job> jobs;
  QSet<QString> unsupported;
  QJsonArray results, imageResults;
  int index = 0;
  bool active = false, doVideo = true, doImages = true;
  std::atomic_bool cancelling{false};  // lido também pela thread das imagens
  void next();
  void runImages();
  void finish();
  int videoShare() const;

public:
  explicit CodecSuite(QObject *p = nullptr);
  bool busy() const { return active; }
  void start(const Gpu &, bool video = true, bool images = true);
  void cancel();
signals:
  void progress(int, QString);
  void result(QJsonObject);
};
