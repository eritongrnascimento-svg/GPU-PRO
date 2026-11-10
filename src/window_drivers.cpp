#include "window.h"
#include "driver_packages.h"
QWidget *MainWindow::driversControls() {
  auto *box = new QGroupBox("Drivers · repositórios da distribuição");
  auto *layout = new QVBoxLayout(box);
  auto *info = new QLabel("Consulte as versões disponíveis nos repositórios já configurados. Nenhum driver é instalado automaticamente.");
  info->setWordWrap(true); info->setTextFormat(Qt::PlainText); layout->addWidget(info);
  auto *refresh = new QPushButton("Consultar drivers e versões disponíveis"); layout->addWidget(refresh);
  auto *form = new QFormLayout;
  auto *packages = new QComboBox, *versions = new QComboBox;
  form->addRow("Pacote / componente",packages); form->addRow("Versão / repositório",versions); layout->addLayout(form);
  auto *actions = new QHBoxLayout;
  auto *preview = new QPushButton("Simular e revisar transação");actions->addWidget(preview);
  auto *install = new QPushButton("Instalar / atualizar versão selecionada");actions->addWidget(install);
  layout->addLayout(actions); install->setEnabled(false); preview->setEnabled(false);
  auto *details = new QLabel("AMD/Intel: Mesa, Vulkan e VA-API são componentes de espaço de usuário; o driver do kernel acompanha o kernel da distro. "
      "NVIDIA: mantenha a família compatível com sua GPU e kernel. A troca de módulo pode exigir headers/DKMS e reinicialização. "
      "No Arch/CachyOS a instalação usa atualização completa do sistema, evitando atualização parcial. Não há backup/rollback de drivers pelo GPU-PRO.");
  details->setWordWrap(true);layout->addWidget(details);
  auto *output = new QPlainTextEdit;output->setReadOnly(true);output->setMaximumHeight(180);output->setMinimumHeight(110);
  output->document()->setMaximumBlockCount(1500);layout->addWidget(output);
  auto *catalog = new DriverCatalog(this);
  auto *simulation = new QProcess(this);
  auto *timeout = new QTimer(simulation);timeout->setSingleShot(true);timeout->setInterval(30000);
  connect(timeout,&QTimer::timeout,simulation,&QProcess::kill);
  auto rows = std::make_shared<QJsonArray>();auto verified=std::make_shared<QString>();auto planText=std::make_shared<QString>();
  auto selectedKey = [versions] {return QString::fromUtf8(QJsonDocument(versions->currentData().toJsonObject()).toJson(QJsonDocument::Compact));};
  connect(packages,qOverload<int>(&QComboBox::currentIndexChanged),box,[rows,packages,versions,install,preview,verified] {
    versions->clear();verified->clear();install->setEnabled(false);
    for(auto value:*rows){auto row=value.toObject();if(row["name"].toString()!=packages->currentText())continue;
      QString label=row["version"].toString()+" · "+row["repo"].toString();
      if(!row["installed"].toString().isEmpty())label+=" · instalado: "+row["installed"].toString();
      versions->addItem(label,row);
    }
    preview->setEnabled(versions->count()>0);
  });
  connect(versions,qOverload<int>(&QComboBox::currentIndexChanged),box,[verified,install,output,versions] {
    verified->clear();install->setEnabled(false);
    auto command=driverTransaction(versions->currentData().toJsonObject(),false);
    output->setPlainText(command.isEmpty()?"Selecione um pacote válido":"Comando proposto (após autenticação):\n"+command.join(' ')+"\n\nSimule a transação antes de instalar. Os índices podem mudar; o gerenciador nativo fará a confirmação final.");
  });
  connect(refresh,&QPushButton::clicked,this,[this,catalog,info,refresh,install,preview,verified] {
    if(offline||testActive||driverBusy||catalog->busy())return;
    verified->clear();install->setEnabled(false);preview->setEnabled(false);refresh->setEnabled(false);
    info->setText("Consultando índices do gerenciador nativo…");catalog->refresh(gpus.value(selected).vendor);
  });
  connect(catalog,&DriverCatalog::listed,box,[this,rows,packages,versions,info,refresh,install](QJsonArray result,QString message) {
    *rows=result;packages->clear();versions->clear();QSet<QString> names;
    for(auto value:result){QString name=value.toObject()["name"].toString();if(!names.contains(name)){names.insert(name);packages->addItem(name);}}
    info->setText(message);refresh->setEnabled(!offline);install->setEnabled(false);
  });
  connect(simulation,&QProcess::readyReadStandardOutput,box,[simulation,output,planText] {QString text=QString::fromUtf8(simulation->readAllStandardOutput());output->moveCursor(QTextCursor::End);output->insertPlainText(text);*planText=(*planText+text).right(250000);});
  connect(simulation,&QProcess::readyReadStandardError,box,[simulation,output,planText] {QString text=QString::fromUtf8(simulation->readAllStandardError());output->moveCursor(QTextCursor::End);output->insertPlainText(text);*planText=(*planText+text).right(250000);});
  auto simulationKey=std::make_shared<QString>();
  connect(preview,&QPushButton::clicked,this,[this,simulation,timeout,versions,output,verified,simulationKey,selectedKey,planText,install] {
    if(offline||testActive||driverBusy||simulation->state()!=QProcess::NotRunning)return;
    auto command=driverTransaction(versions->currentData().toJsonObject(),true);if(command.isEmpty())return;
    *simulationKey=selectedKey();verified->clear();planText->clear();install->setEnabled(false);output->setPlainText("Simulação sem instalação: "+command.join(' ')+"\n");
    auto env = nativeEnvironment(); env.insert("LC_ALL","C"); simulation->setProcessEnvironment(env);QString program=command.takeFirst();simulation->start(program,command);timeout->start();
  });
  connect(simulation,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,simulationKey,selectedKey,verified,planText,install,timeout,output](int code,QProcess::ExitStatus state) {
    timeout->stop();bool dnfAbort=code==1&&planText->contains("Operation aborted",Qt::CaseInsensitive);
    bool ok=state==QProcess::NormalExit&&(code==0||dnfAbort)&&*simulationKey==selectedKey();
    *verified=ok?*simulationKey:QString{};install->setEnabled(ok&&!offline&&!testActive);
    output->appendPlainText(ok?"\nSimulação concluída. Revise os pacotes e confirme no gerenciador ao instalar.":"\nSimulação não validada. Confira erros, índices e compatibilidade; nada foi instalado.");
  });
  connect(simulation,&QProcess::errorOccurred,box,[output,timeout](QProcess::ProcessError error){if(error==QProcess::FailedToStart){timeout->stop();output->appendPlainText("Não foi possível iniciar o gerenciador nativo");}});
  connect(install,&QPushButton::clicked,this,[this,versions,verified,selectedKey,output,install] {
    if(offline||testActive||driverBusy||*verified!=selectedKey()||clockProcess.state()!=QProcess::NotRunning)return;
    auto row=versions->currentData().toJsonObject();auto command=driverTransaction(row,false);if(command.isEmpty())return;
    if(QMessageBox::question(this,"Confirmar instalação de driver",QString("Instalar/atualizar %1 na versão %2?\n\n%3\n\n%4\n\nO terminal mostrará a transação final. Confirme também no gerenciador. Pode ser necessário reiniciar.")
       .arg(row["name"].toString(),row["version"].toString(),command.join(' '),row["manager"].toString()=="pacman"?"Arch/CachyOS: atualizará também os demais pacotes do sistema.":"Somente repositórios já configurados serão usados."))!=QMessageBox::Yes)return;
    QString terminal;QStringList args;
    if(QFileInfo("/usr/bin/konsole").isExecutable()){terminal="/usr/bin/konsole";args={"--separate","--nofork","-e"};}
    else if(QFileInfo("/usr/bin/xterm").isExecutable()){terminal="/usr/bin/xterm";args={"-e"};}
    else if(QFileInfo("/usr/bin/gnome-terminal").isExecutable()){terminal="/usr/bin/gnome-terminal";args={"--wait","--"};}
    else if(QFileInfo("/usr/bin/xfce4-terminal").isExecutable()){terminal="/usr/bin/xfce4-terminal";args={"--disable-server","-x"};}
    if(terminal.isEmpty()||!QFileInfo("/usr/bin/pkexec").isExecutable()){notify("Instale um terminal compatível e pkexec para executar pelo painel",command.join(' '));return;}
    args<<"/usr/bin/pkexec"<<"--disable-internal-agent";args<<command;
    driverBusy=true;activeKind="driver-maintenance";setTesting(true);install->setEnabled(false);
    driverTerminal.setProcessEnvironment(nativeEnvironment());driverTerminal.start(terminal,args);
    output->appendPlainText("\nGerenciador aberto no terminal. A autenticação ocorre no diálogo do sistema; nunca digite senha no GPU-PRO.");
  });
  connect(&driverTerminal,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,catalog,output,verified](int code,QProcess::ExitStatus) {
    driverBusy=false;setTesting(false);verified->clear();output->appendPlainText(QString("Terminal encerrado (código %1). Confira o resultado do gerenciador; o driver carregado pode mudar apenas após reiniciar.").arg(code));
    catalog->refresh(gpus.value(selected).vendor);
  });
  connect(&driverTerminal,&QProcess::errorOccurred,this,[this,output](QProcess::ProcessError error){if(error==QProcess::FailedToStart){driverBusy=false;setTesting(false);output->appendPlainText("Falha ao abrir terminal: "+driverTerminal.errorString());}});
  if(offline)refresh->setEnabled(false);
  return box;
}
