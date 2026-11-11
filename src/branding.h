#pragma once
#include <QtGui>
namespace Branding {
QImage transparentArtwork();
QIcon windowIcon(QColor accent);
QIcon trayIcon(QColor accent, qreal pulse = 0.6);
bool writePreview(const QString &path);
} // namespace Branding
