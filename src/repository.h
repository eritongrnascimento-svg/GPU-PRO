#pragma once
#include "model.h"
struct sqlite3;
QString deviceKey(const Gpu &);
QJsonObject publicGpu(const Gpu &);
QJsonObject sanitizeResult(const QJsonObject &);
bool validFeedback(const QJsonObject &, QString &error);
class Repository {
  sqlite3 *db = nullptr;

public:
  explicit Repository(QString path);
  ~Repository();
  QString error;
  bool add(QString kind, QString device, QJsonObject data, QString origin = "local");
  QJsonArray recent(QString kind, QString device, int count = 3) const;
  QJsonObject feedback() const;
  QJsonArray references() const;
  int importFeedback(const QJsonObject &object, QString &error);
  int count(QString origin = "local") const;
};
