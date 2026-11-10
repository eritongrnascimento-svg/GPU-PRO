#pragma once
#include "codec.h"
class CodecSuite : public QObject {
  Q_OBJECT
  CodecRunner runner;
  Gpu gpu;
  QJsonArray results;
  int index = 0;
  bool active = false, cancelling = false;
  void next();
  void finish();

public:
  explicit CodecSuite(QObject *p = nullptr);
  bool busy() const { return active; }
  void start(const Gpu &);
  void cancel();
signals:
  void progress(int, QString);
  void result(QJsonObject);
};
