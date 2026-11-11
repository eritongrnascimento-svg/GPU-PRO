#pragma once
#include "model.h"
QJsonObject parseAudioInventory(QJsonArray sinks, QJsonArray sources, QString info,
                                QString alsaCards);
QJsonObject audioInventory();
QJsonObject alignAudio(QString output, QString input, QString storage);
QJsonObject restoreAudio(QString storage);
QJsonObject gpuSessionPlan(const QList<Gpu> &, QString pci, QString configHome = {});
QJsonObject applyGpuSession(const QJsonObject &, QString storage);
QJsonObject restoreGpuSession(QString storage);
