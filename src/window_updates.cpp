#include "window.h"
#include "update_manager.h"
QWidget *MainWindow::updatesPage() {
  auto *root = page("GPU-PRO · Atualizações e backup");
  auto *layout = qobject_cast<QVBoxLayout *>(root->layout());
  auto *info = new QLabel("Versão instalada: " + QCoreApplication::applicationVersion() +
      "\nCanal estável: eritongrnascimento-svg/GPU-PRO · GitHub Releases\n"
      "A consulta só acontece ao clicar. Nada é enviado sobre sua GPU ou seus testes.");
  info->setWordWrap(true); layout->addWidget(info);
  auto *state = new QLabel("Pronto para verificar atualizações oficiais");
  state->setWordWrap(true); state->setTextFormat(Qt::PlainText); layout->addWidget(state);
  auto *row = new QHBoxLayout;
  auto *check = new QPushButton("Verificar atualizações"); row->addWidget(check);
  auto *install = new QPushButton("Baixar, salvar backup e atualizar"); row->addWidget(install);
  install->setEnabled(false); layout->addLayout(row);
  auto *backup = new QPushButton("Salvar backup da versão atual"); layout->addWidget(backup);
  auto *restore = new QPushButton("Restaurar último backup"); layout->addWidget(restore);
  auto *releases = new QPushButton("Abrir notas e releases oficiais"); layout->addWidget(releases);
  auto *restart = new QPushButton("Reiniciar GPU-PRO"); layout->addWidget(restart);
  restart->setEnabled(false);
  auto *details = new QLabel(
      "O pacote é verificado por SHA-256 publicado na mesma release, extraído em uma área temporária "
      "e instalado somente após concluir o backup. Em caso de falha de cópia, tenta restaurar a versão anterior. "
      "O checksum verifica integridade; não é uma assinatura independente do GitHub.\n\n"
      "Atualização pelo painel: pacote portátil x86_64 em diretório gravável pelo usuário. "
      "Instalações por gerenciador de pacotes ou compiladas em build/ devem ser atualizadas pelo mesmo método. "
      "Dados de testes ficam preservados no diretório de configuração. O helper root não é atualizado automaticamente.");
  details->setWordWrap(true); layout->addWidget(details); layout->addStretch();
  auto *manager = new UpdateManager(QFileInfo(QCoreApplication::applicationDirPath()).dir().absolutePath(), configRoot() + "/updates", this);
  auto allowed = [this, manager] {
    if (offline || testActive || codecSuite.busy() || clockProcess.state() != QProcess::NotRunning || displayBusy || manager->busy()) {
      notify("Aguarde as operações em andamento antes de atualizar"); return false;
    }
    if (QFileInfo(QCoreApplication::applicationDirPath()).fileName() != "bin") {
      notify("Esta execução usa build/; recompile com build.sh para atualizar"); return false;
    }
    return true;
  };
  manager->mayInstall = [this] { return !testActive && !codecSuite.busy() && clockProcess.state() == QProcess::NotRunning && !displayBusy; };
  connect(check, &QPushButton::clicked, this, [this, manager] {
    if (!offline && !testActive) manager->check();
  });
  connect(manager, &UpdateManager::message, this, [state](QString message) { state->setText(message); });
  connect(manager, &UpdateManager::checked, this, [install, this](QString, bool available) { install->setEnabled(available && !offline); });
  connect(install, &QPushButton::clicked, this, [this, manager, allowed, install] {
    if (!allowed()) return;
    if (QMessageBox::question(this, "Atualizar GPU-PRO", "Baixar a release oficial, salvar backup da instalação atual e substituir os arquivos do aplicativo?") != QMessageBox::Yes) return;
    install->setEnabled(false); manager->downloadAndInstall();
  });
  connect(manager, &UpdateManager::finished, this, [restart](bool installed) { restart->setEnabled(installed); });
  connect(backup, &QPushButton::clicked, this, [this, manager, allowed, state] {
    if (!allowed()) return;
    QString error; auto folder = manager->backup(error);
    state->setText(folder.isEmpty() ? error : "Backup salvo: " + folder);
  });
  connect(restore, &QPushButton::clicked, this, [this, manager, allowed, state, restart] {
    if (!allowed()) return;
    if (QMessageBox::question(this, "Restaurar backup", "Restaurar os arquivos da versão anterior nesta instalação? Os resultados dos testes serão preservados.") != QMessageBox::Yes) return;
    QString error; bool ok = manager->restore({}, error);
    state->setText(ok ? "Backup restaurado. Reinicie o GPU-PRO." : error); restart->setEnabled(ok);
  });
  connect(releases, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("https://github.com/eritongrnascimento-svg/GPU-PRO/releases")); });
  connect(restart, &QPushButton::clicked, this, [this, allowed] {
    if (!allowed()) return;
    if (QProcess::startDetached(QCoreApplication::applicationFilePath(), {})) close();
    else notify("Não foi possível reiniciar; feche e abra pelo run.sh");
  });
  if (offline) for (auto *button : {check, install, backup, restore, releases}) button->setEnabled(false);
  return root;
}
