#pragma once
#include <QtCore>
class FrameLog {
  QFile file;
  QString path;
  QByteArray pending;
  int column=-1;
  quint64 device=0,inode=0;
public:
  QJsonArray poll(const QString &path);
};
