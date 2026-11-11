#include "window.h"
namespace {
QString number(const QJsonObject &o, QString key, QString unit = {}, double scale = 1) {
  return o[key].isDouble() ? QString::number(o[key].toDouble() / scale, 'f', scale == 1 ? 0 : 1) +
                                 (unit.isEmpty() ? "" : " " + unit)
                           : "Não informado pelo driver";
}
} // namespace
QWidget *MainWindow::specs() {
  auto *root = specContent = page("Especificações da GPU · ficha técnica");
  auto *l = qobject_cast<QVBoxLayout *>(root->layout());
  auto *refresh = new QPushButton("Atualizar ficha e consultar Vulkan");
  l->addWidget(refresh);
  specHost = new QWidget;
  specHost->setObjectName("specCards");
  specGrid = new QGridLayout(specHost);
  specGrid->setContentsMargins(0, 0, 0, 0);
  specGrid->setSpacing(8);
  l->addWidget(specHost);
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
      valueLabel->setMinimumHeight(0);
      card->setCompact(true);
      card->body()->addWidget(valueLabel);
      specOrder << key;
      int row = specValues.size();
      specGrid->addWidget(card, row / 3, row % 3);
      specValues[key] = valueLabel;
    }
    auto *valueLabel = specValues[key];
    QString full = value.isEmpty() ? "Não informado pelo driver" : value;
    valueLabel->setToolTip(full);
    QString visible = full.startsWith("Não informado") ? "N/D"
        : full.startsWith("Não exposto") || full.startsWith("Não exposta") ? "Não exposto pelo driver"
        : full.startsWith("Ainda não testado") ? "Não testado" : full;
    valueLabel->setText(visible);
    valueLabel->parentWidget()->show();
    QString metric = key.contains("Hotspot") ? "hotspot" : key.contains("memória", Qt::CaseInsensitive) && key.contains("Temperatura") ? "memoryTemp"
      : key.contains("Temperatura") ? "temp" : key.contains("Clock memória") ? "memClock"
      : key.contains("Clock") ? "clock" : key.startsWith("Ventoinha") ? "fan" : "temp";
    static_cast<NeonCard *>(valueLabel->parentWidget())->setReading(latest, metric, QColor(g.accent()));
  };
  add("Modelo", g.name);
  add("Fabricante", g.vendor);
  add("Arquitetura", s["architecture"].toString());
  add("Unidades de processamento",s["executionUnits"].isDouble()?number(s,"executionUnits","EUs (não CUDA cores)"):number(s,"gpuCores",g.vendor=="NVIDIA"?"CUDA cores":"unidades"));
  if(s["executionUnits"].isDouble())add("Topologia Intel",number(s,"sliceCount","slices")+" · "+number(s,"subsliceCount","subslices"));
  if(g.vendor=="Intel")add("Diagnóstico Intel",s["intelProbeStatus"].toString());
  bool integrated=s["integrated"].toBool()||cachedVulkan["integrated"].toBool();
  add("Memória dedicada", integrated?"Não se aplica · RAM compartilhada":latest["vramTotal"].isDouble()
                              ? number(latest, "vramTotal", "GiB", 1073741824.)
                              : number(s, "memoryBytes", "GiB", 1073741824.));
  add("Barramento de memória",integrated?"Compartilhado com a memória do sistema":number(s,"memoryBusWidthBits","bits"));
  add("Tipo de memória",integrated?"RAM compartilhada (UMA)":s["memoryType"].toString("Não informado pelo driver; sem inferência pelo nome do modelo"));
  add("Clock GPU atual", displayMetric(latest, "clock"));
  add("Clock memória atual",integrated?"Não se aplica como clock de VRAM dedicada":displayMetric(latest,"memClock"));
  add("Clock máximo GPU reportado", number(s, "maxGraphicsMHz", "MHz"));
  add("Clock máximo memória reportado",integrated?"Não se aplica como clock de VRAM dedicada":number(s,"maxMemoryMHz","MHz"));
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
      integrated?"Não se aplica · GPU integrada":s["pcieSpeed"].toString() + " · x" + s["pcieWidth"].toVariant().toString());
  add("Interface PCIe máxima",
      integrated?"Não se aplica · GPU integrada":s["pcieMaxSpeed"].toString() + " · x" + s["pcieMaxWidth"].toVariant().toString());
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
    if(v["integrated"].toBool()&&v["hostVisibleHeapBytes"].isDouble())add("Heap compartilhado reportado",number(v,"hostVisibleHeapBytes","GiB · capacidade do driver, não VRAM dedicada",1073741824.));
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
  queueSpecAdapt();
}

void MainWindow::queueSpecAdapt() {
  if (specAdaptPending) return;
  specAdaptPending = true;
  QTimer::singleShot(0, this, [this] { specAdaptPending = false; adaptSpecs(); });
}
bool MainWindow::eventFilter(QObject *object, QEvent *event) {
  if (object == specViewport && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) queueSpecAdapt();
  return QMainWindow::eventFilter(object, event);
}
void MainWindow::adaptSpecs() {
  if (!specViewport || !specHost || !specContent) return;
  QStringList keys;
  for (const auto &key : specOrder) if (!specValues[key]->parentWidget()->isHidden()) keys << key;
  if (keys.isEmpty()) return;
  auto *layout = specContent->layout();
  const auto margins = layout->contentsMargins();
  int width = qMax(180, specViewport->width() - margins.left() - margins.right());
  int reserved = margins.top() + margins.bottom(), widgets = 0;
  for (int i = 0; i < layout->count(); ++i) {
    auto *widget = layout->itemAt(i)->widget();
    if (!widget || widget->isHidden()) continue;
    ++widgets;
    if (widget != specHost) reserved += widget->hasHeightForWidth() ? widget->heightForWidth(width) : widget->sizeHint().height();
  }
  reserved += qMax(0, widgets - 1) * layout->spacing();
  int available = qMax(100, specViewport->height() - reserved - 2);
  int maxCols = qBound(1, (width + 8) / 178, 8), columns = maxCols, minimum = 70;
  auto minimumFor = [&](int cols) {
    int cardWidth = (width - (cols - 1) * 8) / cols, height = 64;
    for (const auto &key : keys) {
      auto *card = static_cast<NeonCard *>(specValues[key]->parentWidget());
      auto *head = qobject_cast<QLabel *>(card->body()->itemAt(0)->widget());
      auto measure = [&](QLabel *label) { return label->fontMetrics().boundingRect(QRect(0,0,qMax(40,cardWidth-20),2000),Qt::TextWordWrap,label->text()).height(); };
      height = qMax(height, measure(head) + measure(specValues[key]) + 22);
    }
    return height;
  };
  bool fits = false;
  for (int cols = 1; cols <= maxCols; ++cols) {
    int rows = (keys.size() + cols - 1) / cols, minHeight = minimumFor(cols);
    if (rows * minHeight + (rows - 1) * 8 <= available) { columns = cols; minimum = minHeight; fits = true; break; }
  }
  if (!fits) minimum = minimumFor(columns);
  int rows = (keys.size() + columns - 1) / columns;
  int cardHeight = fits ? qMax(minimum, (available - (rows - 1) * 8) / rows) : minimum;
  if (columns != specColumns || arrangedSpecs != keys) {
    while (auto *item = specGrid->takeAt(0)) delete item;
    for (int c = 0; c < 8; ++c) specGrid->setColumnStretch(c, c < columns ? 1 : 0);
    for (int row = 0; row < specOrder.size(); ++row) { specGrid->setRowMinimumHeight(row, 0); specGrid->setRowStretch(row, 0); }
    for (int i = 0; i < keys.size(); ++i) specGrid->addWidget(specValues[keys[i]]->parentWidget(), i / columns, i % columns);
    specColumns = columns; arrangedSpecs = keys;
  }
  for (const auto &key : keys) specValues[key]->parentWidget()->setMinimumHeight(cardHeight);
  specHost->setMinimumHeight(rows * cardHeight + (rows - 1) * 8);
}
