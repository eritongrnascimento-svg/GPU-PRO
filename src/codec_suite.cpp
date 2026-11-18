#include "codec_suite.h"
#include <QBuffer>
#include <QThread>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <cmath>

namespace {
const QList<QSize> kResolutions{{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
const QStringList kCodecs{"h264", "hevc", "av1"};
QString Label(const QSize &s) { return QString("%1×%2").arg(s.width()).arg(s.height()); }
}  // namespace

CodecSuite::CodecSuite(QObject *p) : QObject(p), runner(this) {
  connect(&runner, &CodecRunner::progress, this, [this](int value, QString text) {
    const int total = jobs.size();
    emit progress(videoShare() * (index * 100 + value) / qMax(1, total) / 100,
                  QString("Vídeo %1 / %2 · ").arg(index + 1).arg(total) + text);
  });
  connect(&runner, &CodecRunner::result, this, [this](QJsonObject r) {
    if (!active) return;
    const Job job = jobs[index];
    r["codec"] = job.codec;
    r["mode"] = job.mode;
    r["width"] = job.size.width();
    r["height"] = job.size.height();
    r["resolution"] = QString("%1x%2").arg(job.size.width()).arg(job.size.height());
    QString error = r.value("error").toString();
    QString status = r["valid"].toBool() ? "passed" : cancelling ? "cancelled" : "failed";
    if (!r["valid"].toBool() && (error.contains("Unknown encoder", Qt::CaseInsensitive) || error.contains("not supported", Qt::CaseInsensitive) ||
                                 error.contains("No usable encoding", Qt::CaseInsensitive) || error.contains("unsupported", Qt::CaseInsensitive) ||
                                 error.contains("No capable devices", Qt::CaseInsensitive) || error.contains("Function not implemented", Qt::CaseInsensitive) ||
                                 error.contains("FFmpeg não instalado") || error.contains("sem render node"))) {
      status = "unsupported";
      r["errorCode"] = "UNSUPPORTED_CODEC_OR_FFMPEG_BUILD";
      unsupported.insert(job.codec + "-" + job.mode);  // não insiste nas outras resoluções
    } else if (!r["valid"].toBool())
      r["errorCode"] = cancelling ? "USER_CANCELLED" : "CODEC_FAILED";
    r["status"] = status;
    results.append(r);
    ++index;
    QTimer::singleShot(0, this, [this] { next(); });
  });
}

int CodecSuite::videoShare() const { return doImages ? (doVideo ? 80 : 0) : 100; }

void CodecSuite::start(const Gpu &g, bool video, bool images) {
  if (active) return;
  gpu = g;
  doVideo = video;
  doImages = images;
  active = true;
  cancelling = false;
  index = 0;
  results = {};
  imageResults = {};
  unsupported.clear();
  jobs.clear();
  if (doVideo)
    for (const auto &size : kResolutions)
      for (const auto &codec : kCodecs)
        for (const QString mode : {"encode", "decode"}) jobs.append({codec, mode, size});
  next();
}

void CodecSuite::next() {
  if (!active) return;
  if (cancelling) return finish();
  while (index < jobs.size() && unsupported.contains(jobs[index].codec + "-" + jobs[index].mode)) {
    const Job &job = jobs[index];
    results.append(QJsonObject{{"codec", job.codec}, {"mode", job.mode}, {"width", job.size.width()}, {"height", job.size.height()},
                               {"resolution", QString("%1x%2").arg(job.size.width()).arg(job.size.height())}, {"status", "unsupported"},
                               {"valid", false}, {"errorCode", "UNSUPPORTED_CODEC_OR_FFMPEG_BUILD"}});
    ++index;
  }
  if (index < jobs.size()) {
    const Job &job = jobs[index];
    runner.start(gpu, job.codec, job.mode, job.size.width(), job.size.height());
    return;
  }
  if (doImages) {
    // Fora da thread principal: no app Linux a suíte roda dentro da interface.
    auto *thread = QThread::create([this] { runImages(); });
    connect(thread, &QThread::finished, this, [this, thread] {
      thread->deleteLater();
      finish();
    });
    thread->start();
    return;
  }
  finish();
}

void CodecSuite::cancel() {
  if (!active) return;
  cancelling = true;
  if (runner.busy()) runner.cancel();
}

// Imagens: codificação e decodificação JPEG (qualidade 90), PNG e WebP (quando o Qt oferece) em quatro
// resoluções, pelo processador, com uma imagem de teste fixa (gradientes e ruído determinístico).
void CodecSuite::runImages() {
  QStringList formats{"jpeg", "png"};
  if (QImageWriter::supportedImageFormats().contains("webp") && QImageReader::supportedImageFormats().contains("webp")) formats << "webp";
  const int total = kResolutions.size() * formats.size();
  int step = 0;
  for (const auto &size : kResolutions) {
    QImage image(size, QImage::Format_RGB32);
    quint32 seed = 12345;
    for (int y = 0; y < size.height(); ++y) {
      auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
      for (int x = 0; x < size.width(); ++x) {
        seed = seed * 1664525u + 1013904223u;
        const int noise = int(seed >> 28);
        line[x] = qRgb((x * 255 / size.width() + noise) & 255, (y * 255 / size.height() + noise) & 255, ((x ^ y) & 255));
      }
    }
    for (const auto &format : formats) {
      if (cancelling) return;
      emit progress(videoShare() + (100 - videoShare()) * step / total, QString("Imagem %1 %2").arg(format.toUpper(), Label(size)));
      ++step;
      QByteArray encoded;
      auto encodeOnce = [&] {
        encoded.clear();
        QBuffer buffer(&encoded);
        buffer.open(QIODevice::WriteOnly);
        QImageWriter writer(&buffer, format.toLatin1());
        if (format == "jpeg") writer.setQuality(90);
        return writer.write(image);
      };
      if (!encodeOnce()) {
        imageResults.append(QJsonObject{{"format", format}, {"width", size.width()}, {"height", size.height()}, {"status", "unsupported"}});
        continue;
      }
      for (const QString mode : {"encode", "decode"}) {
        QElapsedTimer timer;
        timer.start();
        int count = 0;
        bool ok = true;
        while ((timer.elapsed() < 1200 || count < 2) && count < 400 && ok) {
          if (mode == "encode") ok = encodeOnce();
          else {
            QBuffer buffer(&encoded);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer, format.toLatin1());
            ok = !reader.read().isNull();
          }
          ++count;
        }
        const double seconds = timer.nsecsElapsed() / 1e9;
        imageResults.append(QJsonObject{{"format", format},
                                        {"mode", mode},
                                        {"width", size.width()},
                                        {"height", size.height()},
                                        {"resolution", QString("%1x%2").arg(size.width()).arg(size.height())},
                                        {"status", ok ? "passed" : "failed"},
                                        {"images", count},
                                        {"fps", count / seconds},
                                        {"mpixPerSecond", count / seconds * size.width() * size.height() / 1e6},
                                        {"bytes", int(encoded.size())}});
      }
    }
  }
}

void CodecSuite::finish() {
  if (!active) return;
  active = false;
  auto geomean = [](const QJsonArray &rows, int &passed, int &failed) {
    double log = 0;
    passed = failed = 0;
    for (const auto &v : rows) {
      const auto r = v.toObject();
      const double rate = r["mpixPerSecond"].toDouble(r["fps"].toDouble() * r["width"].toInt(1920) * r["height"].toInt(1080) / 1e6);
      if (r["status"].toString() == "passed" && rate > 0) {
        log += std::log(rate);
        ++passed;
      } else if (r["status"].toString() == "failed")
        ++failed;
    }
    return passed ? std::exp(log / passed) : 0.0;
  };
  int videoPassed, videoFailed, imagePassed, imageFailed;
  const double video = geomean(results, videoPassed, videoFailed), images = geomean(imageResults, imagePassed, imageFailed);
  const qint64 videoPoints = qRound64(video * 10), imagePoints = qRound64(images * 10);
  QStringList mask;
  for (auto v : results) {
    auto r = v.toObject();
    if (r["status"].toString() == "passed") mask << r["codec"].toString() + "-" + r["mode"].toString() + "-" + r["resolution"].toString();
  }
  const bool any = videoPassed + imagePassed > 0;
  QJsonObject r{{"kind", "video"},
                {"method", "codec-suite-v0.3"},
                {"methodText", "Vídeo: H.264, HEVC e AV1 por hardware (FFmpeg), codificação e decodificação separadas em 1280×720, 1920×1080, "
                               "2560×1440 e 3840×2160, 360 quadros cada. Imagem: JPEG, PNG e WebP (se disponível) pelo processador nas mesmas "
                               "resoluções. Pontuação = média dos megapixels processados por segundo × 10 (vídeo e imagem somados)."},
                {"valid", any && videoFailed == 0 && imageFailed == 0 && !cancelling},
                {"partial", (doVideo && videoPassed < jobs.size()) || !doImages || !doVideo},
                {"cancelled", cancelling.load()},
                {"codecResults", results},
                {"imageResults", imageResults},
                {"coverage", mask.join(",")},
                {"supported", videoPassed},
                {"attempted", int(jobs.size())},
                {"videoPoints", videoPoints},
                {"imagePoints", imagePoints},
                {"points", videoPoints + imagePoints},
                {"scoreUnit", "MPix-s-geomean"},
                {"width", 1920},
                {"height", 1080},
                {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
  if (videoPassed) r["score"] = video;
  if (videoFailed || imageFailed) r["error"] = "Há falhas de execução; confira os itens marcados como falha";
  if (!any) r["error"] = "Nenhum teste de vídeo ou imagem foi concluído";
  if (cancelling) r["error"] = "Suíte cancelada";
  emit result(r);
}
