#pragma once
#include <QtNetwork>
struct ReleaseInfo { QString version, tag, assetName; QUrl assetUrl, checksumUrl; };
class UpdateManager : public QObject {
  Q_OBJECT
  QString root, storage;
  QNetworkAccessManager network;
  bool working = false;
  QString kind="portable", managedTarget;
  void backupDeb(std::function<void(QString)> done);
  QString imageBackup(QString &error);
  ReleaseInfo release;
  void fetch(QUrl url, qint64 maximum, std::function<void(QByteArray, QString)> done);
public:
  UpdateManager(QString installRoot, QString dataRoot, QObject *parent = nullptr);
  static bool newer(QString candidate, QString current);
  static bool officialUrl(const QUrl &);
  QString backup(QString &error);
  bool restore(QString backupPath, QString &error);
  bool installArchive(QString archive, QByteArray sha256, QString version, QString &error);
  std::function<bool()> mayInstall = [] { return true; };
  bool busy() const { return working; }
  void setFormat(QString format,QString target={}) {kind=format;managedTarget=target;}
  QString format() const {return kind;}
  bool installImage(QString file,QByteArray sha256,QString version,QString &error);
  bool stageDeb(QString file,QByteArray sha256,QString version,QString &path,QString &error);
  void backupCurrent();
  void restoreManaged();
  void check();
  void downloadAndInstall();
signals:
  void message(QString text);
  void checked(QString version, bool available);
  void finished(bool installed);
  void managedPackageReady(QString path,QString version,bool restoring);
};
