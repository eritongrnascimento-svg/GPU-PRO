#include "frame_log.h"
#include <cmath>
#include <sys/stat.h>
QJsonArray FrameLog::poll(const QString &requested) {
  QJsonArray samples;
  struct stat st{};
  bool exists=!requested.isEmpty() && ::stat(QFile::encodeName(requested).constData(),&st)==0 && S_ISREG(st.st_mode);
  if(!exists) {file.close();path.clear();column=-1;pending.clear();return samples;}
  if(path!=requested || inode!=quint64(st.st_ino) || device!=quint64(st.st_dev) || file.pos()>st.st_size) {
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
    for(int i=0;i<cols.size();++i) if(cols[i].trimmed().toLower()=="frametime") header=i;
    if(header>=0) {column=header;continue;}
    if(column<0 || column>=cols.size()) continue;
    bool ok=false;double n=cols[column].trimmed().toDouble(&ok);
    if(ok && std::isfinite(n) && n>0 && n<=10000) {samples.append(n);if(samples.size()>240)samples.removeFirst();}
  }
  if(pending.size()>16384) pending.clear();
  return samples;
}
