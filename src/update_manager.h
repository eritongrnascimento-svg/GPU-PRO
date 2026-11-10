#pragma once
#include <QtNetwork>
struct ReleaseInfo { QString version, tag, assetName; QUrl assetUrl, checksumUrl; };
class UpdateManager : public QObject {
  Q_OBJECT
  QString root, storage;
  QNetworkAccessManager network;
  bool working = false;
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
  void check();
  void downloadAndInstall();
signals:
  void message(QString text);
  void checked(QString version, bool available);
  void finished(bool installed);
};
