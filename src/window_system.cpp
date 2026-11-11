#include "system_settings.h"
#include "window.h"
QWidget *MainWindow::systemControls() {
  auto *box = new QGroupBox("GPU preferida e áudio");
  auto *layout = new QVBoxLayout(box);
  auto *choice = new QComboBox;
  choice->setObjectName("preferredGpuChoice");
  choice->addItem("Escolher GPU após consultar dispositivos");
  auto *refresh = new QPushButton("Consultar GPUs e áudio");
  layout->addWidget(refresh);
  layout->addWidget(choice);
  auto *perApp = new QPushButton("Usar GPU selecionada nos aplicativos iniciados pelo painel");
  layout->addWidget(perApp);
  auto *session = new QPushButton("Preparar GPU principal da próxima sessão KDE Wayland");
  layout->addWidget(session);
  auto *undoGpu = new QPushButton("Desfazer preferência da sessão");
  layout->addWidget(undoGpu);
  auto *audio = new QPushButton("Alinhar saídas de áudio"),
       *undoAudio = new QPushButton("Restaurar destinos de áudio anteriores");
  layout->addWidget(audio);
  layout->addWidget(undoAudio);
  auto *state = new QLabel("As preferências não desativam GPUs. Mudanças na sessão exigem novo "
                           "login. O áudio mantém filtros, volumes e conexões atuais; codecs "
                           "ausentes no ALSA exigem diagnóstico antes do roteamento.");
  state->setWordWrap(true);
  state->setTextFormat(Qt::PlainText);
  layout->addWidget(state);
  auto populate = [this, choice, state] {
    choice->clear();
    choice->addItem("Automático · manter política do sistema", QString{});
    QString preferred = QSettings().value("graphics/preferredPci").toString();
    for (auto g : gpus)
      choice->addItem(g.name + " · " + g.pci, g.pci);
    int index = choice->findData(preferred);
    if (index >= 0)
      choice->setCurrentIndex(index);
    auto inv = audioInventory();
    if (!inv["valid"].toBool())
      state->setText(inv["error"].toString());
    else
      state->setText(QString("Saída atual: %1\nEntrada atual: %2\n%3\nPlacas ALSA:\n%4")
                         .arg(inv["output"].toString(), inv["input"].toString(),
                              inv["notice"].toString(), inv["alsaCards"].toString()));
  };
  connect(refresh, &QPushButton::clicked, this, [this, populate] {
    if (!offline && !testActive && !driverBusy)
      populate();
  });
  connect(perApp, &QPushButton::clicked, this, [this, choice, state] {
    if (offline || testActive || driverBusy)
      return;
    if (choice->currentData().toString().isEmpty()) {
      QSettings().remove("graphics/preferredPci");
      state->setText(
          "Preferência por aplicativo removida; novos lançamentos seguem a política do sistema.");
      return;
    }
    QSettings().setValue("graphics/preferredPci", choice->currentData());
    for (int n = 0; n < gpus.size(); ++n)
      if (gpus[n].pci == choice->currentData().toString())
        gpuSelect->setCurrentIndex(n);
    state->setText("Preferência salva para novos aplicativos lançados pelo painel. Programas já "
                   "abertos não mudam de GPU. Benchmarks mantêm seleção explícita do dispositivo.");
  });
  connect(session, &QPushButton::clicked, this, [this, choice, state] {
    if (offline || testActive || driverBusy || displayBusy)
      return;
    auto plan = gpuSessionPlan(gpus, choice->currentData().toString());
    if (!plan["valid"].toBool()) {
      state->setText(plan["error"].toString());
      return;
    }
    if (QMessageBox::question(
            this, "Preferência de GPU da sessão",
            plan["message"].toString() + "\n\nArquivo: " + plan["target"].toString() + "\n\n" +
                plan["content"].toString() + "\nSalvar com backup da preferência anterior?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
      return;
    auto result = applyGpuSession(plan, configRoot() + "/system-backups");
    state->setText(result[result["valid"].toBool() ? "message" : "error"].toString());
  });
  connect(undoGpu, &QPushButton::clicked, this, [this, state] {
    if (offline || testActive || driverBusy)
      return;
    if (QMessageBox::question(
            this, "Restaurar preferência",
            "Restaurar a preferência de sessão salva antes da alteração? Exige novo login.") !=
        QMessageBox::Yes)
      return;
    auto r = restoreGpuSession(configRoot() + "/system-backups");
    state->setText(r[r["valid"].toBool() ? "message" : "error"].toString());
  });
  connect(audio, &QPushButton::clicked, this, [this, state] {
    if (offline || testActive || driverBusy || displayBusy)
      return;
    auto inv = audioInventory();
    if (!inv["valid"].toBool()) {
      state->setText(inv["error"].toString());
      return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Alinhar saídas e entradas de áudio");
    dialog.resize(690, 350);
    auto *l = new QVBoxLayout(&dialog);
    auto *notice = new QLabel(inv["notice"].toString());
    notice->setWordWrap(true);
    l->addWidget(notice);
    auto *outputs = new QComboBox, *inputs = new QComboBox;
    outputs->addItem("Manter saída atual", QString{});
    inputs->addItem("Manter entrada atual", QString{});
    for (auto pair : QList<QPair<QString, QComboBox *>>{{"outputs", outputs}, {"inputs", inputs}})
      for (auto v : inv[pair.first].toArray()) {
        auto n = v.toObject();
        pair.second->addItem(n["description"].toString() + " · " + n["name"].toString() +
                                 (n["virtual"].toBool() ? " [virtual]" : ""),
                             n["name"].toString());
      }
    auto select = [&](QComboBox *c, QString key, QString current) {
      QString preferred = QSettings().value(key, current).toString();
      int n = c->findData(preferred);
      if (n >= 0)
        c->setCurrentIndex(n);
    };
    select(outputs, "audio/preferredOutput", inv["output"].toString());
    select(inputs, "audio/preferredInput", inv["input"].toString());
    auto *form = new QFormLayout;
    form->addRow("Saída padrão", outputs);
    form->addRow("Entrada padrão", inputs);
    l->addLayout(form);
    bool physical = false;
    for (auto v : inv["inputs"].toArray())
      if (!v.toObject()["virtual"].toBool())
        physical = true;
    if (!physical) {
      auto *warning = new QLabel(
          "Nenhuma entrada física foi localizada. Uma entrada virtual não comprova microfone "
          "funcional. O alinhamento não corrige falha de inicialização do codec.");
      warning->setWordWrap(true);
      l->addWidget(warning);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Salvar destinos e alinhar");
    l->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
      return;
    auto out = outputs->currentData().toString(), in = inputs->currentData().toString();
    auto r = alignAudio(out, in, configRoot() + "/system-backups");
    if (r["valid"].toBool()) {
      if (!out.isEmpty())
        QSettings().setValue("audio/preferredOutput", out);
      if (!in.isEmpty())
        QSettings().setValue("audio/preferredInput", in);
    }
    state->setText(r[r["valid"].toBool() ? "message" : "error"].toString());
  });
  connect(undoAudio, &QPushButton::clicked, this, [this, state] {
    if (offline || testActive || driverBusy)
      return;
    if (QMessageBox::question(this, "Restaurar destinos de áudio",
                              "Voltar aos destinos padrão anteriores? Dispositivos precisam estar "
                              "disponíveis; conexões já abertas são preservadas.") !=
        QMessageBox::Yes)
      return;
    auto r = restoreAudio(configRoot() + "/system-backups");
    if (r["valid"].toBool()) {
      auto inv = audioInventory();
      QSettings().setValue("audio/preferredOutput", inv["output"].toString());
      QSettings().setValue("audio/preferredInput", inv["input"].toString());
    }
    state->setText(r[r["valid"].toBool() ? "message" : "error"].toString());
  });
  if (offline)
    for (auto *b : {refresh, perApp, session, undoGpu, audio, undoAudio})
      b->setEnabled(false);
  return box;
}
