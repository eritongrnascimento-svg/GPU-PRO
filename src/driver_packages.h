#pragma once
#include <QtCore>
QString nativePackageManager();
bool driverPackageAllowed(QString package, QString vendor = {});
QJsonArray parseDriverPackages(QString manager, QString text, QString vendor = {});
QStringList driverTransaction(const QJsonObject &candidate, bool simulate);
QProcessEnvironment nativeEnvironment();
class DriverCatalog : public QObject {
  Q_OBJECT
  QProcess process;
  QTimer timeout;
  QString manager, vendor, installed, names;
  int phase = 0;
  QByteArray output;
  void run(QStringList args);
  void completed(int code, QProcess::ExitStatus status);
public:
  explicit DriverCatalog(QObject *parent = nullptr);
  bool busy() const { return process.state() != QProcess::NotRunning; }
  void refresh(QString vendor);
  ~DriverCatalog();
signals:
  void listed(QJsonArray candidates, QString message);
};
