#include "cpu_telemetry.h"
#include <cmath>
std::optional<CpuCounters> parseCpuCounters(const QString &text) {
  const auto f=text.section('\n',0,0).simplified().split(' ');
  if(f.size()<5 || f[0]!="cpu") return {};
  CpuCounters c;
  // guest/guest_nice are already included in user/nice; never count them twice.
  for(int i=1;i<qMin(9,int(f.size()));++i) {
    bool ok=false; auto n=f[i].toULongLong(&ok); if(!ok) return {};
    c.total+=n; if(i==4 || i==5) c.idle+=n;
  }
  return c;
}
QJsonObject CpuTelemetry::sample(const QString &procStat,const QString &sys) {
  QJsonObject out;
  auto now=parseCpuCounters(readText(procStat));
  if(now && previous && now->total>previous->total && now->idle>=previous->idle) {
    auto total=now->total-previous->total, idle=now->idle-previous->idle;
    if(idle<=total) out["cpuUsage"]=100.*double(total-idle)/double(total);
  }
  previous=now;
  double temp=-1000; QString source; int priority=-1;
  auto consider=[&](const QString &path,int rank) {
    auto n=readNumber(path); if(!n || !std::isfinite(*n)) return;
    double c=*n/1000.; if(c< -20 || c>150) return;
    if(rank>priority || (rank==priority && c>temp)) {temp=c;priority=rank;source=path;}
  };
  QDir hw(sys+"/hwmon");
  for(auto dir:hw.entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
    QString root=hw.filePath(dir), name=readText(root+"/name").toLower();
    if(!QStringList{"coretemp","k10temp","zenpower","cpu_thermal","cpu-thermal"}.contains(name)) continue;
    for(auto f:QDir(root).entryList({"temp*_input"},QDir::Files)) {
      QString label=readText(root+"/"+QString(f).replace("_input","_label")).toLower();
      int rank=label.contains("tdie")?4:label.contains("package")?3:label.contains("tctl")?1:2;
      consider(root+"/"+f,rank);
    }
  }
  if(priority<0) {
    QDir thermal(sys+"/thermal");
    for(auto zone:thermal.entryList({"thermal_zone*"},QDir::Dirs|QDir::NoDotAndDotDot)) {
      QString root=thermal.filePath(zone),type=readText(root+"/type").toLower();
      if(type.contains("cpu") || type.contains("x86_pkg") || type.contains("soc_thermal")) consider(root+"/temp",0);
    }
  }
  if(priority>=0) {out["cpuTemp"]=temp;out["cpuTempSource"]=source;}
  return out;
}
