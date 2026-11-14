#include "gpu.h"
#include <cmath>
namespace {
QJsonObject control(int lo, int hi, int current, QString unit) {
  return {{"min",lo},{"max",hi},{"current",current},{"unit",unit}};
}
QString amdHwmon(const Gpu &g) {
  if(g.vendor!="AMD" || g.driver!="amdgpu") return {};
  for(const auto &name:QDir(g.sys+"/hwmon").entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
    QString p=g.sys+"/hwmon/"+name+"/";
    if(readText(p+"name")=="amdgpu") return p;
  }
  return {};
}
bool put(QString p, QString text, QString &error) {
  QFile f(p); QByteArray b=(text+'\n').toLatin1();
  if(!f.open(QIODevice::WriteOnly)||f.write(b)!=b.size()||!f.flush()) {error=f.errorString();return false;}
  return true;
}
}
QJsonObject amdVoltageOffset(const QString &text) {
  // Only the documented independent offset interface, never legacy voltage tables.
  auto current=QRegularExpression("(?:^|\\n)OD_VDDGFX_OFFSET:\\s*\\n\\s*(-?\\d+)mV",QRegularExpression::CaseInsensitiveOption).match(text);
  auto range=QRegularExpression("(?:^|\\n)\\s*VDDGFX_OFFSET:\\s*(-?\\d+)mV\\s+(-?\\d+)mV",QRegularExpression::CaseInsensitiveOption).match(text);
  if(!current.hasMatch()||!range.hasMatch()) return {};
  int lo=range.captured(1).toInt(), hi=range.captured(2).toInt(), value=current.captured(1).toInt();
  if(lo < -2000 || hi>2000 || lo>hi || value<lo || value>hi) return {};
  return control(lo,hi,value,"mV");
}
QJsonObject gpuTuningCaps(const Gpu &g, Nvml &nv) {
  QJsonObject caps;
  if(g.specs["wslReadOnly"].toBool())return caps;
  if(g.vendor=="NVIDIA") {
    auto h=nv.handle(g.pci); if(!h)return caps;
    using H=void*; using U=unsigned;
    U lo=0,hi=0,current=0;
    auto bounds=nv.fn<int(*)(H,U*,U*)>("nvmlDeviceGetPowerManagementLimitConstraints");
    auto get=nv.fn<int(*)(H,U*)>("nvmlDeviceGetPowerManagementLimit");
    if(bounds&&get&&nv.fn<int(*)(H,U)>("nvmlDeviceSetPowerManagementLimit")&&bounds(h,&lo,&hi)==0&&get(h,&current)==0 && lo<=current&&current<=hi) {
      int low=int(std::ceil(lo/1000.)),high=int(hi/1000);
      if(low<=high)caps["power"]=control(low,high,int(current/1000),"W");
    }
    U count=0,minFan=0,maxFan=0;
    auto num=nv.fn<int(*)(H,U*)>("nvmlDeviceGetNumFans");
    auto range=nv.fn<int(*)(H,U*,U*)>("nvmlDeviceGetMinMaxFanSpeed");
    auto speed=nv.fn<int(*)(H,U,U*)>("nvmlDeviceGetFanSpeed_v2");
    auto policy=nv.fn<int(*)(H,U,U*)>("nvmlDeviceGetFanControlPolicy_v2");
    if(num&&range&&speed&&policy&&nv.fn<int(*)(H,U,U*)>("nvmlDeviceGetTargetFanSpeed")&&nv.fn<int(*)(H,U,U)>("nvmlDeviceSetFanSpeed_v2")&&nv.fn<int(*)(H,U)>("nvmlDeviceSetDefaultFanSpeed_v2")&&num(h,&count)==0&&range(h,&minFan,&maxFan)==0&&minFan<=maxFan&&maxFan<=100) {
      for(U i=0;i<qMin(count,16u);++i) {U value=0,mode=0;
        if(speed(h,i,&value)==0&&policy(h,i,&mode)==0&&mode<=1&&value<=100) {
          auto c=control(int(minFan),int(maxFan),int(value),"%");c["automatic"]=mode==0;
          caps["fan"+QString::number(i)]=c;
        }
      }
    }
  } else {
    const QString base=amdHwmon(g); if(base.isEmpty())return caps;
    auto lo=readNumber(base+"power1_cap_min"),hi=readNumber(base+"power1_cap_max"),now=readNumber(base+"power1_cap");
    if(lo&&hi&&now&&std::isfinite(*lo)&&std::isfinite(*hi)&&*lo>=0&&*hi<=2000000000.&&*lo<=*now&&*now<=*hi) {
      int low=int(std::ceil(*lo/1e6)),high=int(*hi/1e6);
      if(low<=high)caps["power"]=control(low,high,int(*now/1e6),"W");
    }
    auto pwm=readNumber(base+"pwm1"),mode=readNumber(base+"pwm1_enable");
    if(pwm&&mode&&*pwm>=0&&*pwm<=255&&(*mode==1||*mode==2)) {
      double low=readNumber(base+"pwm1_min").value_or(0),high=readNumber(base+"pwm1_max").value_or(255);
      if(std::isfinite(low)&&std::isfinite(high)&&low>=0&&low<=high&&high<=255) {
        auto c=control(int(std::ceil(low*100./255.)),int(std::floor(high*100./255.)),qRound(*pwm*100./255.),"%");
        if(c["min"].toInt()<=c["max"].toInt()){c["automatic"]=*mode==2;caps["fan0"]=c;}
      }
    }
    auto voltage=amdVoltageOffset(readText(g.sys+"/pp_od_clk_voltage"));
    if(!voltage.isEmpty()) caps["voltageOffset"]=voltage;
  }
  return caps;
}
bool setGpuTuning(const Gpu &g, Nvml &nv, QString domain, int value, QString &error) {
  auto all=gpuTuningCaps(g,nv);auto c=all[domain].toObject();
  bool automatic=domain.startsWith("fan")&&value==-1;
  if(c.isEmpty()||(!automatic&&(value<c["min"].toInt()||value>c["max"].toInt()))) {error="Controle indisponível ou valor fora da faixa consultada no driver";return false;}
  if(g.vendor=="NVIDIA") {
    using H=void*;using U=unsigned;H h=nv.handle(g.pci);
    int rc=-1;
    if(domain=="power") {
      auto set=nv.fn<int(*)(H,U)>("nvmlDeviceSetPowerManagementLimit");
      auto get=nv.fn<int(*)(H,U*)>("nvmlDeviceGetPowerManagementLimit");U old=0,after=0;
      if(!set||!get||get(h,&old)!=0){error="NVML: leitura inicial indisponível";return false;}
      rc=set(h,U(value)*1000);
      if(rc==0&&get(h,&after)==0&&after==U(value)*1000)return true;
      bool restored=set(h,old)==0&&get(h,&after)==0&&after==old;
      error=QString("NVML recusou/não confirmou o limite; restauração %1").arg(restored?"confirmada":"NÃO confirmada");return false;
    }
    if(domain.startsWith("fan")) {
      U index=domain.mid(3).toUInt();
      auto reset=nv.fn<int(*)(H,U)>("nvmlDeviceSetDefaultFanSpeed_v2");
      auto set=nv.fn<int(*)(H,U,U)>("nvmlDeviceSetFanSpeed_v2");
      auto get=nv.fn<int(*)(H,U,U*)>("nvmlDeviceGetFanControlPolicy_v2");
      auto target=nv.fn<int(*)(H,U,U*)>("nvmlDeviceGetTargetFanSpeed");
      rc=automatic?(reset?reset(h,index):-1):(set?set(h,index,U(value)):-1);
      U mode=99,after=0;
      // Policy and target verify the command, not physical RPM (which has inertia).
      if(rc==0&&get&&get(h,index,&mode)==0&&mode==U(automatic?0:1)&&
         (automatic||(target&&target(h,index,&after)==0&&after==U(value))))return true;
      bool restored=reset&&reset(h,index)==0&&get&&get(h,index,&mode)==0&&mode==0;
      error=QString("Ventoinha não confirmada; retorno automático %1").arg(restored?"confirmado":"NÃO confirmado");return false;
    }
    error="Operação NVML não suportada";return false;
  }
  QString base=amdHwmon(g);
  if(domain=="power") {
    QString old=readText(base+"power1_cap");
    bool ok=put(base+"power1_cap",QString::number(qint64(value)*1000000),error);
    auto after=readNumber(base+"power1_cap");
    if(ok&&after&&*after==qint64(value)*1000000)return true;
    bool restored=put(base+"power1_cap",old,error)&&readText(base+"power1_cap")==old;
    error=QString("Limite não confirmado; restauração %1").arg(restored?"confirmada":"NÃO confirmada");return false;
  }
  if(domain=="fan0") {
    if(automatic) {
      bool ok=put(base+"pwm1_enable","2",error)&&readText(base+"pwm1_enable")=="2";
      if(!ok)error="Retorno automático da ventoinha NÃO confirmado: "+error;
      return ok;
    }
    int pwm=qRound(value*255./100.);
    bool ok=put(base+"pwm1_enable","1",error)&&put(base+"pwm1",QString::number(pwm),error);
    if(ok&&readText(base+"pwm1_enable")=="1"&&readText(base+"pwm1").toInt()==pwm)return true;
    bool restored=put(base+"pwm1_enable","2",error)&&readText(base+"pwm1_enable")=="2";
    error=QString("Ventoinha não confirmada; retorno automático %1").arg(restored?"confirmado":"NÃO confirmado");return false;
  }
  if(domain=="voltageOffset") {
    QString perf=g.sys+"/power_dpm_force_performance_level",od=g.sys+"/pp_od_clk_voltage",previous=readText(perf);
    if(!QStringList{"auto","manual","low","high","profile_standard","profile_min_sclk","profile_min_mclk","profile_peak"}.contains(previous)){error="Perfil de energia não reconhecido";return false;}
    if(!put(perf,"manual",error))return false;
    bool ok=put(od,"vo "+QString::number(value),error)&&put(od,"c",error);
    auto after=amdVoltageOffset(readText(od));
    ok=ok&&!after.isEmpty()&&after["current"].toInt()==value;
    bool restored=true;
    if(!ok) {
      restored=put(od,"vo "+QString::number(c["current"].toInt()),error)&&put(od,"c",error);
      after=amdVoltageOffset(readText(od));restored=restored&&!after.isEmpty()&&after["current"]==c["current"];
    }
    bool profile=put(perf,previous,error)&&readText(perf)==previous;
    after=amdVoltageOffset(readText(od));
    if(ok&&profile&&!after.isEmpty()&&after["current"].toInt()==value)return true;
    error=QString("Tensão não confirmada; offset anterior %1; perfil %2").arg(restored?"preservado/restaurado":"NÃO confirmado",profile?"restaurado":"NÃO confirmado");return false;
  }
  error="Operação não suportada";return false;
}
