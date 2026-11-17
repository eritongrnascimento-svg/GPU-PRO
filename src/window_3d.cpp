// Configurações 3D & Filtros (igual à versão Windows): o que a GPU suporta conforme o modelo e o
// driver, quais bibliotecas existem no sistema, e opções extras do perfil de jogo (Gamescope,
// Proton/DXVK/VKD3D e GameMode). Tecnologias que só o jogo ativa aparecem indicadas como tal.
#include "window.h"
#include "benchmark.h"

namespace {
bool vulkanLayerPresent(const QStringList &patterns) {
  for (const QString &folder : {QString("/usr/share/vulkan/implicit_layer.d"), QString("/usr/share/vulkan/explicit_layer.d"),
                                QString("/etc/vulkan/implicit_layer.d"), QDir::homePath() + "/.local/share/vulkan/implicit_layer.d"})
    if (!QDir(folder).entryList(patterns, QDir::Files).isEmpty()) return true;
  return false;
}
bool dxvkNvapiPresent() {
  for (const QString &folder : {"/usr/lib/nvidia/wine", "/usr/lib64/nvidia/wine", "/usr/lib/x86_64-linux-gnu/nvidia/wine"})
    if (QFileInfo::exists(folder + "/nvngx.dll")) return true;
  return false;
}
bool has(const QString &exe) { return !QStandardPaths::findExecutable(exe).isEmpty(); }
}

void MainWindow::buildThreeDFeatures(QVBoxLayout *l) {
  auto *box = new QGroupBox("Recursos suportados por esta GPU e pelo sistema");
  auto *bl = new QVBoxLayout(box);
  featureGrid = new QGridLayout;
  featureGrid->setSpacing(8);
  bl->addLayout(featureGrid);
  bl->addWidget(new QLabel("FSR, XeSS e DLSS são ativados dentro de cada jogo. No Linux, DLSS em jogos Windows usa Proton com DXVK-NVAPI; "
                      "ray tracing DX12 usa VKD3D-Proton. As opções abaixo entram no comando de lançamento do perfil."));
  qobject_cast<QLabel *>(bl->itemAt(1)->widget())->setWordWrap(true);
  l->insertWidget(1, box);
  refreshThreeDFeatures();
}

void MainWindow::refreshThreeDFeatures() {
  if (!featureGrid) return;
  while (auto *item = featureGrid->takeAt(0)) { delete item->widget(); delete item; }
  const auto g = gpus.value(selected);
  const QString vendor = g.vendor, name = g.name;
  const int arch = g.specs["architectureId"].toInt();
  const bool rtx = vendor == "NVIDIA" && arch >= 6;
  const QString extensions = cachedVulkan["Vulkan extensions"].toString();
  const bool vkRt = extensions.contains("VK_KHR_ray_tracing_pipeline") || cachedVulkan["Ray tracing pipeline extension"].toString().startsWith("Exposta");
  const bool vulkan = !cachedVulkan.isEmpty() || offline;
  const bool vkb = vulkanLayerPresent({"*vkBasalt*.json", "*vkbasalt*.json"}), mango = has("mangohud") || vulkanLayerPresent({"*MangoHud*.json", "*mangohud*.json"});
  const QList<std::tuple<QString, QString, bool>> rows = {
      {"Ray tracing · Vulkan", cachedVulkan.isEmpty() ? "Consulte a ficha (Especificações) para detectar" : vkRt ? "VK_KHR_ray_tracing_pipeline" : "Não exposto pelo driver", vkRt},
      {"Ray tracing · DX12 (Proton)", vkRt ? "VKD3D-Proton · opção no perfil" : "Requer ray tracing Vulkan", vkRt},
      {"DLSS Super Resolution / DLAA", rtx ? (dxvkNvapiPresent() ? "Suportado · DXVK-NVAPI encontrado" : "Suportado · requer DXVK-NVAPI do driver") : "Requer NVIDIA RTX", rtx},
      {"DLSS Frame Generation", vendor == "NVIDIA" && arch >= 8 ? "Suportado (RTX 40+)" : "Requer RTX série 40 ou mais nova", vendor == "NVIDIA" && arch >= 8},
      {"DLSS Multi Frame Generation", vendor == "NVIDIA" && arch >= 10 ? "Suportado (RTX 50)" : "Requer RTX série 50", vendor == "NVIDIA" && arch >= 10},
      {"AMD FSR 1 / 2 / 3", vulkan ? "Disponível nos jogos compatíveis" : "Requer Vulkan", vulkan},
      {"AMD FSR 4", vendor == "AMD" && name.contains("RX 9") ? "Suportado (RDNA 4)" : "Requer Radeon RX 9000", vendor == "AMD" && name.contains("RX 9")},
      {"Intel XeSS", vendor == "Intel" && name.contains("Arc") ? "XMX (Arc)" : "DP4a nos jogos compatíveis", true},
      {"Gamescope · FSR / NIS", has("gamescope") ? "Instalado · upscale e nitidez no perfil" : "Não instalado", has("gamescope")},
      {"vkBasalt · CAS / FXAA / SMAA", vkb ? "Instalado · nitidez e antisserrilhamento" : "Não instalado", vkb},
      {"MangoHud · limite de FPS", mango ? "Instalado" : "Não instalado", mango},
      {"GameMode", has("gamemoderun") ? "Instalado · prioridade de desempenho" : "Não instalado", has("gamemoderun")}};
  int i = 0;
  for (const auto &[title, value, ok] : rows) {
    auto *card = new NeonCard(title);
    card->setCompact(true);
    auto *v = new QLabel(value);
    v->setWordWrap(true);
    card->body()->addWidget(v);
    card->setReading(ok ? QJsonObject{{"static", 0}} : QJsonObject{}, "static", QColor(g.accent()));
    featureGrid->addWidget(card, i / 3, i % 3);
    ++i;
  }
  if (gsFilter) {
    protonDlss->setEnabled(rtx);
    protonRt->setEnabled(vkRt || offline);
    gameMode->setEnabled(has("gamemoderun") || offline);
  }
}

// Opções extras do perfil: filtro/nitidez/HDR/VRR do Gamescope e Proton (DLSS, DXR, HDR) + GameMode.
void MainWindow::buildThreeDProfileOptions(QFormLayout *gamescope, QVBoxLayout *proton) {
  gsFilter = new QComboBox;
  gsFilter->addItem("FSR (AMD, nitidez ajustável)", "fsr");
  gsFilter->addItem("NIS (NVIDIA Image Scaling)", "nis");
  gsFilter->addItem("Linear", "linear");
  gsFilter->addItem("Pixel (nearest)", "pixel");
  gsSharpness = new QSpinBox;
  gsSharpness->setRange(0, 20);
  gsSharpness->setValue(5);
  gsSharpness->setToolTip("0 = mais nítido · 20 = mais suave (padrão do Gamescope: 5)");
  gsHdr = new QCheckBox("HDR no Gamescope");
  gsVrr = new QCheckBox("VRR (adaptive sync)");
  gamescope->addRow("Filtro", gsFilter);
  gamescope->addRow("Nitidez", gsSharpness);
  gamescope->addRow(gsHdr);
  gamescope->addRow(gsVrr);
  protonDlss = new QCheckBox("DLSS via DXVK-NVAPI (NVIDIA RTX)");
  protonRt = new QCheckBox("Ray tracing DX12 via VKD3D-Proton (DXR)");
  protonHdr = new QCheckBox("HDR no DXVK/Proton");
  gameMode = new QCheckBox("Iniciar com GameMode");
  for (auto *c : {protonDlss, protonRt, protonHdr, gameMode}) proton->addWidget(c);
}

void MainWindow::applyThreeDProfileOptions(QStringList &env, QStringList &wrapper, QJsonObject &profile) {
  if (!gsFilter) return;
  if (protonDlss->isChecked()) env << "PROTON_ENABLE_NVAPI=1" << "DXVK_ENABLE_NVAPI=1";
  if (protonRt->isChecked()) env << "VKD3D_CONFIG=dxr11";
  if (protonHdr->isChecked()) env << "DXVK_HDR=1" << "PROTON_ENABLE_HDR=1";
  const int gs = wrapper.indexOf("gamescope");
  if (gs >= 0) {
    const int filter = wrapper.indexOf("-F", gs);
    if (filter >= 0 && filter + 1 < wrapper.size()) wrapper[filter + 1] = gsFilter->currentData().toString();
    const int separator = wrapper.indexOf("--", gs);
    QStringList extra;
    if (gsFilter->currentData().toString() == "fsr" || gsFilter->currentData().toString() == "nis") extra << "--sharpness" << QString::number(gsSharpness->value());
    if (gsHdr->isChecked()) extra << "--hdr-enabled";
    if (gsVrr->isChecked()) extra << "--adaptive-sync";
    for (int i = 0; i < extra.size(); ++i) wrapper.insert(separator + i, extra[i]);
  }
  if (gameMode->isChecked()) wrapper.prepend("gamemoderun");
  profile["gamescopeFilter"] = gsFilter->currentData().toString();
  profile["gamescopeSharpness"] = gsSharpness->value();
  profile["gamescopeHdr"] = gsHdr->isChecked();
  profile["gamescopeVrr"] = gsVrr->isChecked();
  profile["protonDlss"] = protonDlss->isChecked();
  profile["protonRayTracing"] = protonRt->isChecked();
  profile["protonHdr"] = protonHdr->isChecked();
  profile["gameMode"] = gameMode->isChecked();
}

void MainWindow::loadThreeDProfileOptions(const QJsonObject &p) {
  if (!gsFilter) return;
  gsFilter->setCurrentIndex(qMax(0, gsFilter->findData(p["gamescopeFilter"].toString("fsr"))));
  gsSharpness->setValue(p["gamescopeSharpness"].toInt(5));
  gsHdr->setChecked(p["gamescopeHdr"].toBool());
  gsVrr->setChecked(p["gamescopeVrr"].toBool());
  protonDlss->setChecked(p["protonDlss"].toBool());
  protonRt->setChecked(p["protonRayTracing"].toBool());
  protonHdr->setChecked(p["protonHdr"].toBool());
  gameMode->setChecked(p["gameMode"].toBool());
}
