#include "window.h"
void MainWindow::buildTuningControls(QVBoxLayout *layout) {
  auto *group=new QGroupBox("Potência, tensão e ventoinhas");auto *form=new QVBoxLayout(group);
  auto *explain=new QLabel("O limite de potência define o teto de consumo em W. Undervolt reduz a tensão por um ajuste negativo em mV. Os controles disponíveis variam conforme a placa e o driver. Nada é aplicado ao abrir.");
  explain->setWordWrap(true);form->addWidget(explain);
  for(QString domain:QStringList{"power","voltageOffset","fan0","fan1","fan2","fan3","fan4","fan5","fan6","fan7","fan8","fan9","fan10","fan11","fan12","fan13","fan14","fan15"}) {
    QString title=domain=="power"?"Power limit / limite de consumo":domain=="voltageOffset"?"Tensão / undervolt (offset)":"Ventoinha "+QString::number(domain.mid(3).toInt()+1);
    auto *label=new QLabel(title+" · não exposto pelo driver");label->setWordWrap(true);tuningLabels[domain]=label;
    form->addWidget(label);auto *row=new QHBoxLayout;
    auto *input=new QSpinBox;input->setRange(0,0);input->setSpecialValueText("N/D");input->setEnabled(false);input->setObjectName("tuning-"+domain);tuningInputs[domain]=input;row->addWidget(input,1);
    auto *apply=new QPushButton("Aplicar");apply->setEnabled(false);tuningButtons[domain]=apply;row->addWidget(apply);form->addLayout(row);
    connect(apply,&QPushButton::clicked,this,[this,domain,input]{input->interpretText();applyTuning(domain,input->value());});
    // Extra physical fans are shown only when discovered.
    if(domain.startsWith("fan")&&domain!="fan0") {label->hide();input->hide();apply->hide();}
  }
  automaticFans=new QPushButton("Restaurar controle automático das ventoinhas");automaticFans->setEnabled(false);form->addWidget(automaticFans);
  connect(automaticFans,&QPushButton::clicked,this,[this]{applyTuning("fans-auto",-1);});
  auto *restore=restoreTuningButton=new QPushButton("Restaurar potência e tensão iniciais desta sessão");restore->setEnabled(false);form->addWidget(restore);
  connect(restore,&QPushButton::clicked,this,[this]{
    const auto initial=sessionTuning.value(deviceKey(gpus.value(selected)));
    if(initial.isEmpty())return;
    QMenu menu(this);
    for(QString domain:{"power","voltageOffset"})if(initial.contains(domain)) {
      auto *action=menu.addAction(domain=="power"?"Restaurar power limit inicial":"Restaurar offset de tensão inicial");
      connect(action,&QAction::triggered,this,[this,domain,initial]{applyTuning(domain,initial[domain].toObject()["current"].toInt());});
    }
    menu.exec(QCursor::pos());
  });
  layout->addWidget(group);
}
void MainWindow::refreshTuningControls(const QJsonObject &sample) {
  const auto caps=sample["tuning"].toObject();QString key=deviceKey(gpus.value(selected));
  auto initial=sessionTuning.value(key);
  for(auto it=caps.begin();it!=caps.end();++it)if(!initial.contains(it.key()))initial[it.key()]=it.value();
  if(!initial.isEmpty())sessionTuning[key]=initial;
  bool ready=!offline&&!testActive&&!driverBusy&&!displayBusy&&clockProcess.state()==QProcess::NotRunning&&rootHelperTrusted(installedClockHelper())&&sample["temp"].isDouble();
  restoreTuningButton->setEnabled(ready&&(initial.contains("power")||initial.contains("voltageOffset")));
  bool fans=false;
  for(auto it=tuningInputs.begin();it!=tuningInputs.end();++it) {
    QString domain=it.key();auto c=caps[domain].toObject();bool available=!c.isEmpty();auto *input=it.value();
    bool initialize=input->property("gpu").toString()!=key || !input->property("known").toBool();
    if(available) {
      input->setSpecialValueText(QString());
      input->setRange(c["min"].toInt(),c["max"].toInt());input->setSuffix(" "+c["unit"].toString());
      if(initialize)input->setValue(c["current"].toInt());
      input->setProperty("gpu",key);input->setProperty("known",true);
    } else {input->setProperty("known",false);input->setRange(0,0);input->setSpecialValueText("N/D");}
    input->setEnabled(ready&&available);tuningButtons[domain]->setEnabled(ready&&available);
    QString title=domain=="power"?"Power limit":domain=="voltageOffset"?"Tensão / undervolt":"Ventoinha "+QString::number(domain.mid(3).toInt()+1);
    tuningLabels[domain]->setText(title+(available?QString(" · atual %1 %2 · faixa %3–%4%5").arg(c["current"].toInt()).arg(c["unit"].toString()).arg(c["min"].toInt()).arg(c["max"].toInt()).arg(c["automatic"].toBool()?" · automática":""):" · não exposto pelo driver"));
    if(domain.startsWith("fan")) {fans|=available;bool visible=available||domain=="fan0";input->setVisible(visible);tuningButtons[domain]->setVisible(visible);tuningLabels[domain]->setVisible(visible);}
  }
  automaticFans->setEnabled(!offline&&!testActive&&!driverBusy&&!displayBusy&&clockProcess.state()==QProcess::NotRunning&&rootHelperTrusted(installedClockHelper())&&fans);
}
void MainWindow::applyTuning(QString domain,int value) {
  if(offline||testActive||driverBusy||displayBusy||clockProcess.state()!=QProcess::NotRunning||selected<0||selected>=gpus.size()||!rootHelperTrusted(installedClockHelper())||(domain!="fans-auto"&&!latest["temp"].isDouble())||!confirmOverclock())return;
  QString description=domain=="fans-auto"?"Restaurar o controle automático de todas as ventoinhas expostas?":QString("Aplicar %1: %2? O ajuste permanece até restauração ou reinicialização. Confirme a faixa e acompanhe a temperatura.").arg(domain).arg(value);
  if(QMessageBox::question(this,"Aplicar ajuste manual",description)!=QMessageBox::Yes)return;
  activeKind="manual-tuning";setTesting(true);clockOutput.clear();
  clockProcess.start("pkexec",{"--disable-internal-agent",installedClockHelper(),gpus[selected].pci,domain,QString::number(value)});
}
