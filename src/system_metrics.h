#pragma once
#include <QJsonObject>
// Acrescenta RAM, disco e clock médio da CPU à leitura enviada ao painel e ao HUD.
void addSystemMetrics(QJsonObject &current);
