#include "codec_suite.h"
#include <cmath>
CodecSuite::CodecSuite(QObject *p) : QObject(p), runner(this) {
  connect(&runner, &CodecRunner::progress, this, [this](int value, QString text) {
    emit progress((index * 100 + value) / 6, QString("%1 / 6 · ").arg(index + 1) + text);
  });
  connect(&runner, &CodecRunner::result, this, [this](QJsonObject r) {
    if (!active)
      return;
    QString codec = QStringList{"h264", "hevc", "av1"}[index / 2],
            mode = index % 2 ? "decode" : "encode";
    r["codec"] = codec;
    r["mode"] = mode;
    QString error = r.value("error").toString();
    QString status = r["valid"].toBool() ? "passed" : cancelling ? "cancelled" : "failed";
    if (!r["valid"].toBool() && (error.contains("Unknown encoder", Qt::CaseInsensitive) ||
                                 error.contains("not supported", Qt::CaseInsensitive) ||
                                 error.contains("No usable encoding", Qt::CaseInsensitive) ||
                                 error.contains("unsupported", Qt::CaseInsensitive))) {
      status = "unsupported";
      r["errorCode"] = "UNSUPPORTED_CODEC_OR_FFMPEG_BUILD";
    } else if (!r["valid"].toBool())
      r["errorCode"] = cancelling ? "USER_CANCELLED" : "CODEC_FAILED";
    r["status"] = status;
    results.append(r);
    ++index;
    if (cancelling || index >= 6)
      finish();
    else
      QTimer::singleShot(0, this, [this] {
        if (active && !cancelling)
          next();
      });
  });
}
void CodecSuite::start(const Gpu &g) {
  if (active)
    return;
  gpu = g;
  active = true;
  cancelling = false;
  index = 0;
  results = {};
  next();
}
void CodecSuite::next() {
  if (!active)
    return;
  runner.start(gpu, QStringList{"h264", "hevc", "av1"}[index / 2], index % 2 ? "decode" : "encode");
}
void CodecSuite::cancel() {
  if (!active)
    return;
  cancelling = true;
  if (runner.busy())
    runner.cancel();
  else
    finish();
}
void CodecSuite::finish() {
  if (!active)
    return;
  active = false;
  double product = 1;
  int passed = 0, failed = 0;
  QStringList mask;
  for (auto v : results) {
    auto r = v.toObject();
    if (r["status"].toString() == "passed" && r["fps"].toDouble() > 0) {
      product *= r["fps"].toDouble() * 1920 * 1080 / 1e6;
      ++passed;
      mask << r["codec"].toString() + "-" + r["mode"].toString();
    } else if (r["status"].toString() == "failed")
      ++failed;
  }
  QJsonObject r{{"kind", "video"},
                {"method", "codec-suite-v0.2"},
                {"valid", passed > 0 && failed == 0 && !cancelling},
                {"partial", passed < 6},
                {"cancelled", cancelling},
                {"codecResults", results},
                {"coverage", mask.join(",")},
                {"supported", passed},
                {"attempted", index},
                {"scoreUnit", "MPix-s-geomean"},
                {"width", 1920},
                {"height", 1080},
                {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
  if (passed > 0 && failed == 0 && !cancelling)
    r["score"] = std::pow(product, 1. / passed);
  if (failed)
    r["error"] = "Há falhas de execução; pontuação consolidada descartada";
  if (!passed)
    r["error"] = "Nenhum caminho de codec por hardware foi concluído";
  if (cancelling)
    r["error"] = "Suíte cancelada";
  emit result(r);
}
