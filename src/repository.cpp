#include "repository.h"
#include <cmath>
#include <sqlite3.h>
QString deviceKey(const Gpu &g) { return g.vendor + "/" + g.deviceId + "/" + g.pci; }
QJsonObject publicGpu(const Gpu &g) {
  QJsonObject o{{"name", g.name}, {"vendor", g.vendor}, {"deviceId", g.deviceId}};
  for (auto key : {"driver", "integrated", "executionUnits", "subsliceCount", "sliceCount", "memoryType", "intelProbeStatus", "topologySource", "minGraphicsMHz", "architecture", "memoryBytes", "memoryBusWidthBits", "gpuCores", "driverVersion",
                   "vbios", "subsystemVendor", "subsystemDevice", "maxGraphicsMHz", "maxMemoryMHz"})
    if (g.specs.contains(key))
      o[key] = g.specs[key];
  return o;
}
namespace {
const QSet<QString> fields = {"id",
                              "kind",
                              "valid",
                              "partial",
                              "cancelled",
                              "method",
                              "api",
                              "width",
                              "height",
                              "durationSeconds",
                              "scene",
                              "score",
                              "scoreUnit",
                              "ffmpegVersion",
                              "timestamp",
                              "gpuMeanMs",
                              "gpuP99Ms",
                              "presentedFps",
                              "onePercentLow",
                              "frames",
                              "cpuSubmitMs",
                              "cpuWorkMs",
                              "deviceVerified",
                              "samples",
                              "sceneResults",
                              "codecResults",
                              "codec",
                              "mode",
                              "status",
                              "errorCode",
                              "seconds",
                              "fps",
                              "coverage",
                              "supported",
                              "attempted",
                              "cpuModel",
                              "cpuThreads",
                              "cpuScores",
                              "comparison",
                              "classification",
                              "summary", "lossModel", "cpuLossPct", "gpuLossPct", "gpuPotentialFps", "cpuPotentialFps", "modelFps",
                              "resolutionGain",
                              "highGpuMs",
                              "lowGpuMs",
                              "highCpuMs",
                              "lowCpuMs",
                              "confidence",
                              "testLimits",
                              "coreBefore",
                              "coreTested",
                              "coreRestored",
                              "memoryBefore",
                              "memoryAfter",
                              "coreMode",
                              "memMode",
                              "coreMin",
                              "coreMax",
                              "memMin",
                              "memMax",
                              "powerBefore",
                              "powerAfter",
                              "voltageWritten",
                              "powerLimitWritten",
                              "restored",
                              "telemetry",
                              "temp",
                              "hotspot",
                              "memoryTemp",
                              "power",
                              "clock",
                              "memClock",
                              "fan",
                              "fanRpm",
                              "usage",
                              "encoder",
                              "decoder",
                              "vram",
                              "vramTotal",
                              "coreCurrent",
                              "memCurrent",
                              "phase",
                              "elapsedMs",
                              "metric",
                              "value",
                              "unit",
                              "computeResults",
                              "parameters",
                              "cpuIterations",
                              "shaderVersion",
                              "livePreview",
                              "originalLimitsPreserved"};
QJsonValue sanitize(const QJsonValue &v, int depth = 0) {
  if (depth > 8)
    return {};
  if (v.isArray()) {
    QJsonArray a;
    for (auto item : v.toArray()) {
      if (a.size() >= 4096)
        break;
      a.append(sanitize(item, depth + 1));
    }
    return a;
  }
  if (v.isObject()) {
    QJsonObject o;
    auto object = v.toObject();
    for (auto it = object.begin(); it != object.end(); ++it)
      if (fields.contains(it.key()))
        o[it.key()] = sanitize(it.value(), depth + 1);
    return o;
  }
  if (v.isDouble())
    return std::isfinite(v.toDouble()) ? v : QJsonValue();
  if (v.isString()) {
    QString s = v.toString();
    if (s.startsWith('/') || s.contains("/home/") || s.contains("/tmp/") || s.contains('\\') ||
        s.size() > 160)
      return QJsonValue();
    return s;
  }
  return v;
}
} // namespace
QJsonObject sanitizeResult(const QJsonObject &o) {
  auto result = sanitize(o).toObject();
  if (o["gpu"].isObject()) {
    Gpu g;
    auto original = o["gpu"].toObject();
    g.name = original["name"].toString().left(160);
    g.vendor = original["vendor"].toString();
    g.deviceId = original["deviceId"].toString();
    g.specs = original;
    result["gpu"] = publicGpu(g);
  }
  return result;
}
bool validFeedback(const QJsonObject &o, QString &error) {
  if (o["schema"].toString() != "linux-gpu-feedback/1" || !o["records"].isArray()) {
    error = "Formato de feedback desconhecido";
    return false;
  }
  auto records = o["records"].toArray();
  if (records.size() > 2000) {
    error = "Limite de 2000 registros excedido";
    return false;
  }
  for (auto v : records) {
    auto r = v.toObject();
    if (r.isEmpty() || QUuid(r["id"].toString()).isNull()) {
      error = "Registro sem UUID válido";
      return false;
    }
    if (!QStringList{"graphics", "video", "compute", "bottleneck", "clock-trial"}.contains(
            r["kind"].toString()) ||
        !r["valid"].isBool()) {
      error = "Categoria ou validade inválida";
      return false;
    }
    const auto gpu = r["gpu"].toObject();
    if (gpu["name"].toString().isEmpty() || gpu["name"].toString().size() > 160 ||
        !QStringList{"NVIDIA", "AMD", "Intel", "Outro"}.contains(gpu["vendor"].toString()) ||
        !QRegularExpression("^0x[0-9a-fA-F]{4}$").match(gpu["deviceId"].toString()).hasMatch()) {
      error = "Identificação técnica da GPU inválida";
      return false;
    }
    if (r["synthetic"].toBool()) {
      error = "Resultados fictícios não podem entrar na base";
      return false;
    }
    if (r.contains("width") &&
        (!r["width"].isDouble() || r["width"].toInt() < 320 || r["width"].toInt() > 7680)) {
      error = "Resolução inválida";
      return false;
    }
    if (r.contains("height") &&
        (!r["height"].isDouble() || r["height"].toInt() < 240 || r["height"].toInt() > 4320)) {
      error = "Resolução inválida";
      return false;
    }
    for (auto key : {"cpuLossPct", "gpuLossPct"}) if (r.contains(key) &&
        (!r[key].isDouble() || !std::isfinite(r[key].toDouble()) || r[key].toDouble() < 0 || r[key].toDouble() > 100)) {
      error = "Percentual fora da faixa"; return false;
    }
    if (r.contains("score") &&
        (!r["score"].isDouble() || r["score"].toDouble() < 0 || r["score"].toDouble() > 1e12)) {
      error = "Pontuação fora da faixa";
      return false;
    }
  }
  return true;
}
Repository::Repository(QString path) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  if (sqlite3_open_v2(path.toUtf8().constData(), &db,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                      nullptr) != SQLITE_OK) {
    error = db ? QString::fromUtf8(sqlite3_errmsg(db)) : "SQLite indisponível";
    return;
  }
  sqlite3_busy_timeout(db, 3000);
  char *message = nullptr;
  if (sqlite3_exec(
          db,
          "CREATE TABLE IF NOT EXISTS records(id TEXT PRIMARY KEY,kind TEXT NOT NULL,device TEXT "
          "NOT NULL,created TEXT NOT NULL,origin TEXT NOT NULL,data TEXT NOT NULL);CREATE INDEX IF "
          "NOT EXISTS history ON records(origin,device,kind,created DESC);",
          nullptr, nullptr, &message) != SQLITE_OK) {
    error = QString::fromUtf8(message);
    sqlite3_free(message);
  }
}
Repository::~Repository() {
  if (db)
    sqlite3_close(db);
}
namespace {
struct Query {
  sqlite3_stmt *stmt = nullptr;
  Query(sqlite3 *db, const char *sql) {
    if (db)
      sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  }
  ~Query() {
    if (stmt)
      sqlite3_finalize(stmt);
  }
  void text(int index, const QString &value) {
    if (stmt)
      sqlite3_bind_text(stmt, index, value.toUtf8().constData(), -1, SQLITE_TRANSIENT);
  }
  void integer(int index, int value) {
    if (stmt)
      sqlite3_bind_int(stmt, index, value);
  }
  QString value(int col = 0) const {
    auto *text = sqlite3_column_text(stmt, col);
    return text ? QString::fromUtf8(reinterpret_cast<const char *>(text)) : QString();
  }
  bool row() { return stmt && sqlite3_step(stmt) == SQLITE_ROW; }
};
} // namespace
bool Repository::add(QString kind, QString device, QJsonObject data, QString origin) {
  if (!db)
    return false;
  if (!data.contains("id"))
    data["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
  data["kind"] = kind;
  if (!data.contains("timestamp"))
    data["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  Query q(db, "INSERT OR IGNORE INTO records VALUES(?,?,?,?,?,?)");
  if (!q.stmt)
    return false;
  q.text(1, data["id"].toString());
  q.text(2, kind);
  q.text(3, device);
  q.text(4, data["timestamp"].toString());
  q.text(5, origin);
  q.text(6, QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
  if (sqlite3_step(q.stmt) != SQLITE_DONE) {
    error = QString::fromUtf8(sqlite3_errmsg(db));
    return false;
  }
  return true;
}
QJsonArray Repository::recent(QString kind, QString device, int count) const {
  QJsonArray a;
  Query q(db, "SELECT data FROM records WHERE origin='local' AND kind=? AND device=? ORDER BY "
              "created DESC,rowid DESC LIMIT ?");
  q.text(1, kind);
  q.text(2, device);
  q.integer(3, qBound(1, count, 2000));
  while (q.row())
    a.append(QJsonDocument::fromJson(q.value().toUtf8()).object());
  return a;
}
int Repository::count(QString origin) const {
  Query q(db, "SELECT COUNT(*) FROM records WHERE origin=?");
  q.text(1, origin);
  return q.row() ? sqlite3_column_int(q.stmt, 0) : 0;
}
QJsonObject Repository::feedback() const {
  QJsonArray a;
  Query q(db, "SELECT data FROM records WHERE origin='local' ORDER BY created DESC LIMIT 2000");
  while (q.row())
    a.append(sanitizeResult(QJsonDocument::fromJson(q.value().toUtf8()).object()));
  return {{"schema", "linux-gpu-feedback/1"},
          {"appVersion", QCoreApplication::applicationVersion()},
          {"exportedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
          {"qtVersion", qVersion()},
          {"records", a},
          {"privacy", QJsonObject{{"automaticUpload", false},
                                  {"includesHostnames", false},
                                  {"includesSerials", false},
                                  {"includesProcessNames", false},
                                  {"includesRawLogs", false}}}};
}
int Repository::importFeedback(const QJsonObject &o, QString &e) {
  if (!validFeedback(o, e))
    return -1;
  if (!db || sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr) != SQLITE_OK) {
    e = "Não foi possível iniciar transação SQLite";
    return -1;
  }
  int before = count("imported");
  for (auto v : o["records"].toArray()) {
    auto r = sanitizeResult(v.toObject());
    auto g = r["gpu"].toObject();
    if (!add(r["kind"].toString(), g["vendor"].toString() + "/" + g["deviceId"].toString(), r,
             "imported")) {
      e = error;
      sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
      return -1;
    }
  }
  if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK) {
    e = QString::fromUtf8(sqlite3_errmsg(db));
    sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    return -1;
  }
  return count("imported") - before;
}

QJsonArray Repository::references() const {
  QJsonArray a;
  Query q(db, "SELECT data FROM records WHERE origin='imported' ORDER BY created DESC LIMIT 2000");
  while (q.row())
    a.append(QJsonDocument::fromJson(q.value().toUtf8()).object());
  return a;
}
