#include "window.h"
namespace {
QString number(const QJsonObject &o, QString key, QString unit = {}, double scale = 1) {
  return o[key].isDouble() ? QString::number(o[key].toDouble() / scale, 'f', scale == 1 ? 0 : 1) +
                                 (unit.isEmpty() ? "" : " " + unit)
                           : "Não informado pelo driver";
}
} // namespace
QWidget *MainWindow::specs() {
  auto *root = page("Especificações da GPU · ficha técnica");
  auto *l = qobject_cast<QVBoxLayout *>(root->layout());
  auto *refresh = new QPushButton("Atualizar ficha e consultar Vulkan");
  l->addWidget(refresh);
  specGrid = new QGridLayout;
  specGrid->setSpacing(14);
  l->addLayout(specGrid);
  l->addStretch();
  auto *info = new QLabel(
      "Clocks e sensores vêm do driver; características não expostas não são adivinhadas. FLOPS e "
      "largura de banda medidos pertencem aos benchmarks computacionais, não à ficha técnica. "
      "Dados brutos e dependências ficam em Feedback & Diagnóstico.");
  info->setWordWrap(true);
  l->addWidget(info);
  connect(refresh, &QPushButton::clicked, this, [this] { refreshSpecs(!offline); });
  return root;
}
void MainWindow::refreshSpecs(bool queryVulkan) {
  if (!specGrid)
    return;

  for (auto *v : specValues) v->parentWidget()->hide();
  if (selected < 0 || selected >= gpus.size())
    return;
  const auto &g = gpus[selected];
  auto s = g.specs;
  auto add = [&](QString key, QString value) {
    if (!specValues.contains(key)) {
      auto *card = new NeonCard(key);
      auto *valueLabel = new QLabel;
      valueLabel->setWordWrap(true);
      valueLabel->setTextFormat(Qt::PlainText);
      valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
      valueLabel->setMinimumHeight(40);
      card->body()->addWidget(valueLabel);
      int row = specValues.size();
      specGrid->addWidget(card, row / 3, row % 3);
      specValues[key] = valueLabel;
    }
    auto *valueLabel = specValues[key];
    valueLabel->setText(value.isEmpty() ? "Não informado pelo driver" : value);
    valueLabel->parentWidget()->show();
    QString metric = key.contains("Hotspot") ? "hotspot" : key.contains("memória", Qt::CaseInsensitive) && key.contains("Temperatura") ? "memoryTemp"
      : key.contains("Temperatura") ? "temp" : key.contains("Clock memória") ? "memClock"
      : key.contains("Clock") ? "clock" : key.startsWith("Ventoinha") ? "fan" : "temp";
    static_cast<NeonCard *>(valueLabel->parentWidget())->setReading(latest, metric, QColor(g.accent()));
  };
  add("Modelo", g.name);
  add("Fabricante", g.vendor);
  add("Arquitetura", s["architecture"].toString());
  add("Unidades de processamento",
      number(s, "gpuCores", g.vendor == "NVIDIA" ? "CUDA cores" : "unidades"));
  add("Memória dedicada", latest["vramTotal"].isDouble()
                              ? number(latest, "vramTotal", "GiB", 1073741824.)
                              : number(s, "memoryBytes", "GiB", 1073741824.));
  add("Barramento de memória", number(s, "memoryBusWidthBits", "bits"));
  add("Tipo de memória", "Não exposto pela API utilizada; sem inferência pelo nome do modelo");
  add("Clock GPU atual", displayMetric(latest, "clock"));
  add("Clock memória atual", displayMetric(latest, "memClock"));
  add("Clock máximo GPU reportado", number(s, "maxGraphicsMHz", "MHz"));
  add("Clock máximo memória reportado", number(s, "maxMemoryMHz", "MHz"));
  add("Potência atual · somente leitura", displayMetric(latest, "power"));
  add("Limite de potência · bloqueado para escrita", number(latest, "powerLimit", "W", 1000));
  add("Temperatura GPU", displayMetric(latest, "temp"));
  add("Hotspot", latest["hotspot"].isDouble() ? displayMetric(latest, "hotspot") : latest["hotspotStatus"].toString("Não exposto pelo driver"));
  add("Temperatura da memória", displayMetric(latest, "memoryTemp"));
  for (auto v : latest["fans"].toArray()) {
    auto fan = v.toObject();
    QStringList values;
    if (fan["rpm"].isDouble())
      values << number(fan, "rpm", "RPM");
    if (fan["percent"].isDouble())
      values << number(fan, "percent", "%");
    add(QString("Ventoinha %1 · %2").arg(fan["index"].toInt() + 1).arg(fan["source"].toString()),
        values.join(" · ") + " · " + fan["rpmMeaning"].toString());
  }
  add("Interface PCIe atual",
      s["pcieSpeed"].toString() + " · x" + s["pcieWidth"].toVariant().toString());
  add("Interface PCIe máxima",
      s["pcieMaxSpeed"].toString() + " · x" + s["pcieMaxWidth"].toVariant().toString());
  add("Driver", g.driver + " " + s["driverVersion"].toString());
  add("VBIOS", s["vbios"].toString());
  add("Identificador PCI", g.pci + " · " + g.deviceId);
  auto video = repository ? repository->recent("video", deviceKey(g), 1) : QJsonArray{};
  if (video.isEmpty())
    add("Vídeo · H.264 / HEVC / AV1",
        "Ainda não testado; execute Encoder + Decoder para validar cada caminho por hardware");
  else
    for (auto v : video[0].toObject()["codecResults"].toArray()) {
      auto c = v.toObject();
      add(c["codec"].toString().toUpper() + " · " + c["mode"].toString(), c["status"].toString());
    }
  if (queryVulkan) {
    cachedVulkan = vulkanSpecifications(g);
  }
  if (!cachedVulkan.isEmpty()) {
    auto v = cachedVulkan;
    add("Vulkan", v["Vulkan API"].toString(v["Vulkan"].toString()));
    add("Dispositivo Vulkan", v["Vulkan device"].toString());
    add("Ray tracing · extensão", v["Ray tracing pipeline extension"].toString());
    QString extensions = v["Vulkan extensions"].toString();
    add("Vulkan Video decode · extensão",
        extensions.contains("VK_KHR_video_decode_queue")
            ? "Exposta; codecs específicos dependem das capabilities"
            : "Não exposta nesta consulta");
    add("Vulkan Video encode · extensão",
        extensions.contains("VK_KHR_video_encode_queue")
            ? "Exposta; codecs específicos dependem das capabilities"
            : "Não exposta nesta consulta");
  }

}
