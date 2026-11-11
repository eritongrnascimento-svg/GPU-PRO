#pragma once
#include <QtCore>
#include <QtNetwork>
#include <utility>
class SingleInstance : public QObject {
  Q_OBJECT
public:
  enum Result { Primary, ActivatedExisting, Failed };
  explicit SingleInstance(QString directory, QString session, QObject *parent = nullptr);
  Result acquire();
  QString error() const { return errorText; }
  bool takePendingActivation() { return std::exchange(pending, false); }
  static QString runtimeDirectory();
  static QString graphicalSession();
signals:
  void activationRequested();

private:
  QString directory, endpoint, errorText;
  std::unique_ptr<QLockFile> lock;
  QLocalServer server;
  bool pending = false;
};
