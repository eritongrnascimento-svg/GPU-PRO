// Teste completo (sequência com pontuação geral, estilo AnTuTu) e resultados simplificados.
#include "window.h"

namespace {
QLabel *text(QString s, int size = 12) {
  auto *l = new QLabel(s);
  l->setWordWrap(true);
  l->setStyleSheet(QString("font-size:%1px;").arg(size));
  l->setTextFormat(Qt::PlainText);
  return l;
}
QPushButton *action(QString s, QBoxLayout *layout) {
  auto *b = new QPushButton(s);
  layout->addWidget(b);
  return b;
}
QString Points(const QJsonValue &v) { return QLocale(QLocale::Portuguese, QLocale::Brazil).toString(qRound64(v.toDouble())); }
QString Rate(double v, const QString &unit) {
  const QLocale br(QLocale::Portuguese, QLocale::Brazil);
  return br.toString(v, 'f', v >= 100 ? 0 : v >= 10 ? 1 : 2) + " " + unit;
}
QString Name(const QJsonObject &c) {
  static const QMap<QString, QString> names{{"h264", "H.264"}, {"hevc", "HEVC"}, {"av1", "AV1"}, {"jpeg", "JPEG"}, {"png", "PNG"}, {"webp", "WebP"}};
  const QString key = c.contains("codec") ? c["codec"].toString() : c["format"].toString();
  return names.value(key, key) + (c["mode"].toString() == "decode" ? " · decodificar" : " · codificar") +
         QString(" · %1×%2").arg(c["width"].toInt()).arg(c["height"].toInt());
}
QString Status(const QJsonObject &c, const QString &unit) {
  if (c["status"].toString() == "passed") return Rate(c["fps"].toDouble(), unit) + " · " + Rate(c["mpixPerSecond"].toDouble(), "MPix/s");
  return c["status"].toString() == "unsupported" ? "não suportado" : "falhou";
}
}  // namespace

// Linhas de resultado para os tipos com pontuação (0.8.1+); vazio para registros antigos.
QStringList MainWindow::resultSummary(const QString &kind, const QJsonObject &r) const {
  QStringList lines;
  if (!r.contains("points")) return lines;
  if (kind == "suite") {
    lines << "Pontuação total: " + Points(r["points"]) + " pontos";
    for (auto v : r["categories"].toArray()) {
      const auto c = v.toObject();
      lines << QString("%1: %2 pontos · %3").arg(c["name"].toString(), Points(c["points"]), c["detail"].toString());
    }
  } else if (kind == "graphics") {
    lines << "Pontuação 3D: " + Points(r["points"]) + " pontos";
    lines << QString("FPS mínimo %1 · médio %2 · máximo %3 · 1%% low %4")
                 .arg(r["fpsMin"].toDouble(), 0, 'f', 0).arg(r["fpsAvg"].toDouble(), 0, 'f', 0)
                 .arg(r["fpsMax"].toDouble(), 0, 'f', 0).arg(r["onePercentLow"].toDouble(), 0, 'f', 0);
    lines << QString("%1 · %2 × %3").arg(r["api"].toString()).arg(r["width"].toInt()).arg(r["height"].toInt());
  } else if (kind == "compute") {
    lines << "Memória: " + Points(r["memoryPoints"]) + " pontos · Computação: " + Points(r["computePoints"]) + " pontos";
    for (auto v : r["computeResults"].toArray()) {
      const auto c = v.toObject();
      lines << c["metric"].toString() + ": " + (c["status"].toString() == "passed" ? Rate(c["value"].toDouble(), c["unit"].toString()) : "não suportado");
    }
  } else if (kind == "video") {
    lines << "Vídeo: " + Points(r["videoPoints"]) + " pontos · Imagem: " + Points(r["imagePoints"]) + " pontos";
    for (auto v : r["codecResults"].toArray()) lines << Name(v.toObject()) + ": " + Status(v.toObject(), "FPS");
    for (auto v : r["imageResults"].toArray())
      if (v.toObject().contains("mode")) lines << Name(v.toObject()) + ": " + Status(v.toObject(), "img/s");
  } else if (kind == "cpu") {
    lines << "Processador: " + Points(r["points"]) + " pontos";
  }
  if (r.contains("methodText")) lines << "Método: " + r["methodText"].toString();
  if (r.contains("error")) lines << r["error"].toString();
  return lines;
}

QWidget *MainWindow::suitePage() {
  auto p = makeTestPanel("Teste completo · pontuação geral", "suite");
  auto *l = qobject_cast<QVBoxLayout *>(p.root->layout());
  auto *actions = new QHBoxLayout;
  p.start = action("Iniciar teste completo", actions);
  p.cancel = action("Cancelar", actions);
  p.cancel->setEnabled(false);
  l->insertLayout(1, actions);
  l->insertWidget(2, text("Todos os testes em sequência, como um benchmark completo: cenas 3D (Vulkan 1920×1080, 30 s), memória e "
                          "computação da GPU, codecs de vídeo e de imagem e processador. Leva cerca de 5 a 8 minutos e gera uma "
                          "pontuação total com a pontuação de cada categoria. Cada teste também pode ser feito sozinho nas outras abas."));
  connect(p.start, &QPushButton::clicked, this, &MainWindow::startSuite);
  connect(p.cancel, &QPushButton::clicked, this, [this] {
    suiteActive = false;
    graphicsProcess.kill();
    codecSuite.cancel();
    suiteCpuProcess.kill();
    log("Teste completo cancelado pelo usuário.");
  });
  panels["suite"] = p;
  if (offline) p.start->setEnabled(false);
  return p.root;
}

void MainWindow::startSuite() {
  if (testActive || offline || selected >= gpus.size()) return;
  suiteActive = true;
  suiteStage = 0;
  suiteParts = {};
  suiteCpu = {};
  log("Teste completo iniciado · " + gpus[selected].name);
  log("Teste completo · etapa 1 de 4 · Cenas 3D · Vulkan 1920×1080 · 30 s");
  startGraphics("graphics", "Vulkan", 1920, 1080, 30);
}

// Chamado depois que cada etapa termina e foi salva no histórico da própria categoria.
void MainWindow::continueSuite(const QJsonObject &r) {
  if (!suiteActive) return;
  suiteParts[r["kind"].toString()] = r;
  ++suiteStage;
  if (suiteStage == 1) {
    log("Teste completo · etapa 2 de 4 · Memória e computação da GPU");
    startGraphics("compute", "Vulkan", 1280, 720, 3);
  } else if (suiteStage == 2) {
    log("Teste completo · etapa 3 de 4 · Codecs de vídeo e imagem");
    activeKind = "video";
    setTesting(true);
    codecSuite.start(gpus[selected], true, true);
  } else if (suiteStage == 3) {
    log("Teste completo · etapa 4 de 4 · Processador (CPU)");
    activeKind = "suite";
    setTesting(true);
    runSuiteCpu(0);
  }
}

// CPU: as três cargas do núcleo compartilhado, 5 s cada, em todas as threads.
void MainWindow::runSuiteCpu(int mode) {
  if (!suiteActive) {
    setTesting(false);
    return;
  }
  if (mode > 2) return finishSuite();
  suiteCpuProcess.disconnect(this);
  connect(&suiteCpuProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, mode](int, QProcess::ExitStatus) {
    const auto r = QJsonDocument::fromJson(suiteCpuProcess.readAllStandardOutput()).object();
    suiteCpu.append(r);
    runSuiteCpu(mode + 1);
  });
  suiteCpuProcess.start(QCoreApplication::applicationDirPath() + "/gpu-cpu-worker",
                        {QString::number(mode), QString::number(qMax(1, QThread::idealThreadCount())), "5000"});
}

void MainWindow::finishSuite() {
  if (!suiteActive) return;
  suiteActive = false;
  double log = 0;
  int count = 0;
  for (auto v : suiteCpu)
    if (v.toObject()["valid"].toBool() && v.toObject()["operationsPerSecond"].toDouble() > 0) {
      log += std::log(v.toObject()["operationsPerSecond"].toDouble());
      ++count;
    }
  const qint64 cpuPoints = count ? qRound64(std::exp(log / count) / 10000) : 0;
  const auto graphics = suiteParts["graphics"].toObject(), compute = suiteParts["compute"].toObject(), codecs = suiteParts["video"].toObject();
  const QJsonArray categories{
      QJsonObject{{"name", "GPU · 3D"}, {"points", qRound64(graphics["points"].toDouble())}, {"detail", QString("%1 FPS médio").arg(graphics["fpsAvg"].toDouble(), 0, 'f', 0)}},
      QJsonObject{{"name", "Memória da GPU"}, {"points", qRound64(compute["memoryPoints"].toDouble())}, {"detail", "banda, cópia e PCIe"}},
      QJsonObject{{"name", "Computação da GPU"}, {"points", qRound64(compute["computePoints"].toDouble())}, {"detail", "FP32, FP64 e inteiros"}},
      QJsonObject{{"name", "Codecs de vídeo"}, {"points", qRound64(codecs["videoPoints"].toDouble())}, {"detail", "H.264, HEVC e AV1"}},
      QJsonObject{{"name", "Codecs de imagem"}, {"points", qRound64(codecs["imagePoints"].toDouble())}, {"detail", "JPEG, PNG e WebP"}},
      QJsonObject{{"name", "Processador"}, {"points", cpuPoints}, {"detail", QString("%1 threads").arg(QThread::idealThreadCount())}}};
  qint64 total = 0;
  for (auto v : categories) total += qRound64(v.toObject()["points"].toDouble());
  QJsonObject suite{{"valid", true},
                    {"kind", "suite"},
                    {"method", "suite-v1"},
                    {"api", "Vulkan"},
                    {"width", 1920},
                    {"height", 1080},
                    {"points", total},
                    {"categories", categories},
                    {"cpuResults", suiteCpu},
                    {"methodText", "Sequência fixa: cenas 3D (Vulkan, 1920×1080, 30 s), memória e computação da GPU, codecs de vídeo "
                                   "(4 resoluções) e de imagem, e processador (3 cargas de 5 s; pontuação = média das operações por "
                                   "segundo ÷ 10 000). A pontuação total é a soma das categorias."}};
  QStringList parts;
  for (auto v : categories) parts << v.toObject()["name"].toString() + " " + Points(v.toObject()["points"]);
  this->log("Teste completo concluído · " + gpus.value(selected).name + " · " + Points(total) + " pontos · " + parts.join(" · "));
  finishTest(suite);
}
