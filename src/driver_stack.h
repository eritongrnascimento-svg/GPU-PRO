#pragma once
#include <QtCore>
QJsonObject nativeDriverCatalog(QString vendor);
QJsonObject parsePacmanPreview(QString text, const QJsonArray &installed);
QJsonObject nativeDriverPlan(const QJsonObject &candidate);
QJsonArray completeDriverProfiles(const QJsonArray &packages, const QJsonArray &installed,
                                  QString vendor);
QJsonObject saveDriverSnapshot(const QJsonObject &plan, QString storage, QString *error);
QJsonArray savedDriverSnapshots(QString storage);
QString driverPlanDescription(const QJsonObject &plan);
