#include "branding.h"
#include <cmath>

QImage Branding::transparentArtwork() {
  // Keep the supplied art intact. Only exterior-connected black is made transparent
  // at render time; the black details enclosed by the drawing remain unchanged.
  static const QImage artwork = [] {
    QImage image(":/assets/gpu-pro.png");
    image = image.convertToFormat(QImage::Format_ARGB32);
    if (image.isNull())
      return image;
    const int w = image.width(), h = image.height();
    QVector<bool> exterior(w * h, false);
    QVector<int> queue;
    queue.reserve(w * h);
    auto visit = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= w || y >= h)
        return;
      int i = y * w + x;
      if (exterior[i])
        return;
      QRgb p = image.pixel(x, y);
      if (qAlpha(p) > 0 && qMax(qRed(p), qMax(qGreen(p), qBlue(p))) > 32)
        return;
      exterior[i] = true;
      queue.append(i);
    };
    for (int x = 0; x < w; ++x) {
      visit(x, 0);
      visit(x, h - 1);
    }
    for (int y = 0; y < h; ++y) {
      visit(0, y);
      visit(w - 1, y);
    }
    for (qsizetype i = 0; i < queue.size(); ++i) {
      int x = queue[i] % w, y = queue[i] / w;
      visit(x - 1, y);
      visit(x + 1, y);
      visit(x, y - 1);
      visit(x, y + 1);
    }
    QRect bounds;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        if (exterior[y * w + x])
          image.setPixel(x, y, qRgba(0, 0, 0, 0));
        else if (qAlpha(image.pixel(x, y)))
          bounds = bounds.united(QRect(x, y, 1, 1));
      }
    return bounds.isEmpty() ? image : image.copy(bounds);
  }();
  return artwork;
}
namespace {
QImage tinted(const QImage &image, QColor color) {
  QImage result(image.size(), QImage::Format_ARGB32_Premultiplied);
  result.fill(Qt::transparent);
  QPainter p(&result);
  p.drawImage(0, 0, image);
  p.setCompositionMode(QPainter::CompositionMode_SourceIn);
  p.fillRect(result.rect(), color);
  return result;
}
QImage compose(int size, QColor color, qreal pulse, bool compact) {
  QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QImage symbol(size, size, QImage::Format_ARGB32_Premultiplied);
  symbol.fill(Qt::transparent);
  {
    QPainter p(&symbol);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (compact) {
      p.scale(size / 64., size / 64.);
      QPainterPath outline;
      outline.moveTo(13, 7);
      outline.lineTo(51, 7);
      outline.lineTo(57, 13);
      outline.lineTo(57, 51);
      outline.lineTo(51, 57);
      outline.lineTo(13, 57);
      outline.lineTo(7, 51);
      outline.lineTo(7, 13);
      outline.closeSubpath();
      p.setPen(QPen(color, 1.6));
      p.setBrush(QColor(15, 20, 29, 235));
      p.drawPath(outline);
      QPainterPath letters;
      letters.setFillRule(Qt::OddEvenFill);
      letters.addPolygon(QPolygonF{{29, 20},
                                   {14, 20},
                                   {10, 24},
                                   {10, 40},
                                   {14, 44},
                                   {29, 44},
                                   {29, 30},
                                   {20, 30},
                                   {20, 35},
                                   {23, 35},
                                   {23, 38},
                                   {16, 38},
                                   {16, 26},
                                   {29, 26}});
      letters.addPolygon(QPolygonF{
          {34, 20}, {49, 20}, {54, 25}, {54, 33}, {49, 38}, {41, 38}, {41, 44}, {34, 44}});
      letters.addPolygon(QPolygonF{{41, 26}, {47, 26}, {48, 27}, {48, 31}, {47, 32}, {41, 32}});
      p.setPen(Qt::NoPen);
      p.setBrush(color);
      p.drawPath(letters);
    } else {
      auto art = Branding::transparentArtwork();
      QSize fit = art.size();
      fit.scale(qRound(size * .80), qRound(size * .80), Qt::KeepAspectRatio);
      p.drawImage(QRect(QPoint((size - fit.width()) / 2, (size - fit.height()) / 2), fit), art);
    }
  }
  auto glow = tinted(symbol, color);
  // Three separable box passes approximate a soft Gaussian halo without a
  // dependency on a compositor effect or an animated widget repaint.
  QVector<qreal> alpha(size * size), scratch(size * size);
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x)
      alpha[y * size + x] = qAlpha(glow.pixel(x, y));
  int radius = qMax(1, qRound(size * (compact ? .018 : .025)));
  for (int pass = 0; pass < 3; ++pass) {
    for (int y = 0; y < size; ++y)
      for (int x = 0; x < size; ++x) {
        qreal sum = 0;
        for (int dx = -radius; dx <= radius; ++dx)
          if (x + dx >= 0 && x + dx < size)
            sum += alpha[y * size + x + dx];
        scratch[y * size + x] = sum / (2 * radius + 1);
      }
    for (int y = 0; y < size; ++y)
      for (int x = 0; x < size; ++x) {
        qreal sum = 0;
        for (int dy = -radius; dy <= radius; ++dy)
          if (y + dy >= 0 && y + dy < size)
            sum += scratch[(y + dy) * size + x];
        alpha[y * size + x] = sum / (2 * radius + 1);
      }
  }
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      QColor tint = color;
      tint.setAlpha(qBound(0, qRound(alpha[y * size + x] * (.8 + pulse)), 255));
      glow.setPixelColor(x, y, tint);
    }
  QPainter p(&image);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.drawImage(0, 0, glow);
  p.setOpacity(1);
  p.drawImage(0, 0, symbol);
  return image;
}
QIcon build(QColor c, qreal pulse, bool compact) {
  QIcon icon;
  for (int size : {16, 22, 24, 32, 48, 64, 128})
    icon.addPixmap(QPixmap::fromImage(compose(size, c, pulse, compact)));
  return icon;
}
} // namespace
QIcon Branding::windowIcon(QColor) {
  QIcon icon;const auto art=transparentArtwork();
  for(int size:{16,22,24,32,48,64,128,256}) {
    QImage canvas(size,size,QImage::Format_ARGB32_Premultiplied);canvas.fill(Qt::transparent);
    QPainter p(&canvas);p.setRenderHint(QPainter::SmoothPixmapTransform);
    QSize fit=art.size();fit.scale(qRound(size*.90),qRound(size*.90),Qt::KeepAspectRatio);
    p.drawImage(QRect(QPoint((size-fit.width())/2,(size-fit.height())/2),fit),art);p.end();
    icon.addPixmap(QPixmap::fromImage(canvas));
  }
  return icon;
}
QIcon Branding::trayIcon(QColor accent, qreal pulse) {
  return build(accent, qBound(0., pulse, 1.), true);
}
bool Branding::writePreview(const QString &path) {
  QImage image(1050, 390, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor("#161a22"));
  QPainter p(&image);
  p.setPen(Qt::white);
  QFont font = p.font();
  font.setPointSize(13);
  p.setFont(font);
  int i = 0;
  for (auto c : {QColor("#76b900"), QColor("#ed1c24"), QColor("#168ce8")}) {
    int x = 25 + i * 350;
    p.drawText(x, 30, QStringList{"NVIDIA", "AMD", "Intel"}[i]);
    p.drawPixmap(x, 48, Branding::windowIcon(c).pixmap(180));
    p.drawImage(x + 185, 70, compose(100, c, .8, true));
    p.drawText(x, 265, "Janela / barra de tarefas");
    p.drawText(x, 293, "Bandeja: GP / halo animado");
    for (int size : {16, 24, 32}) {
      p.drawImage(x, 320, compose(size, c, .7, true));
      x += 65;
    }
    ++i;
  }
  p.end();
  return image.save(path);
}
