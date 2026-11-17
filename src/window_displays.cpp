// Monitores & Telas (igual à versão Windows): além do modo, a saída selecionada pode ter
// rotação, monitor principal, escala, HDR, VRR, perfil de cor e brilho/contraste por DDC/CI
// ajustados no próprio app. Backends: xrandr (X11), kscreen-doctor (KDE Wayland) e ddcutil.
// Mudanças de geometria pedem confirmação e voltam ao estado anterior em 15 s.
#include "window.h"

namespace {
const QStringList rotationNames = {"Paisagem", "Retrato (esquerda)", "Paisagem invertida", "Retrato (direita)"};
const QStringList xrandrRotations = {"normal", "left", "inverted", "right"};
const QStringList kscreenRotations = {"none", "left", "inverted", "right"};
int kscreenRotationIndex(int flag) { return flag == 2 ? 1 : flag == 4 ? 2 : flag == 8 ? 3 : 0; }
QLabel *note(const QString &text) {
  auto *l = new QLabel(text);
  l->setWordWrap(true);
  auto f = l->font();
  f.setPixelSize(12);
  l->setFont(f);
  return l;
}
}

void MainWindow::buildDisplayExtras(QVBoxLayout *l) {
  auto *box = new QGroupBox("Ajustes da saída selecionada");
  auto *grid = new QGridLayout(box);
  grid->setHorizontalSpacing(16);
  grid->setVerticalSpacing(8);
  rotationChoice = new QComboBox;
  rotationChoice->addItems(rotationNames);
  scaleChoice = new QComboBox;
  for (int pct : {100, 125, 150, 175, 200, 225, 250, 300}) scaleChoice->addItem(QString("%1 %").arg(pct), pct / 100.0);
  vrrChoice = new QComboBox;
  vrrChoice->addItems({"VRR desligado", "VRR sempre", "VRR automático (jogos)"});
  hdrToggle = new QCheckBox("HDR");
  primaryButton = new QPushButton("Tornar principal");
  iccChoice = new QComboBox;
  iccChoice->setMinimumWidth(240);
  brightnessSlider = new QSlider(Qt::Horizontal);
  contrastSlider = new QSlider(Qt::Horizontal);
  for (auto *s : {brightnessSlider, contrastSlider}) { s->setRange(0, 100); s->setEnabled(false); s->setMinimumWidth(180); }
  const int row = 0;
  grid->addWidget(new QLabel("Rotação"), row, 0); grid->addWidget(rotationChoice, row, 1);
  grid->addWidget(new QLabel("Escala"), row, 2); grid->addWidget(scaleChoice, row, 3);
  grid->addWidget(new QLabel("Taxa variável"), 1, 0); grid->addWidget(vrrChoice, 1, 1);
  grid->addWidget(hdrToggle, 1, 2); grid->addWidget(primaryButton, 1, 3);
  grid->addWidget(new QLabel("Perfil de cor"), 2, 0); grid->addWidget(iccChoice, 2, 1, 1, 3);
  grid->addWidget(new QLabel("Brilho (DDC/CI)"), 3, 0); grid->addWidget(brightnessSlider, 3, 1);
  grid->addWidget(new QLabel("Contraste (DDC/CI)"), 3, 2); grid->addWidget(contrastSlider, 3, 3);
  displayExtraInfo = note("Consulte os modos para habilitar os ajustes.");
  grid->addWidget(displayExtraInfo, 4, 0, 1, 4);
  l->addWidget(box);
  // Único atalho para as configurações de tela do sistema.
  auto *system = new QPushButton("Abrir configurações de tela do sistema");
  l->addWidget(system, 0, Qt::AlignLeft);
  connect(system, &QPushButton::clicked, this, [this] {
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    QList<QPair<QString, QStringList>> options;
    if (desktop.contains("KDE", Qt::CaseInsensitive)) options << qMakePair(QString("kcmshell6"), QStringList{"kcm_kscreen"}) << qMakePair(QString("systemsettings"), QStringList{"kcm_kscreen"});
    if (desktop.contains("GNOME", Qt::CaseInsensitive)) options << qMakePair(QString("gnome-control-center"), QStringList{"display"});
    if (desktop.contains("XFCE", Qt::CaseInsensitive)) options << qMakePair(QString("xfce4-display-settings"), QStringList{});
    options << qMakePair(QString("kcmshell6"), QStringList{"kcm_kscreen"}) << qMakePair(QString("gnome-control-center"), QStringList{"display"})
            << qMakePair(QString("arandr"), QStringList{});
    for (const auto &[exe, args] : options)
      if (!QStandardPaths::findExecutable(exe).isEmpty() && QProcess::startDetached(exe, args)) { log("Configurações de tela do sistema abertas: " + exe); return; }
    notify("Configurações de tela do sistema não encontradas", "Instale ou abra as configurações de tela do seu ambiente gráfico.");
  });

  connect(rotationChoice, qOverload<int>(&QComboBox::activated), this, [this](int idx) {
    auto o = currentDisplay(); if (o.isEmpty()) return;
    const bool x11 = o["backend"].toString() == "xrandr";
    const int old = x11 ? qMax(0, xrandrRotations.indexOf(o["rotation"].toString("normal"))) : kscreenRotationIndex(o["rotation"].toInt(1));
    if (idx == old) return;
    auto args = [&](int i) { return x11 ? QStringList{"--output", o["name"].toString(), "--rotate", xrandrRotations[i]}
                                        : QStringList{QString("output.%1.rotation.%2").arg(o["id"].toInt()).arg(kscreenRotations[i])}; };
    applyDisplayChange(o["backend"].toString(), args(idx), args(old), "rotação " + rotationNames[idx], true);
  });
  connect(scaleChoice, qOverload<int>(&QComboBox::activated), this, [this](int) {
    auto o = currentDisplay(); if (o.isEmpty() || o["backend"].toString() != "kscreen-doctor") return;
    const double next = scaleChoice->currentData().toDouble(), old = o["scale"].toDouble(1.0);
    if (qFuzzyCompare(next, old)) return;
    auto arg = [&](double v) { return QStringList{QString("output.%1.scale.%2").arg(o["id"].toInt()).arg(v, 0, 'f', 2)}; };
    applyDisplayChange("kscreen-doctor", arg(next), arg(old), QString("escala %1 %").arg(qRound(next * 100)), true);
  });
  connect(vrrChoice, qOverload<int>(&QComboBox::activated), this, [this](int idx) {
    auto o = currentDisplay(); if (o.isEmpty() || o["backend"].toString() != "kscreen-doctor") return;
    const QStringList policy = {"never", "always", "automatic"};
    applyDisplayChange("kscreen-doctor", {QString("output.%1.vrrpolicy.%2").arg(o["id"].toInt()).arg(policy[idx])}, {}, vrrChoice->currentText(), false);
  });
  connect(hdrToggle, &QCheckBox::clicked, this, [this](bool on) {
    auto o = currentDisplay(); if (o.isEmpty() || o["backend"].toString() != "kscreen-doctor") return;
    const QString id = QString::number(o["id"].toInt());
    applyDisplayChange("kscreen-doctor", {"output." + id + ".hdr." + (on ? "enable" : "disable"), "output." + id + ".wcg." + (on ? "enable" : "disable")},
                       {"output." + id + ".hdr." + (on ? "disable" : "enable"), "output." + id + ".wcg." + (on ? "disable" : "enable")},
                       on ? "HDR ativado" : "HDR desativado", true);
  });
  connect(primaryButton, &QPushButton::clicked, this, [this] {
    auto o = currentDisplay(); if (o.isEmpty()) return;
    const bool x11 = o["backend"].toString() == "xrandr";
    applyDisplayChange(o["backend"].toString(), x11 ? QStringList{"--output", o["name"].toString(), "--primary"}
                                                    : QStringList{QString("output.%1.priority.1").arg(o["id"].toInt())},
                       {}, "monitor principal: " + o["name"].toString(), false);
  });
  connect(iccChoice, qOverload<int>(&QComboBox::activated), this, [this](int) {
    auto o = currentDisplay(); const QString path = iccChoice->currentData().toString();
    if (o.isEmpty() || o["backend"].toString() != "kscreen-doctor") return;
    applyDisplayChange("kscreen-doctor", {QString("output.%1.iccprofile.%2").arg(o["id"].toInt()).arg(path.isEmpty() ? QString("none") : path)}, {},
                       "perfil de cor " + iccChoice->currentText(), false);
  });
  for (auto *slider : {brightnessSlider, contrastSlider})
    connect(slider, &QSlider::sliderReleased, this, [this, slider] {
      const int display = ddcDisplays.value(currentDisplay()["name"].toString(), -1);
      if (display < 0) return;
      const QString code = slider == brightnessSlider ? "10" : "12";
      auto *p = new QProcess(this);
      connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, p, slider](int exit, QProcess::ExitStatus) {
        if (exit == 0) log(QString("Monitor %1: %2 %3").arg(currentDisplay()["name"].toString(), slider == brightnessSlider ? "brilho" : "contraste").arg(slider->value()));
        else notify("DDC/CI recusado pelo monitor", QString::fromUtf8(p->readAllStandardError()).trimmed());
        p->deleteLater();
      });
      p->start("ddcutil", {"--display", QString::number(display), "setvcp", code, QString::number(slider->value())});
    });
  connect(displayChoice, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updateDisplayExtras(); });
  if (offline) for (auto *w : std::initializer_list<QWidget *>{rotationChoice, scaleChoice, vrrChoice, hdrToggle, primaryButton, iccChoice}) w->setEnabled(false);
}

QJsonObject MainWindow::currentDisplay() const {
  const int idx = displayChoice ? displayChoice->currentIndex() : -1;
  return idx >= 0 && idx < displayOutputs.size() ? displayOutputs[idx].toObject() : QJsonObject{};
}

// Preenche os controles com o estado da saída escolhida e consulta DDC/CI (ddcutil) quando instalado.
void MainWindow::updateDisplayExtras() {
  auto o = currentDisplay();
  const bool any = !o.isEmpty(), kde = o["backend"].toString() == "kscreen-doctor";
  rotationChoice->setEnabled(any);
  primaryButton->setEnabled(any && !o["primary"].toBool());
  primaryButton->setText(o["primary"].toBool() ? "Monitor principal" : "Tornar principal");
  for (auto *w : std::initializer_list<QWidget *>{scaleChoice, vrrChoice, hdrToggle, iccChoice}) w->setEnabled(kde);
  if (!any) return;
  rotationChoice->setCurrentIndex(kde ? kscreenRotationIndex(o["rotation"].toInt(1)) : qMax(0, xrandrRotations.indexOf(o["rotation"].toString("normal"))));
  if (kde) {
    const int scale = qRound(o["scale"].toDouble(1.0) * 100);
    for (int i = 0; i < scaleChoice->count(); ++i) if (qRound(scaleChoice->itemData(i).toDouble() * 100) == scale) scaleChoice->setCurrentIndex(i);
    vrrChoice->setCurrentIndex(qBound(0, o["vrrPolicy"].toInt(2), 2));
    hdrToggle->setChecked(o["hdr"].toBool());
  }
  iccChoice->clear();
  iccChoice->addItem("Sem perfil (padrão)", QString());
  QStringList folders = {"/usr/share/color/icc", "/usr/local/share/color/icc", QDir::homePath() + "/.local/share/icc", QDir::homePath() + "/.color/icc"};
  for (const auto &folder : folders)
    for (const auto &file : QDir(folder).entryInfoList({"*.icc", "*.icm", "*.ICC", "*.ICM"}, QDir::Files, QDir::Name))
      iccChoice->addItem(file.fileName(), file.absoluteFilePath());
  const QString currentIcc = o["iccProfile"].toString();
  if (!currentIcc.isEmpty()) iccChoice->setCurrentIndex(qMax(0, iccChoice->findData(currentIcc)));
  displayExtraInfo->setText(kde ? "KDE Wayland: rotação, escala, HDR, VRR, perfil de cor e monitor principal pelo kscreen-doctor. Rotação, escala e HDR revertem em 15 s sem confirmação."
                                : "X11: rotação e monitor principal pelo xrandr (revertem em 15 s sem confirmação). Escala, HDR, VRR e perfil de cor dependem do compositor e não são expostos pelo xrandr.");
  brightnessSlider->setEnabled(false);
  contrastSlider->setEnabled(false);
  if (QStandardPaths::findExecutable("ddcutil").isEmpty()) {
    displayExtraInfo->setText(displayExtraInfo->text() + " Brilho e contraste: instale o ddcutil (e ative DDC/CI no menu do monitor).");
    return;
  }
  auto *detect = new QProcess(this);
  connect(detect, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, detect](int, QProcess::ExitStatus) {
    // "Display 1 ... DRM connector: card1-DP-1" → associa o número do ddcutil ao nome da saída.
    ddcDisplays.clear();
    int display = -1;
    for (const auto &line : QString::fromUtf8(detect->readAllStandardOutput()).split('\n')) {
      auto t = line.trimmed();
      if (t.startsWith("Display ")) display = t.section(' ', 1, 1).toInt();
      if (t.startsWith("DRM connector:") && display > 0) ddcDisplays[t.section(':', 1).trimmed().section('-', 1)] = display;
    }
    detect->deleteLater();
    const int target = ddcDisplays.value(currentDisplay()["name"].toString(), -1);
    if (target < 0) return;
    for (auto [slider, code] : std::initializer_list<std::pair<QSlider *, QString>>{{brightnessSlider, "10"}, {contrastSlider, "12"}}) {
      auto *get = new QProcess(this);
      connect(get, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [get, slider](int exit, QProcess::ExitStatus) {
        // --brief: "VCP 10 C 50 100"
        const auto parts = QString::fromUtf8(get->readAllStandardOutput()).simplified().split(' ');
        if (exit == 0 && parts.size() >= 5) { slider->setRange(0, parts[4].toInt()); slider->setValue(parts[3].toInt()); slider->setEnabled(true); }
        get->deleteLater();
      });
      get->start("ddcutil", {"--display", QString::number(target), "--brief", "getvcp", code});
    }
  });
  detect->start("ddcutil", {"detect"});
}

// Aplica um comando de tela; com reversão, pede confirmação e volta ao estado anterior em 15 s.
void MainWindow::applyDisplayChange(const QString &exe, const QStringList &args, const QStringList &revert, const QString &what, bool confirm) {
  if (offline || displayBusy) return;
  displayBusy = true;
  auto *change = new QProcess(this);
  connect(change, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [=](int code, QProcess::ExitStatus status) {
    const QString error = QString::fromUtf8(change->readAllStandardError()).trimmed();
    change->deleteLater();
    if (code != 0 || status != QProcess::NormalExit) {
      displayBusy = false;
      notify("Monitor: " + what + " não aplicado", error);
      refreshDisplays();
      return;
    }
    bool keep = true;
    if (confirm && !revert.isEmpty()) {
      QMessageBox ask(QMessageBox::Question, "Confirmar ajuste de tela", "Manter " + what + "? Volta ao estado anterior em 15 segundos.",
                      QMessageBox::Yes | QMessageBox::No, this);
      ask.setDefaultButton(QMessageBox::No);
      QTimer::singleShot(15000, &ask, &QDialog::reject);
      keep = ask.exec() == QMessageBox::Yes;
    }
    if (!keep) QProcess::execute(exe, revert);
    displayBusy = false;
    log("Monitor " + currentDisplay()["name"].toString() + ": " + what + (keep ? " aplicado" : " revertido"));
    refreshDisplays();
  });
  connect(change, &QProcess::errorOccurred, this, [this, change, what, exe](QProcess::ProcessError e) {
    if (e != QProcess::FailedToStart) return;
    displayBusy = false;
    notify("Monitor: " + what + " não aplicado", exe + " não encontrado");
    change->deleteLater();
  });
  change->start(exe, args);
}
