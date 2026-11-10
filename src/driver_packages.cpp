#include "driver_packages.h"
namespace {
bool token(QString s) {return QRegularExpression("^[A-Za-z0-9][A-Za-z0-9+._:~\\-]*$").match(s).hasMatch();}
QString binary(QString manager) {return manager == "pacman" ? "/usr/bin/pacman" : manager == "apt" ? "/usr/bin/apt-cache" : manager == "dnf" ? "/usr/bin/dnf" : QString{};}
}
QProcessEnvironment nativeEnvironment() {
  auto env = QProcessEnvironment::systemEnvironment();
  if (env.contains("APPIMAGE") || env.contains("APPDIR")) {
    for (auto name : {"APPIMAGE","APPDIR","LD_LIBRARY_PATH","LD_PRELOAD","QT_PLUGIN_PATH","QT_QPA_PLATFORM_PLUGIN_PATH","QML2_IMPORT_PATH","GTK_PATH","GIO_EXTRA_MODULES","GDK_PIXBUF_MODULE_FILE","PYTHONHOME","PYTHONPATH"}) env.remove(name);
  }
  return env;
}
QString nativePackageManager() {
  QFile os("/etc/os-release"); if (!os.open(QIODevice::ReadOnly)) return {};
  QString id;
  for (auto line : QString::fromUtf8(os.readAll()).split('\n'))
    if (line.startsWith("ID=") || line.startsWith("ID_LIKE=")) id += ' ' + line.section('=',1).remove('"');
  QString manager = id.contains("arch") || id.contains("cachyos") || id.contains("manjaro") ? "pacman"
      : id.contains("debian") || id.contains("ubuntu") ? "apt"
      : id.contains("fedora") || id.contains("rhel") ? "dnf" : QString{};
  return QFileInfo(binary(manager)).isExecutable() ? manager : QString{};
}
bool driverPackageAllowed(QString name, QString vendor) {
  if (!token(name)) return false;
  bool nvidia = QRegularExpression("^(?:(?:lib32-)?nvidia(?:-[A-Za-z0-9+._-]+)*|libnvidia-[A-Za-z0-9+._-]+|linux(?:-[a-z0-9]+)*-nvidia(?:-open)?|akmod-nvidia|xorg-x11-drv-nvidia(?:-[a-z0-9-]+)?)$").match(name).hasMatch();
  bool shared = QStringList{"mesa","lib32-mesa","mesa-vulkan-drivers","mesa-va-drivers","mesa-dri-drivers","libgl1-mesa-dri"}.contains(name);
  bool amd = QStringList{"vulkan-radeon","lib32-vulkan-radeon","libva-mesa-driver","lib32-libva-mesa-driver","xf86-video-amdgpu","xf86-video-ati","firmware-amd-graphics"}.contains(name);
  bool intel = QStringList{"vulkan-intel","lib32-vulkan-intel","intel-media-driver","intel-media-va-driver","intel-media-va-driver-non-free","i965-va-driver","libva-intel-driver","xf86-video-intel","firmware-intel-graphics"}.contains(name);
  return vendor == "NVIDIA" ? nvidia : vendor == "AMD" ? amd || shared : vendor == "Intel" ? intel || shared : nvidia || shared || amd || intel;
}
QJsonArray parseDriverPackages(QString manager, QString text, QString vendor) {
  QJsonArray result; QSet<QString> seen;
  for (const auto &line : text.split('\n')) {
    QString name, version, repo, architecture, installed;
    if (manager == "pacman") {
      auto parts = line.simplified().split(' '); if (parts.size() < 3) continue;
      repo=parts[0];name=parts[1];version=parts[2];
      if (line.contains("[installed]")) installed=version;
      else { auto match=QRegularExpression("\\[installed: ([^\\]]+)\\]").match(line); if(match.hasMatch()) installed=match.captured(1); }
    } else if (manager == "apt") {
      auto parts=line.split('|');if(parts.size()<3)continue;
      name=parts[0].trimmed();version=parts[1].trimmed();repo=parts[2].trimmed();
    } else if (manager == "dnf") {
      auto parts=line.simplified().split(' ');if(parts.size()<3||!parts[0].contains('.'))continue;
      name=parts[0].section('.',0,-2);architecture=parts[0].section('.',-1);version=parts[1];repo=parts[2];
      if(!QStringList{"x86_64","i686","noarch","aarch64"}.contains(architecture))continue;
    } else continue;
    if (!driverPackageAllowed(name,vendor) || !token(version) || repo.isEmpty() || repo.size()>240) continue;
    if (manager != "apt" && !token(repo)) continue;
    QString key=name+'|'+version+'|'+repo+'|'+architecture;if(seen.contains(key))continue;seen.insert(key);
    result.append(QJsonObject{{"manager",manager},{"name",name},{"version",version},{"repo",repo},{"architecture",architecture},{"installed",installed}});
    if (result.size()>=1500) break;
  }
  return result;
}
QStringList driverTransaction(const QJsonObject &candidate, bool simulate) {
  QString manager=candidate["manager"].toString(),name=candidate["name"].toString(),version=candidate["version"].toString(),repo=candidate["repo"].toString();
  if (!driverPackageAllowed(name) || !token(version)) return {};
  if (manager=="pacman" && token(repo)) return simulate ? QStringList{"/usr/bin/pacman","-Sup","--needed","--print-format","%r/%n %v",repo+'/'+name+'='+version}
      : QStringList{"/usr/bin/pacman","-Syu","--needed",repo+'/'+name+'='+version};
  if (manager=="apt") return simulate ? QStringList{"/usr/bin/apt-get","--simulate","install",name+'='+version}
      : QStringList{"/usr/bin/apt-get","install",name+'='+version};
  if (manager=="dnf") {
    QString arch=candidate["architecture"].toString();if(!QStringList{"x86_64","i686","noarch","aarch64"}.contains(arch))return {};
    QString spec=name+'-'+version+'.'+arch;
    return simulate ? QStringList{"/usr/bin/dnf","--assumeno","install",spec} : QStringList{"/usr/bin/dnf","install",spec};
  }
  return {};
}
DriverCatalog::DriverCatalog(QObject *parent):QObject(parent) {
  timeout.setSingleShot(true);
  connect(&timeout,&QTimer::timeout,this,[this]{process.kill();});
  connect(&process,&QProcess::readyReadStandardOutput,this,[this]{output+=process.readAllStandardOutput();if(output.size()>16*1024*1024)process.kill();});
  connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,&DriverCatalog::completed);
  connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){timeout.stop();emit listed({},"Gerenciador nativo indisponível: "+process.errorString());}});
}
DriverCatalog::~DriverCatalog(){process.disconnect(this);process.kill();process.waitForFinished(1000);}
void DriverCatalog::run(QStringList args) {
  output.clear(); auto env = nativeEnvironment(); env.insert("LC_ALL","C"); process.setProcessEnvironment(env);process.setProgram(binary(manager));process.setArguments(args);timeout.start(30000);process.start();
}
void DriverCatalog::refresh(QString selectedVendor) {
  if(busy()) return;
  manager=nativePackageManager();vendor=selectedVendor;phase=0;installed.clear();names.clear();
  if(manager.isEmpty()){emit listed({},"Distro não suportada para instalação pelo painel. Use seu gerenciador nativo; suporte atual: APT, pacman e DNF.");return;}
  if(manager=="pacman")run({"-Sl"});
  else if(manager=="apt")run({"pkgnames"});
  else run({"--color=never","--showduplicates","list","--available","nvidia*","akmod-nvidia","xorg-x11-drv-nvidia*","mesa-vulkan-drivers","mesa-dri-drivers","intel-media-driver"});
}
void DriverCatalog::completed(int code,QProcess::ExitStatus status) {
  timeout.stop();output+=process.readAllStandardOutput();
  if(status!=QProcess::NormalExit || (code!=0 && !(manager=="apt" && phase==2))) {emit listed({},"Consulta interrompida ou repositórios indisponíveis. "+QString::fromUtf8(process.readAllStandardError()).left(800));return;}
  if(manager=="apt" && phase==0) {
    QStringList packages;for(auto name:QString::fromUtf8(output).split('\n'))if(driverPackageAllowed(name.trimmed(),vendor))packages<<name.trimmed();
    packages.removeDuplicates();if(packages.isEmpty()){emit listed({},"Nenhum pacote de driver encontrado nos índices APT locais");return;}
    if(packages.size()>300) packages=packages.mid(0,300);
    names=packages.join('\n');phase=1;run(QStringList{"madison"}+packages);return;
  }
  if(manager=="apt" && phase==1){installed=QString::fromUtf8(output);phase=2;output.clear();process.setProcessEnvironment(nativeEnvironment());process.setProgram("/usr/bin/dpkg-query");process.setArguments(QStringList{"-W","-f=${Package}\\t${Version}\\n"}+names.split('\n'));timeout.start(30000);process.start();return;}
  auto result=parseDriverPackages(manager,manager=="apt"?installed:QString::fromUtf8(output),vendor);
  if(manager=="apt"){
    QMap<QString,QString> versions;for(auto line:QString::fromUtf8(output).split('\n')){auto p=line.split('\t');if(p.size()==2)versions[p[0]]=p[1];}
    for(int i=0;i<result.size();++i){auto row=result[i].toObject();row["installed"]=versions.value(row["name"].toString());result[i]=row;}
  }
  emit listed(result,QString("%1 · %2 opções nos repositórios configurados. Consulta usa índices locais; não adiciona repositórios nem baixa instaladores externos.").arg(manager).arg(result.size()));
}
