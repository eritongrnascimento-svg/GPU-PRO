#include "frame_feed.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstddef>

namespace {
const QSet<QString> &ignored() {
  // Apresentam quadros, mas não são jogos: compositores, gamescope (a janela dele é a composição; o
  // jogo dentro dele aparece separado), sobreposições, navegadores e o próprio GPU-PRO.
  static const QSet<QString> names{"",          "gamescope",       "gamescope-wl",   "kwin_wayland", "kwin_x11",    "gnome-shell",
                                   "Xwayland",  "steamwebhelper",  "steam",          "firefox",      "chrome",      "chromium",
                                   "brave",     "obs",             "linux-gpu-control", "gpu-bench-worker", "mangoapp", "plasmashell",
                                   "Discord",   "electron",        "vkcube"};
  return names;
}
}  // namespace

FrameFeed::FrameFeed() {
  fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
  if (fd < 0) return;
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  static const char name[] = "gpu-pro-frames";
  memcpy(address.sun_path + 1, name, sizeof name - 1);
  if (bind(fd, reinterpret_cast<sockaddr *>(&address), static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + sizeof name - 1)) != 0) {
    close(fd);
    fd = -1;
  }
}

FrameFeed::~FrameFeed() {
  if (fd >= 0) close(fd);
}

void FrameFeed::parse(const QByteArray &datagram, qint64 now) {
  // GPUPRO1 \t pid \t nome \t API \t ms,ms,...
  const auto parts = datagram.split('\t');
  if (parts.size() < 5 || parts[0] != "GPUPRO1") return;
  bool ok = false;
  const int pid = parts[1].toInt(&ok);
  if (!ok || pid <= 0 || pid == QCoreApplication::applicationPid()) return;
  auto &s = sources[pid];
  s.name = QString::fromUtf8(parts[2]).left(128);
  s.api = QString::fromUtf8(parts[3]).left(16);
  for (const auto &value : parts[4].split(',')) {
    const double ms = value.toDouble(&ok);
    if (!ok || !(ms > 0) || ms > 5000) continue;
    s.times.append(ms);
    if (s.times.size() > 240) s.times.removeFirst();
    ++s.recent;
  }
  s.last = now;
  if (!s.recentStart) s.recentStart = now;
  if (now - s.recentStart >= 1000) {
    s.rate = s.recent * 1000.0 / (now - s.recentStart);
    s.recent = 0;
    s.recentStart = now;
  }
}

void FrameFeed::poll() {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (fd >= 0) {
    char buffer[65536];
    for (int i = 0; i < 512; ++i) {
      const ssize_t n = recv(fd, buffer, sizeof buffer, 0);
      if (n <= 0) break;
      parse(QByteArray(buffer, static_cast<int>(n)), now);
    }
  }
  for (auto it = sources.begin(); it != sources.end();)
    it = now - it->last > 10000 ? sources.erase(it) : std::next(it);
}

QList<QPair<int, FrameFeed::Source>> FrameFeed::active() const {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  QList<QPair<int, Source>> list;
  for (auto it = sources.begin(); it != sources.end(); ++it) {
    if (now - it->last < 2000 && !ignored().contains(it->name)) list.append({it.key(), *it});
  }
  std::sort(list.begin(), list.end(), [](const auto &a, const auto &b) { return a.second.rate > b.second.rate; });
  return list;
}

QJsonArray FrameFeed::times(int pid) const {
  QJsonArray out;
  auto it = sources.find(pid);
  if (it == sources.end() || QDateTime::currentMSecsSinceEpoch() - it->last > 3000) return out;
  for (double v : it->times) out.append(v);
  return out;
}

int FrameFeed::pick() {
  const auto list = active();
  if (list.isEmpty()) return sticky = 0;
  for (const auto &entry : list)
    if (entry.first == sticky) return sticky;
  return sticky = list.first().first;
}

QString FrameFeed::describe(int pid) const {
  auto it = sources.find(pid);
  return it == sources.end() ? QString() : it->name + " · " + it->api;
}

namespace FpsLayer {
namespace {
const auto preference = QStringLiteral("overlay/automaticFps");
}

QString systemManifest() {
  for (const QString &path : {QStringLiteral("/usr/share/vulkan/implicit_layer.d/gpu-pro-fps.json"),
                              QStringLiteral("/etc/vulkan/implicit_layer.d/gpu-pro-fps.json")})
    if (QFileInfo::exists(path)) return path;
  return {};
}

QString userManifest() {
  return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/vulkan/implicit_layer.d/gpu-pro-fps.json";
}

bool active() { return !systemManifest().isEmpty() || QFileInfo::exists(userManifest()); }

QString libraryNextToApp() {
  const QString dir = QCoreApplication::applicationDirPath();
  for (const QString &path : {dir + "/libgpupro_fps.so", dir + "/../lib/gpu-pro/libgpupro_fps.so", QStringLiteral("/usr/lib/gpu-pro/libgpupro_fps.so")})
    if (QFileInfo::exists(path)) return QFileInfo(path).canonicalFilePath();
  return {};
}

bool installForUser(QString &error) {
  const QString source = libraryNextToApp();
  if (source.isEmpty()) return error = "libgpupro_fps.so não encontrada junto do aplicativo.", false;
  // O caminho deve continuar válido após desmontar o AppImage e ser visível pelo Steam Runtime.
  if (!QDir().mkpath(QFileInfo(userManifest()).absolutePath())) return error = "Não foi possível criar ~/.local/share/vulkan.", false;
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/gpu-pro/vulkan";
  const QString library = dir + "/libgpupro_fps.so";
  if (!QDir().mkpath(dir)) return error = "Não foi possível criar " + dir + ".", false;
  QFile bundled(source);
  if (!bundled.open(QIODevice::ReadOnly)) return error = bundled.errorString(), false;
  const auto contents = bundled.readAll();
  if (bundled.error() != QFileDevice::NoError || contents.isEmpty())
    return error = "Falha ao ler a camada Vulkan do GPU-PRO em " + source + ".", false;
  QFile installed(library);
  if (!installed.open(QIODevice::ReadOnly) || installed.readAll() != contents) {
    QSaveFile copy(library);
    if (!copy.open(QIODevice::WriteOnly) || copy.write(contents) != contents.size() || !copy.commit())
      return error = "Falha ao instalar a camada Vulkan do GPU-PRO: " + copy.errorString(), false;
  }
  const QJsonObject layer{{"name", "VK_LAYER_GPUPRO_fps"},
                          {"type", "GLOBAL"},
                          {"library_path", library},
                          {"api_version", "1.3.0"},
                          {"implementation_version", "1"},
                          {"description", "GPU-PRO: contador de quadros para o FPS do HUD"},
                          {"disable_environment", QJsonObject{{"DISABLE_GPUPRO_FPS", "1"}}}};
  QSaveFile file(userManifest());
  if (!file.open(QIODevice::WriteOnly)) return error = file.errorString(), false;
  const auto manifest = QJsonDocument(QJsonObject{{"file_format_version", "1.1.2"}, {"layer", layer}}).toJson();
  if (file.write(manifest) != manifest.size() || !file.commit()) return error = file.errorString(), false;
  QSettings().setValue(preference, true);
  return true;
}

// Na primeira abertura, instala a camada. Nas demais, respeita a opção do usuário.
void refresh() {
  QString error;
  if (systemManifest().isEmpty() && QSettings().value(preference, true).toBool() && !installForUser(error))
    qWarning().noquote() << "GPU-PRO: FPS automático:" << error;
}

bool removeForUser(QString &error) {
  if (QFileInfo::exists(userManifest()) && !QFile::remove(userManifest())) return error = "Não foi possível remover " + userManifest(), false;
  QSettings().setValue(preference, false);
  QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/gpu-pro/vulkan/libgpupro_fps.so");
  return true;
}
}  // namespace FpsLayer
