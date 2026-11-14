#include "update_manager.h"
#include <archive.h>
#include <archive_entry.h>
#include "driver_packages.h"
namespace {
constexpr qint64 maxPackage = 128ll * 1024 * 1024, maxFiles = 512ll * 1024 * 1024;
const QString repository = "eritongrnascimento-svg/GPU-PRO";
bool safePath(QString p) {
  if (p.isEmpty() || p.startsWith('/') || p.contains('\\')) return false;
  for (auto segment : p.split('/')) if (segment == ".." || segment == "." || segment.isEmpty()) return false;
  return true;
}
bool allowed(QString p) {
  return p == "run.sh" || p == "build.sh" || p == "README.md" || p == "LICENSE" ||
    p == ".gitignore" || p == "NOTICE" || p == "CONTRIBUTING.md" || p == "CHANGELOG.md" || p == "SECURITY.md" ||
    p == "CMakeLists.txt" || p == "release.json" || p.startsWith("bin/") ||
    p.startsWith("src/") || p.startsWith("docs/") || p.startsWith("shaders/") ||
    p.startsWith("packaging/") || p.startsWith("tools/") || p.startsWith("assets/") || p.startsWith("examples/") || p.startsWith(".github/");
}
bool copyAtomic(QString from, QString to, QString &error) {
  if (QFileInfo(from).isSymLink() || QFileInfo(to).isSymLink()) { error = "Links simbólicos não são aceitos na atualização"; return false; }
  QFile in(from); QSaveFile out(to);
  if (!QDir().mkpath(QFileInfo(to).absolutePath()) || !in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly)) { error = "Não foi possível copiar " + QFileInfo(from).fileName(); return false; }
  while (!in.atEnd()) { auto bytes = in.read(1024 * 1024); if (bytes.isEmpty() && in.error() != QFile::NoError) { error = in.errorString(); return false; } if (out.write(bytes) != bytes.size()) { error = out.errorString(); return false; } }
  out.setPermissions(in.permissions());
  if (!out.commit()) { error = out.errorString(); return false; }
  return true;
}
bool validRoot(QString root) {
  return QFileInfo(root + "/run.sh").isFile() && QFileInfo(root + "/bin/linux-gpu-control").isFile() && !QFileInfo(root).isSymLink();
}
QStringList installedFiles(QString root, QString &error) {
  QStringList files; qint64 total = 0;
  QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next(); auto info = it.fileInfo(); QString relative = QDir(root).relativeFilePath(info.filePath());
    if (!allowed(relative)) continue;
    if (info.isSymLink()) { error = "Instalação contém link simbólico: " + relative; return {}; }
    if (!info.isFile()) continue;
    if (!safePath(relative) || (total += info.size()) > maxFiles || files.size() >= 5000) { error = "Instalação excede os limites de backup"; return {}; }
    files << relative;
  }
  return files;
}
bool safeDestination(QString root, QString relative) {
  if (!safePath(relative) || !allowed(relative)) return false;
  QString path = root;
  for (auto part : relative.split('/')) { path += '/' + part; if (QFileInfo(path).isSymLink()) return false; }
  return true;
}
}
UpdateManager::UpdateManager(QString installRoot, QString dataRoot, QObject *parent)
 : QObject(parent), root(QDir(installRoot).absolutePath()), storage(QDir(dataRoot).absolutePath()) {}
bool UpdateManager::newer(QString candidate, QString current) {
  static QRegularExpression version("^[0-9]+\\.[0-9]+\\.[0-9]+$");
  if (!version.match(candidate).hasMatch() || !version.match(current).hasMatch()) return false;
  return QVersionNumber::compare(QVersionNumber::fromString(candidate), QVersionNumber::fromString(current)) > 0;
}
bool UpdateManager::officialUrl(const QUrl &url) {
  if (url.scheme() != "https" || !url.userInfo().isEmpty() || url.port(-1) != -1 || !url.query().isEmpty() || !url.fragment().isEmpty()) return false;
  return (url.host() == "api.github.com" && url.path().startsWith("/repos/" + repository + "/releases/")) ||
    (url.host() == "github.com" && url.path().startsWith('/' + repository + "/releases/download/"));
}
void UpdateManager::fetch(QUrl url, qint64 maximum, std::function<void(QByteArray, QString)> done) {
  if (!officialUrl(url)) { done({}, "Endereço fora do repositório oficial"); return; }
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader, "GPU-PRO/0.7.0");
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setTransferTimeout(30000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  auto *reply = network.get(request);
  auto bytes = std::make_shared<QByteArray>();
  connect(reply, &QNetworkReply::readyRead, this, [reply, bytes, maximum] {
    bytes->append(reply->readAll()); if (bytes->size() > maximum) reply->abort();
  });
  connect(reply, &QNetworkReply::finished, this, [reply, bytes, maximum, done] {
    bytes->append(reply->readAll());
    QString error;
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) error = "Nenhuma release oficial disponível ainda";
    else if (reply->error() != QNetworkReply::NoError) error = reply->errorString();
    else if (bytes->size() > maximum) error = "Download excedeu o tamanho permitido";
    done(error.isEmpty() ? *bytes : QByteArray{}, error); reply->deleteLater();
  });
}
void UpdateManager::check() {
  if (working) return;
  if (QSysInfo::currentCpuArchitecture() != "x86_64") { emit message("Atualizações binárias disponíveis somente para x86_64; compile pelo código-fonte"); emit checked({}, false); return; }
  working = true; release = {};
  emit message("Consultando releases oficiais de " + repository + "…");
  fetch(QUrl("https://api.github.com/repos/" + repository + "/releases/latest"), 2 * 1024 * 1024,
    [this](QByteArray bytes, QString error) {
      working = false;
      if (!error.isEmpty()) { emit message(error); emit checked({}, false); return; }
      auto json = QJsonDocument::fromJson(bytes).object(); QString tag = json["tag_name"].toString();
      QString version = tag.startsWith('v') ? tag.mid(1) : tag;
      if (json["draft"].toBool() || json["prerelease"].toBool() || !newer(version, QCoreApplication::applicationVersion())) { emit message("Nenhuma versão estável mais recente disponível"); emit checked(version, false); return; }
      release.version = version; release.tag = tag;
      release.assetName = kind=="appimage" ? "GPU-PRO-"+version+"-x86_64.AppImage" : kind=="deb" ? "gpu-pro_"+version+"_ubuntu24.04_amd64.deb" : "GPU-PRO-"+version+"-linux-x86_64.tar.gz";
      for (auto value : json["assets"].toArray()) {
        auto a = value.toObject();
        if (a["name"].toString() == release.assetName) release.assetUrl = QUrl(a["browser_download_url"].toString());
        if (a["name"].toString() == "SHA256SUMS.txt") release.checksumUrl = QUrl(a["browser_download_url"].toString());
      }
      bool ready = officialUrl(release.assetUrl) && officialUrl(release.checksumUrl) &&
        release.assetUrl.path().contains("/download/" + tag + "/") && release.checksumUrl.path().contains("/download/" + tag + "/");
      emit message(ready ? "GPU-PRO " + version + " disponível. Será criado backup antes da instalação." : "Release encontrada, mas pacote x86_64 ou SHA256SUMS.txt está ausente");
      emit checked(version, ready);
    });
}
QString UpdateManager::backup(QString &error) {
  if(kind=="appimage")return imageBackup(error);
  if(kind=="deb"){error="Use o backup do pacote instalado pelo painel";return {}; }
  if (!validRoot(root) || !QFileInfo(root).isWritable()) { error = "Atualização pelo painel requer pacote portátil com bin/ e diretório gravável pelo usuário"; return {}; }
  auto files = installedFiles(root, error); if (!error.isEmpty() || files.isEmpty()) return {};
  QString folder = storage + "/backups/" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz") + '-' + QUuid::createUuid().toString(QUuid::WithoutBraces);
  if (!QDir().mkpath(folder + "/files")) { error = "Falha ao criar backup"; return {}; }
  for (auto file : files) if (!copyAtomic(root + '/' + file, folder + "/files/" + file, error)) return {};
  QSaveFile manifest(folder + "/backup.json");
  auto data = QJsonDocument(QJsonObject{{"root", root}, {"files", QJsonArray::fromStringList(files)}, {"version", QCoreApplication::applicationVersion()}}).toJson();
  if (!manifest.open(QIODevice::WriteOnly) || manifest.write(data) != data.size() || !manifest.commit()) { error = "Falha ao concluir manifesto do backup"; return {}; }
  QSaveFile pointer(storage + "/last-backup.txt");
  if (!pointer.open(QIODevice::WriteOnly) || pointer.write(folder.toUtf8()) < 0 || !pointer.commit()) { error = "Falha ao registrar backup"; return {}; }
  return folder;
}
bool UpdateManager::restore(QString folder, QString &error) {
  if(kind=="appimage") {
    QFile manifest(storage+"/app-last-backup.json");if(!manifest.open(QIODevice::ReadOnly)){error="Backup AppImage ausente";return false;}
    auto data=QJsonDocument::fromJson(manifest.readAll()).object();QString file=data["file"].toString();
    if(data["kind"]!="appimage"||data["target"]!=managedTarget||!QFileInfo(file).canonicalFilePath().startsWith(QFileInfo(storage+"/app-backups").canonicalFilePath()+'/')){error="Backup pertence a outra instalação";return false;}
    QFile input(file);if(!input.open(QIODevice::ReadOnly)){error="Arquivo de backup ausente";return false;}QCryptographicHash h(QCryptographicHash::Sha256);h.addData(&input);
    if(h.result().toHex()!=data["sha256"].toString().toLatin1()){error="Checksum do backup não confere";return false;}
    return copyAtomic(file,managedTarget,error);
  }
  if (folder.isEmpty()) { QFile f(storage + "/last-backup.txt"); if (f.open(QIODevice::ReadOnly)) folder = QString::fromUtf8(f.readAll()); }
  QString canonical = QFileInfo(folder).canonicalFilePath(), backups = QFileInfo(storage + "/backups").canonicalFilePath();
  if (backups.isEmpty() || !canonical.startsWith(backups + '/')) { error = "Backup local não encontrado"; return false; }
  QFile manifest(canonical + "/backup.json"); if (!manifest.open(QIODevice::ReadOnly)) { error = "Manifesto do backup ausente"; return false; }
  auto data = QJsonDocument::fromJson(manifest.readAll()).object();
  if (data["root"].toString() != root || !validRoot(root)) { error = "Backup pertence a outra instalação"; return false; }
  auto files = data["files"].toArray();
  if (files.isEmpty() || files.size() > 5000) { error = "Backup inválido"; return false; }
  for (auto value : files) if (!safeDestination(root, value.toString()) || !QFileInfo(canonical + "/files/" + value.toString()).isFile()) { error = "Caminho de backup inválido"; return false; }
  for (auto value : files) if (!copyAtomic(canonical + "/files/" + value.toString(), root + '/' + value.toString(), error)) return false;
  for (auto value : data["introduced"].toArray()) {
    QString path = value.toString();
    if (safeDestination(root, path) && !files.contains(path) && QFileInfo(root + '/' + path).isFile() && !QFile::remove(root + '/' + path)) { error = "Falha ao remover arquivo introduzido: " + path; return false; }
  }
  return true;
}
bool UpdateManager::installArchive(QString archiveFile, QByteArray expected, QString version, QString &error) {
  if (!mayInstall()) { error = "Operação em andamento; tente atualizar após os testes/ajustes"; return false; }
  QFile package(archiveFile);
  if (expected.size() != 64 || !QRegularExpression("^[0-9a-fA-F]{64}$").match(QString::fromLatin1(expected)).hasMatch() || !package.open(QIODevice::ReadOnly) || package.size() > maxPackage) { error = "Pacote ou checksum inválido"; return false; }
  QCryptographicHash hash(QCryptographicHash::Sha256); hash.addData(&package); package.close();
  if (hash.result().toHex() != expected.toLower()) { error = "SHA-256 não confere; instalação recusada"; return false; }
  QDir().mkpath(storage);
  QTemporaryDir stage(storage + "/stage-XXXXXX");
  if (!stage.isValid()) { error = "Sem espaço/diretório para preparar atualização"; return false; }
  archive *reader = archive_read_new(); archive_read_support_filter_gzip(reader); archive_read_support_format_tar(reader);
  auto cleanup = qScopeGuard([&] { archive_read_free(reader); });
  if (archive_read_open_filename(reader, QFile::encodeName(archiveFile), 65536) != ARCHIVE_OK) { error = "Arquivo tar.gz inválido"; return false; }
  QStringList files; QSet<QString> entries; QString prefix; qint64 total = 0; archive_entry *entry; int state;
  while ((state = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
    QString name = QString::fromUtf8(archive_entry_pathname(entry));
    if (name.endsWith('/')) name.chop(1);
    if (!safePath(name) || entries.contains(name) || archive_entry_symlink(entry) || archive_entry_hardlink(entry)) { error = "Pacote contém caminho ou link proibido"; return false; }
    entries.insert(name);
    QString head = name.section('/', 0, 0);
    if (prefix.isEmpty()) prefix = head;
    if ((prefix != "GPU-PRO" && prefix != "LinuxGPUControl") || head != prefix || entries.size() > 5000) { error = "Estrutura de pacote inválida"; return false; }
    QString relative = name.mid(prefix.size() + 1);
    if (archive_entry_filetype(entry) == AE_IFDIR) continue;
    if (archive_entry_filetype(entry) != AE_IFREG || !safePath(relative) || !allowed(relative) || !safeDestination(root, relative)) { error = "Arquivo de atualização não permitido"; return false; }
    qint64 size = archive_entry_size(entry); total += size;
    if (size < 0 || size > maxPackage || total > maxFiles) { error = "Pacote expandido excede limites"; return false; }
    QDir().mkpath(QFileInfo(stage.path() + '/' + relative).absolutePath());
    QSaveFile output(stage.path() + '/' + relative);
    if (!output.open(QIODevice::WriteOnly)) { error = output.errorString(); return false; }
    char buffer[65536]; la_ssize_t count; qint64 actual = 0;
    while ((count = archive_read_data(reader, buffer, sizeof buffer)) > 0) {
      actual += count;
      if (actual > size || output.write(buffer, count) != count) { error = "Falha ao extrair pacote"; return false; }
    }
    if (count < 0 || actual != size || !output.commit()) { error = "Pacote incompleto"; return false; }
    auto perms = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther;
    if (relative.startsWith("bin/") || relative.endsWith(".sh")) perms |= QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther;
    QFile::setPermissions(stage.path() + '/' + relative, perms);
    files << relative;
  }
  if (state != ARCHIVE_EOF || !validRoot(stage.path())) { error = "Pacote truncado ou executável ausente"; return false; }
  QFile releaseFile(stage.path() + "/release.json"); if (!releaseFile.open(QIODevice::ReadOnly)) { error = "Manifesto da release ausente"; return false; }
  auto metadata = QJsonDocument::fromJson(releaseFile.readAll()).object();
  if (metadata["version"].toString() != version || metadata["repository"].toString() != repository || metadata["architecture"].toString() != "x86_64") { error = "Manifesto não corresponde à release"; return false; }
  QString saved = backup(error); if (saved.isEmpty()) return false;
  QFile manifest(saved + "/backup.json"); if (!manifest.open(QIODevice::ReadOnly)) { error = "Manifesto do backup indisponível"; return false; } auto data = QJsonDocument::fromJson(manifest.readAll()).object(); manifest.close();
  QJsonArray introduced;
  for (auto file : files) if (!QFileInfo::exists(root + '/' + file)) introduced.append(file);
  data["introduced"] = introduced;
  QSaveFile updatedManifest(saved + "/backup.json"); auto encoded = QJsonDocument(data).toJson();
  if (!updatedManifest.open(QIODevice::WriteOnly) || updatedManifest.write(encoded) != encoded.size() || !updatedManifest.commit()) { error = "Falha ao registrar restauração"; return false; }
  for (auto file : files) if (!copyAtomic(stage.path() + '/' + file, root + '/' + file, error)) {
    QString rollbackError; bool rolledBack = restore(saved, rollbackError);
    error += rolledBack ? " · Versão anterior restaurada" : " · Restauração incompleta: " + rollbackError;
    return false;
  }
  emit message("Atualização instalada. Backup: " + saved + ". Reinicie o GPU-PRO. O helper administrativo não foi substituído.");
  return true;
}
void UpdateManager::downloadAndInstall() {
  if (working || !officialUrl(release.assetUrl)) return;
  working = true;
  emit message("Baixando checksum da release oficial…");
  fetch(release.checksumUrl, 65536, [this](QByteArray sums, QString error) {
    QByteArray expected;
    for (auto line : sums.split('\n')) {
      auto parts = line.simplified().split(' ');
      if (parts.size() == 2 && (parts[1] == release.assetName.toUtf8() || parts[1] == '*' + release.assetName.toUtf8())) expected = parts[0];
    }
    if (!error.isEmpty() || expected.size() != 64) { working = false; emit message(error.isEmpty() ? "Checksum do pacote ausente" : error); emit finished(false); return; }
    emit message("Baixando pacote oficial; a versão atual será salva antes de instalar…");
    fetch(release.assetUrl, maxPackage, [this, expected](QByteArray bytes, QString error) {
      bool installed = false;
      if (error.isEmpty()) {
        QDir().mkpath(storage); QTemporaryFile package(storage + "/download-XXXXXX.tar.gz");
        if (!package.open() || package.write(bytes) != bytes.size() || !package.flush()) error = "Falha ao salvar download";
        else if(kind=="appimage")installed=installImage(package.fileName(),expected,release.version,error);
        else if(kind=="deb") {
          QString path;
          if(stageDeb(package.fileName(),expected,release.version,path,error)) {
            backupDeb([this,path](QString e){working=false;if(!e.isEmpty()){emit message(e);emit finished(false);return;}emit message("Novo DEB verificado e pacote anterior preservado. Confira a simulação antes de autorizar a instalação.");emit managedPackageReady(path,release.version,false);});
            return;
          }
        } else installed = installArchive(package.fileName(), expected, release.version, error);
      }
      working = false;
      if (!error.isEmpty()) emit message(error);
      emit finished(installed);
    });
  });
}

namespace {
bool jsonAtomic(QString path,QJsonObject value,QString &error) {
  QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile f(path);auto data=QJsonDocument(value).toJson();
  if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit()){error="Falha ao registrar backup";return false;}return true;
}
QByteArray fileDigest(QString path) {QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};QCryptographicHash hash(QCryptographicHash::Sha256);hash.addData(&f);return hash.result().toHex();}
bool verifiedFile(QString file,QByteArray expected,QString &error) {
  if(expected.size()!=64||!QRegularExpression("^[a-fA-F0-9]{64}$").match(QString::fromLatin1(expected)).hasMatch()||QFileInfo(file).size()>maxPackage||fileDigest(file)!=expected.toLower()){error="Pacote/checksum inválido";return false;}return true;
}
QByteArray checksum(QByteArray sums,QString name) {for(auto l:sums.split('\n')){auto parts=l.simplified().split(' ');if(parts.size()==2&&(parts[1]==name.toUtf8()||parts[1]=='*'+name.toUtf8()))return parts[0];}return {};}
QString debField(QString path,QString field) {
  QProcess p;p.setProcessEnvironment(nativeEnvironment());p.start("/usr/bin/dpkg-deb",{"--field",path,field});if(!p.waitForFinished(10000)||p.exitCode()!=0)return {};return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}
}
QString UpdateManager::imageBackup(QString &error) {
  QFileInfo info(managedTarget);
  if(info.isSymLink()||!info.isFile()||!QFileInfo(info.absolutePath()).isWritable()){error="AppImage precisa ser arquivo normal em pasta gravável pelo seu usuário";return {};}
  QString folder=storage+"/app-backups/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+'-'+QUuid::createUuid().toString(QUuid::WithoutBraces);QDir().mkpath(folder);
  QString target=folder+"/GPU-PRO-"+QCoreApplication::applicationVersion()+".AppImage";
  if(!copyAtomic(managedTarget,target,error))return {};
  QJsonObject manifest{{"kind","appimage"},{"target",managedTarget},{"file",target},{"sha256",QString::fromLatin1(fileDigest(target))},{"version",QCoreApplication::applicationVersion()}};
  if(!jsonAtomic(storage+"/app-last-backup.json",manifest,error))return {};
  return target;
}
bool UpdateManager::installImage(QString file,QByteArray expected,QString version,QString &error) {
  if(!mayInstall()){error="Operação em andamento; atualização recusada";return false;}
  if(!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+$").match(version).hasMatch()||!verifiedFile(file,expected,error))return false;
  QFile input(file);if(!input.open(QIODevice::ReadOnly)){error="AppImage indisponível";return false;}auto header=input.read(64);input.close();
  if(header.size()<64||header.left(4)!=QByteArray("\x7f" "ELF",4)||header[4]!=2||header[5]!=1||header[18]!=62||header[19]!=0||header.mid(8,3)!=QByteArray("AI\x02",3)){error="Arquivo não é AppImage tipo 2 x86_64";return false;}
  auto permissions=QFile::permissions(managedTarget);
  if(imageBackup(error).isEmpty())return false;
  if(!copyAtomic(file,managedTarget,error))return false;
  QFile::setPermissions(managedTarget,permissions|QFileDevice::ExeOwner);
  emit message("AppImage atualizado no mesmo caminho com backup preservado. Reinicie o aplicativo.");return true;
}
bool UpdateManager::stageDeb(QString file,QByteArray expected,QString version,QString &path,QString &error) {
  if(!mayInstall()){error="Operação em andamento; instalação recusada";return false;}
  if(!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+$").match(version).hasMatch()||!verifiedFile(file,expected,error))return false;
  if(debField(file,"Package")!="gpu-pro"||debField(file,"Version")!=version||debField(file,"Architecture")!="amd64"){error="Identificação/versão/arquitetura do DEB não corresponde à release";return false;}
  path=storage+"/downloads/gpu-pro_"+version+"_amd64.deb";
  return copyAtomic(file,path,error);
}
void UpdateManager::backupDeb(std::function<void(QString)> done) {
  QString current=QCoreApplication::applicationVersion();
  QProcess installed;installed.setProcessEnvironment(nativeEnvironment());installed.start("/usr/bin/dpkg-query",{"-W","-f=${Version}","gpu-pro"});
  if(!installed.waitForFinished(5000)||installed.exitCode()!=0||QString::fromUtf8(installed.readAllStandardOutput()).trimmed()!=current){done("A versão do pacote instalado difere desta execução. Reinicie o painel antes de atualizar ou salvar backup.");return;}
  if(!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+$").match(current).hasMatch()){done("Versão instalada não reconhecida");return;}
  QString name="gpu-pro_"+current+"_ubuntu24.04_amd64.deb";
  QString url="https://github.com/"+repository+"/releases/download/v"+current+'/';
  emit message("Preservando o pacote oficial da versão atual antes da instalação…");
  fetch(QUrl(url+"SHA256SUMS.txt"),65536,[this,done,url,name,current](QByteArray sums,QString error){
    auto expected=checksum(sums,name);if(!error.isEmpty()||expected.size()!=64){done("Não foi possível obter backup da versão instalada. Nada foi instalado. "+error);return;}
    QString folder=storage+"/app-backups/"+current;QDir().mkpath(folder);QString path=folder+'/'+name;
    auto complete=[this,done,path,current,expected](QByteArray bytes,QString error){
      if(!error.isEmpty()){done(error);return;}
      if(!bytes.isEmpty()){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit()){done("Falha ao salvar DEB anterior");return;}}
      if(!verifiedFile(path,expected,error)||debField(path,"Package")!="gpu-pro"||debField(path,"Version")!=current||debField(path,"Architecture")!="amd64"){done("Pacote de backup não confere: "+error);return;}
      QJsonObject manifest{{"kind","deb"},{"file",path},{"version",current},{"sha256",QString::fromLatin1(expected)}};
      jsonAtomic(storage+"/app-last-backup.json",manifest,error);done(error);
    };
    if(fileDigest(path)==expected)complete({},{});else fetch(QUrl(url+name),maxPackage,complete);
  });
}
void UpdateManager::backupCurrent() {
  if(working)return;
  if(kind=="deb"){working=true;backupDeb([this](QString error){working=false;emit message(error.isEmpty()?"Pacote anterior salvo para restauração":error);});}
  else {QString error;auto path=backup(error);emit message(path.isEmpty()?error:"Backup salvo: "+path);}
}
void UpdateManager::restoreManaged() {
  if(working||!mayInstall())return;
  if(kind=="appimage"){QString error;bool ok=restore({},error);emit message(ok?"AppImage anterior restaurado. Reinicie.":error);emit finished(ok);return;}
  QFile f(storage+"/app-last-backup.json");if(!f.open(QIODevice::ReadOnly)){emit message("Nenhum backup do pacote disponível");return;}
  auto b=QJsonDocument::fromJson(f.readAll()).object();QString file=b["file"].toString(),version=b["version"].toString(),error;
  QString base=QFileInfo(storage+"/app-backups").canonicalFilePath();
  if(b["kind"]!="deb"||base.isEmpty()||!QFileInfo(file).canonicalFilePath().startsWith(base+'/')||!verifiedFile(file,b["sha256"].toString().toLatin1(),error)||debField(file,"Package")!="gpu-pro"||debField(file,"Version")!=version){emit message("Backup DEB não validado: "+error);return;}
  emit managedPackageReady(file,version,true);
}
