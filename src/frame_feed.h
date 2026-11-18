#pragma once
#include <QtCore>

// Recebe os quadros enviados pela camada libgpupro_fps.so (Vulkan, e OpenGL pelo lançador
// gpu-pro-fps) num socket local abstrato "gpu-pro-frames" e escolhe o jogo automaticamente.
class FrameFeed {
public:
  struct Source {
    QString name, api;
    QList<double> times;
    qint64 last = 0, recentStart = 0;
    int recent = 0;
    double rate = 0;
  };
  FrameFeed();
  ~FrameFeed();
  bool listening() const { return fd >= 0; }
  void poll();
  // Processos apresentando quadros agora, do mais rápido ao mais lento (sem compositores/sobreposições).
  QList<QPair<int, Source>> active() const;
  QJsonArray times(int pid) const;
  // Último jogo escolhido enquanto continuar ativo; senão o de mais quadros por segundo.
  int pick();
  QString describe(int pid) const;
  void parse(const QByteArray &datagram, qint64 now);  // público para os testes

private:
  int fd = -1, sticky = 0;
  QHash<int, Source> sources;
};

// Camada Vulkan implícita do GPU-PRO para o usuário (~/.local/share/vulkan/implicit_layer.d).
namespace FpsLayer {
QString systemManifest();  // instalada pelo pacote DEB, se existir
QString userManifest();
bool active();
QString libraryNextToApp();
bool installForUser(QString &error);
bool removeForUser(QString &error);
void refresh();
}
