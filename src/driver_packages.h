#pragma once
#include <QtCore>
#include <QtDBus>
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
  bool dbusBusy = false;
  QString transactionPath, backendError, backendName;
  QJsonArray backendRows;
  void queryPackageKit();
private slots:
  void packageReceived(uint info, QString packageId, QString summary);
  void packageQueryFinished(uint exitCode, uint runtime);
  void packageQueryError(uint code, QString details);
private:
  void run(QStringList args);
  void completed(int code, QProcess::ExitStatus status);
public:
  explicit DriverCatalog(QObject *parent = nullptr);
  bool busy() const { return dbusBusy || process.state() != QProcess::NotRunning; }
  void refresh(QString vendor);
  ~DriverCatalog();
signals:
  void listed(QJsonArray candidates, QString message);
};
