#include "window.h"
#include "update_manager.h"
#include "driver_packages.h"
QWidget *MainWindow::updatesPage() {
  auto *root = page("GPU-PRO · Atualizações e backup");
  auto *layout = qobject_cast<QVBoxLayout *>(root->layout());
  const bool debInstall = qEnvironmentVariable("APPIMAGE").isEmpty() && QCoreApplication::applicationDirPath().startsWith("/usr/lib/gpu-pro/");
  bool managed = !qEnvironmentVariable("APPIMAGE").isEmpty() || QCoreApplication::applicationDirPath().startsWith("/usr/lib/gpu-pro/");
  auto *info = new QLabel("Versão instalada: " + QCoreApplication::applicationVersion() +
      "\nCanal estável: eritongrnascimento-svg/GPU-PRO · GitHub Releases\n"
      "A consulta só acontece ao clicar. Nada é enviado sobre sua GPU ou seus testes.");
  info->setWordWrap(true); layout->addWidget(info);
  auto *state = new QLabel("Pronto para verificar atualizações oficiais");
  state->setWordWrap(true); state->setTextFormat(Qt::PlainText); layout->addWidget(state);
  auto *row = new QHBoxLayout;
  auto *check = new QPushButton("Verificar atualizações"); row->addWidget(check);
  auto *install = new QPushButton("Baixar, salvar backup e atualizar"); row->addWidget(install);
  install->setEnabled(false);
  if (managed) install->setText("Baixar, salvar backup e instalar atualização");
  layout->addLayout(row);
  auto *backup = new QPushButton("Salvar backup da versão atual"); layout->addWidget(backup);
  auto *restore = new QPushButton("Restaurar último backup"); layout->addWidget(restore);
  auto *downloadRow = new QHBoxLayout;
  auto *releases = new QPushButton("Baixar no GitHub"); downloadRow->addWidget(releases);
  auto *previousReleases = new QPushButton("Versões anteriores");
  previousReleases->setObjectName("previousReleasesButton");
  previousReleases->setToolTip("Lista os pacotes antigos por versão, com links diretos para download.");
  downloadRow->addWidget(previousReleases); layout->addLayout(downloadRow);
  auto *restart = new QPushButton("Reiniciar GPU-PRO"); layout->addWidget(restart);
  restart->setEnabled(false);
  auto *details = new QLabel(
      "O pacote é verificado por SHA-256 publicado na mesma release, extraído em uma área temporária "
      "e instalado somente após concluir o backup. Em caso de falha de cópia, tenta restaurar a versão anterior. "
      "O checksum verifica integridade; não é uma assinatura independente do GitHub.\n\n"
      "Atualização pelo painel: pacote portátil x86_64 em diretório gravável pelo usuário. "
      "Instalações por gerenciador de pacotes ou compiladas em build/ devem ser atualizadas pelo mesmo método. "
      "Dados de testes ficam preservados no diretório de configuração. O helper root não é atualizado automaticamente.");
  if (managed) details->setText("AppImage: download verificado, backup integral e troca no mesmo caminho gravável pelo usuário. DEB: preserva o pacote oficial anterior, baixa a atualização e mostra a simulação APT antes de solicitar autenticação no gerenciador nativo. A opção de abrir o GitHub continua disponível. Dados de testes permanecem no diretório de configuração; reinicie após atualizar.");
  if (debInstall) {
    install->hide(); backup->hide(); restore->hide();
    details->setText("Versão DEB: use Baixar no GitHub para obter o pacote oficial e instale-o pelo gerenciador de pacotes do sistema. O download interno de DEB está desativado. Seus resultados e configurações permanecem preservados.");
  }
  details->setWordWrap(true); layout->addWidget(details);
  layout->addWidget(driversControls());
  layout->addWidget(systemControls());
  auto *licenses = new QPushButton("Licenças e componentes de terceiros"); layout->addWidget(licenses);
  connect(licenses, &QPushButton::clicked, this, [this] {
    QDialog dialog(this); dialog.setWindowTitle("GPU-PRO · Licenças"); dialog.resize(850, 640);
    auto *body = new QVBoxLayout(&dialog);
    auto *notice = new QLabel("GPU-PRO utiliza Qt sob LGPLv3 e outras bibliotecas de terceiros. "
        "As restrições do código original não limitam os direitos dessas bibliotecas.");
    notice->setWordWrap(true); body->addWidget(notice);
    auto *tabs = new QTabWidget; body->addWidget(tabs, 1);
    for (auto item : QList<QPair<QString, QString>>{{"GPU-PRO", ":/LICENSE"}, {"Avisos", ":/NOTICE"},
        {"LGPLv3", ":/docs/licenses/LGPL-3.0.txt"}, {"GPLv3", ":/docs/licenses/GPL-3.0.txt"},
        {"LGPLv2.1", ":/docs/licenses/LGPL-2.1.txt"}}) {
      auto *text = new QPlainTextEdit; text->setReadOnly(true);
      QFile file(item.second);
      text->setPlainText(file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : "Texto não encontrado no pacote");
      tabs->addTab(text, item.first);
    }
    auto *aboutQt = new QPushButton("Sobre Qt"); body->addWidget(aboutQt);
    connect(aboutQt, &QPushButton::clicked, &dialog, [&dialog] { QMessageBox::aboutQt(&dialog); });
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close); body->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
  });
  auto *behavior = new QGroupBox("Comportamento do aplicativo");
  auto *behaviorLayout = new QVBoxLayout(behavior);
  keepInTray = new QCheckBox("Manter aberto na bandeja ao fechar o painel");
  keepInTray->setChecked(!offline && QSettings().value("ui/keepInTray",false).toBool());
  keepInTray->setEnabled(!offline && QSystemTrayIcon::isSystemTrayAvailable());
  behaviorLayout->addWidget(keepInTray);
  auto *neonMotion=new QCheckBox("Animar luzes neon dos cards (desative para gravações, se necessário)");
  neonMotion->setChecked(offline || QSettings().value("ui/neonMotion",true).toBool());
  qApp->setProperty("gpuProNeonMotion",neonMotion->isChecked());
  behaviorLayout->addWidget(neonMotion);
  connect(neonMotion,&QCheckBox::toggled,this,[this](bool checked){
    qApp->setProperty("gpuProNeonMotion",checked);
    if(!offline)QSettings().setValue("ui/neonMotion",checked);
    for(auto *card:findChildren<NeonCard *>())card->update();
  });
  auto *trayNote = new QLabel(QSystemTrayIcon::isSystemTrayAvailable() ? "Clique no ícone da bandeja para reabrir; use Sair para encerrar." : "O ambiente atual não oferece bandeja do sistema; fechar continuará encerrando o painel.");
  trayNote->setWordWrap(true); behaviorLayout->addWidget(trayNote);
  connect(keepInTray,&QCheckBox::toggled,this,[this](bool checked){if(!offline)QSettings().setValue("ui/keepInTray",checked);});
  layout->addWidget(behavior);
  layout->addStretch();
  auto *manager = new UpdateManager(QFileInfo(QCoreApplication::applicationDirPath()).dir().absolutePath(), configRoot() + "/updates", this);
  if(!qEnvironmentVariable("APPIMAGE").isEmpty())manager->setFormat("appimage",qEnvironmentVariable("APPIMAGE"));
  else if(managed&&nativePackageManager()=="apt")manager->setFormat("deb");
  auto allowed = [this, manager, managed] {
    if (offline || driverBusy || testActive || codecSuite.busy() || clockProcess.state() != QProcess::NotRunning || displayBusy || manager->busy()) {
      notify("Aguarde as operações em andamento antes de atualizar"); return false;
    }
    if(managed&&manager->format()=="portable"){notify("Este pacote foi instalado por outro gerenciador; utilize o método nativo ou o AppImage oficial");return false;}
    if (manager->format()=="portable" && QFileInfo(QCoreApplication::applicationDirPath()).fileName() != "bin") {
      notify("Esta execução usa build/; recompile com build.sh para atualizar"); return false;
    }
    return true;
  };
  manager->mayInstall = [this] { return !driverBusy && !testActive && !codecSuite.busy() && clockProcess.state() == QProcess::NotRunning && !displayBusy; };
  connect(check, &QPushButton::clicked, this, [this, manager] {
    if (!offline && !testActive) manager->check();
  });
  connect(manager, &UpdateManager::message, this, [state](QString message) { state->setText(message); });
  connect(manager, &UpdateManager::checked, this, [install, this, debInstall](QString, bool available) { install->setEnabled(available && !offline && !debInstall); });
  connect(install, &QPushButton::clicked, this, [this, manager, allowed, install, debInstall] {
    if (debInstall) return;
    if (!allowed()) return;
    if (QMessageBox::question(this, "Atualizar GPU-PRO", "Baixar a release oficial e preservar a versão atual? AppImage será substituído no mesmo caminho; instalações portáteis serão atualizadas após o backup.") != QMessageBox::Yes) return;
    install->setEnabled(false); manager->downloadAndInstall();
  });
  connect(manager, &UpdateManager::finished, this, [restart](bool installed) { restart->setEnabled(installed); });
  connect(backup, &QPushButton::clicked, this, [this, manager, allowed, state] {
    if (!allowed()) return;
    if(manager->format()!="portable"){manager->backupCurrent();return;}
    QString error; auto folder = manager->backup(error);
    state->setText(folder.isEmpty() ? error : "Backup salvo: " + folder);
  });
  connect(restore, &QPushButton::clicked, this, [this, manager, allowed, state, restart] {
    if (!allowed()) return;
    if (QMessageBox::question(this, "Restaurar backup", "Restaurar os arquivos da versão anterior nesta instalação? Os resultados dos testes serão preservados.") != QMessageBox::Yes) return;
    if(manager->format()!="portable"){manager->restoreManaged();return;}
    QString error; bool ok = manager->restore({}, error);
    state->setText(ok ? "Backup restaurado. Reinicie o GPU-PRO." : error); restart->setEnabled(ok);
  });
  connect(releases, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("https://github.com/eritongrnascimento-svg/GPU-PRO/releases/latest")); });
  connect(previousReleases, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("https://github.com/eritongrnascimento-svg/GPU-PRO/blob/main/docs/VERSOES-ANTERIORES.md")); });
  connect(manager,&UpdateManager::managedPackageReady,this,[this,manager,state,restart](QString package,QString version,bool restoring){
    if(offline||driverBusy||testActive||nativePackageManager()!="apt")return;
    auto *simulation=new QProcess(this);simulation->setProcessEnvironment(nativeEnvironment());
    auto *timer=new QTimer(simulation);timer->setSingleShot(true);timer->setInterval(30000);connect(timer,&QTimer::timeout,simulation,&QProcess::kill);
    driverBusy=true;setTesting(true);
    connect(simulation,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,simulation,timer,package,version,restoring,state,restart](int code,QProcess::ExitStatus exit){
      timer->stop();driverBusy=false;setTesting(false);QString text=QString::fromUtf8(simulation->readAllStandardOutput())+QString::fromUtf8(simulation->readAllStandardError());simulation->deleteLater();
      if(code!=0||exit!=QProcess::NormalExit){state->setText("Simulação APT recusada; nada instalado.\n"+text);return;}
      QMessageBox confirm(QMessageBox::Question,restoring?"Restaurar GPU-PRO anterior":"Instalar atualização GPU-PRO","Pacote verificado: GPU-PRO "+version+"\nA versão anterior foi preservada. Confira os pacotes adicionados/removidos nos detalhes. Abrir autenticação do sistema e confirmar no APT?",QMessageBox::Yes|QMessageBox::No,this);confirm.setDetailedText(text);confirm.setDefaultButton(QMessageBox::No);
      if(confirm.exec()!=QMessageBox::Yes)return;
      QStringList command{"/usr/bin/apt-get","install"};if(restoring)command<<"--allow-downgrades";command<<package;
      startNativeTransaction(command,[this,state,restart,version](int result){
        QProcess query;query.setProcessEnvironment(nativeEnvironment());query.start("/usr/bin/dpkg-query",{"-W","-f=${Version}","gpu-pro"});query.waitForFinished(5000);
        bool ok=result==0&&QString::fromUtf8(query.readAllStandardOutput()).trimmed()==version;
        state->setText(ok?"Pacote GPU-PRO "+version+" confirmado no sistema. Reinicie o painel.":"Terminal encerrado; versão esperada não confirmada. Confira o resultado do APT.");restart->setEnabled(ok);
      });
    });
    connect(simulation,&QProcess::errorOccurred,this,[this,simulation,timer,state](QProcess::ProcessError error){if(error==QProcess::FailedToStart){timer->stop();driverBusy=false;setTesting(false);state->setText(simulation->errorString());simulation->deleteLater();}});
    QStringList args{"--simulate","install"};if(restoring)args<<"--allow-downgrades";args<<package;simulation->start("/usr/bin/apt-get",args);timer->start();
  });
  connect(restart, &QPushButton::clicked, this, [this, allowed] {
    if (!allowed()) return;
    QString executable=qEnvironmentVariable("APPIMAGE");if(executable.isEmpty())executable=QCoreApplication::applicationFilePath();
    explicitQuit=true;bool accepted=close();explicitQuit=false;
    if(accepted){QProcess::startDetached(executable,{});QCoreApplication::quit();}
  });

  if (offline) for (auto *button : {check, install, backup, restore, releases, previousReleases}) button->setEnabled(false);
  return root;
}
