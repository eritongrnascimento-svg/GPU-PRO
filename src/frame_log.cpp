#include "frame_log.h"
#include "gpupro_core.h"
#include <cmath>
#include <algorithm>
void FrameLog::reset() {
  file.close(); path.clear(); pending.clear(); column=-1; device=inode=0;
}
QJsonObject frameMetrics(const QJsonArray &samples) {
  QList<double> times;for(auto v:samples)if(v.isDouble())times.append(v.toDouble());
  const auto stats=gpp_frame_stats(times.constData(),int(times.size()));if(!stats.count)return {};
  QJsonObject result{{"fps",stats.current},{"fpsAvg",stats.mean},{"fpsMax",stats.maximum}};
  if(stats.lowAvailable)result["fpsLow"]=stats.low;return result;
}
#include <sys/stat.h>
QJsonArray FrameLog::poll(const QString &requested) {
  QJsonArray samples;
  resetDetected=false;
  struct stat st{};
  bool exists=!requested.isEmpty() && ::stat(QFile::encodeName(requested).constData(),&st)==0 && S_ISREG(st.st_mode);
  if(!exists) {resetDetected=!path.isEmpty();reset();return samples;}
  if(path!=requested || inode!=quint64(st.st_ino) || device!=quint64(st.st_dev) || file.pos()>st.st_size) {
    resetDetected=true;
    file.close();file.setFileName(requested);path=requested;column=-1;pending.clear();
    inode=st.st_ino;device=st.st_dev;
  }
  if(!file.isOpen() && !file.open(QIODevice::ReadOnly)) return samples;
  // Avoid unbounded memory/CPU for a per-frame log; discover its header first.
  if(file.size()-file.pos()>262144 && column>=0) {file.seek(file.size()-262144);file.readLine();pending.clear();}
  pending+=file.read(262144);
  int end;
  while((end=pending.indexOf('\n'))>=0) {
    QByteArray line=pending.left(end).trimmed();pending.remove(0,end+1);
    auto cols=line.split(',');int header=-1;
    for(int i=0;i<cols.size();++i) if(cols[i].trimmed().replace("\"", "").toLower()=="frametime") header=i;
    if(header>=0) {if(column>=0){resetDetected=true;samples={};}column=header;continue;}
    if(column<0 || column>=cols.size()) continue;
    bool ok=false;double n=cols[column].trimmed().replace("\"", "").toDouble(&ok);
    if(ok && std::isfinite(n) && n>0 && n<=10000) {samples.append(n);if(samples.size()>240)samples.removeFirst();}
  }
  if(pending.size()>16384) pending.clear();
  return samples;
}
