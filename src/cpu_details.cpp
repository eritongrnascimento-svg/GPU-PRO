#include "cpu_telemetry.h"
QJsonObject cpuDetails(const QString &procStat,const QString &cpuRoot) {
 QJsonObject out;const QString proc=QFileInfo(procStat).absolutePath();const QString info=readText(proc+"/cpuinfo");QSet<QString> physical;
 for(const auto &block:info.split("\n\n")){QString package,core;for(const auto &line:block.split('\n')){auto key=line.section(':',0,0).trimmed(),value=line.section(':',1).trimmed();if(key=="model name"&&!out.contains("cpuModel"))out["cpuModel"]=value;if(key=="flags"&&!out.contains("cpuFeatures"))out["cpuFeatures"]=value;if(key=="physical id")package=value;if(key=="core id")core=value;}if(!package.isEmpty()&&!core.isEmpty())physical.insert(package+":"+core);}
 if(!physical.isEmpty())out["cpuPhysicalCores"]=physical.size();out["cpuArchitecture"]=QSysInfo::currentCpuArchitecture();
 for(const auto &line:readText(proc+"/meminfo").split('\n')){bool ok=false;auto n=line.section(':',1).simplified().section(' ',0,0).toDouble(&ok);if(ok&&line.startsWith("MemTotal:"))out["systemRamTotal"]=n*1024;if(ok&&line.startsWith("MemAvailable:"))out["systemRamAvailable"]=n*1024;}
 QJsonArray caches;for(const auto &index:QDir(cpuRoot+"/cpu0/cache").entryList({"index*"},QDir::Dirs|QDir::NoDotAndDotDot)){QString p=cpuRoot+"/cpu0/cache/"+index;auto size=readText(p+"/size");if(!size.isEmpty())caches.append(QJsonObject{{"level",readText(p+"/level")},{"type",readText(p+"/type")},{"size",size}});}out["cpuCaches"]=caches;return out;
}
