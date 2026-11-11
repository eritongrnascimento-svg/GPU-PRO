#include "update_manager.h"
#include <archive.h>
#include <archive_entry.h>
#include <cstdio>
static bool put(QString p, QByteArray bytes) { QDir().mkpath(QFileInfo(p).absolutePath()); QFile f(p); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); }
static QString package(QString folder, QList<QPair<QString, QByteArray>> files, QString link = {}) {
  QString filename = folder + "/candidate.tar.gz";
  auto *writer = archive_write_new(); archive_write_add_filter_gzip(writer); archive_write_set_format_pax_restricted(writer);
  archive_write_open_filename(writer, QFile::encodeName(filename));
  for (auto file : files) {
    auto *entry = archive_entry_new(); archive_entry_set_pathname(entry, file.first.toUtf8());
    archive_entry_set_filetype(entry, AE_IFREG); archive_entry_set_perm(entry, 0644); archive_entry_set_size(entry, file.second.size());
    archive_write_header(writer, entry); archive_write_data(writer, file.second.constData(), file.second.size()); archive_entry_free(entry);
  }
  if (!link.isEmpty()) {
    auto *entry = archive_entry_new(); archive_entry_set_pathname(entry, "GPU-PRO/bin/link"); archive_entry_set_filetype(entry, AE_IFLNK); archive_entry_set_symlink(entry, link.toUtf8()); archive_entry_set_size(entry, 0); archive_write_header(writer, entry); archive_entry_free(entry);
  }
  archive_write_close(writer); archive_write_free(writer); return filename;
}
static QByteArray sha(QString p) { QFile f(p); if (!f.open(QIODevice::ReadOnly)) return {}; return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex(); }
static QByteArray read(QString p) { QFile f(p); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv); QCoreApplication::setApplicationVersion("0.3.0"); int errors = 0;
  if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--package") {
    QTemporaryDir sample; QString root = sample.path() + "/app", data = sample.path() + "/data";
    put(root + "/run.sh", "original"); put(root + "/bin/linux-gpu-control", "original");
    UpdateManager manager(root, data); QString error; QString file = QString::fromLocal8Bit(argv[2]);
    if (!manager.installArchive(file, sha(file), "0.3.0", error) || !manager.restore({}, error) || read(root + "/run.sh") != "original") {
      fprintf(stderr, "%s\n", error.toUtf8().constData()); return 1;
    }
    printf("Full package install and restore passed in temporary directories\n"); return 0;
  }
  auto check = [&](bool ok, const char *reason) { if (!ok) { ++errors; fprintf(stderr,"FAIL %s\n",reason); } };
  check(UpdateManager::newer("0.10.0","0.3.0") && !UpdateManager::newer("0.2.9","0.3.0") && !UpdateManager::newer("0.4.0-malicious","0.3.0"), "strict semantic versions");
  check(UpdateManager::officialUrl(QUrl("https://github.com/eritongrnascimento-svg/GPU-PRO/releases/download/v0.4.0/a.tar.gz")),"official release URL");
  for (auto url : {"https://evil.test/eritongrnascimento-svg/GPU-PRO/releases/download/a", "http://github.com/eritongrnascimento-svg/GPU-PRO/releases/download/a", "https://github.com/other/GPU-PRO/releases/download/a", "https://github.com/eritongrnascimento-svg/GPU-PRO/releases/download/a?token=secret"}) check(!UpdateManager::officialUrl(QUrl(url)), "reject unrelated hosts/repositories");
  QTemporaryDir temp; QString root = temp.path()+"/app", storage=temp.path()+"/data";
  put(root+"/run.sh","old-run"); put(root+"/bin/linux-gpu-control","old-app"); put(storage+"/results.sqlite","user-results");
  UpdateManager manager(root,storage); QString error;
  auto metadata=QJsonDocument(QJsonObject{{"version","0.4.0"},{"repository","eritongrnascimento-svg/GPU-PRO"},{"architecture","x86_64"}}).toJson();
  QList<QPair<QString,QByteArray>> files{{"GPU-PRO/run.sh","new-run"},{"GPU-PRO/bin/linux-gpu-control","new-app"},{"GPU-PRO/release.json",metadata},{"GPU-PRO/docs/new.txt","new-file"}};
  auto candidate=package(temp.path(),files);
  check(!manager.installArchive(candidate,QByteArray(64,'0'),"0.4.0",error) && read(root+"/run.sh")=="old-run","checksum refusal leaves installation unchanged"); error.clear();
  manager.mayInstall=[] {return false;}; check(!manager.installArchive(candidate,sha(candidate),"0.4.0",error),"active tests block install"); manager.mayInstall=[] {return true;}; error.clear();
  check(manager.installArchive(candidate,sha(candidate),"0.4.0",error),"verified package installs with backup");
  check(read(root+"/bin/linux-gpu-control")=="new-app" && read(storage+"/results.sqlite")=="user-results","application replaced; user results preserved");
  error.clear(); check(manager.restore({},error),"restore last backup");
  check(read(root+"/run.sh")=="old-run" && !QFileInfo::exists(root+"/docs/new.txt") && !QFileInfo::exists(root+"/release.json"),"restore includes removing introduced files");
  files.append(QPair<QString,QByteArray>{"GPU-PRO/../../escape","bad"}); candidate=package(temp.path(),files); error.clear();
  check(!manager.installArchive(candidate,sha(candidate),"0.4.0",error) && !QFileInfo::exists(temp.path()+"/escape"),"reject traversal"); files.removeLast();
  candidate=package(temp.path(),files,"/tmp"); error.clear(); check(!manager.installArchive(candidate,sha(candidate),"0.4.0",error),"reject archive symlinks");
  candidate=package(temp.path(),files); error.clear(); check(!manager.installArchive(candidate,sha(candidate),"0.5.0",error),"reject mismatched release manifest");
  error.clear(); check(!manager.restore(temp.path(),error),"reject unrelated backup paths");
  QString imagePath=temp.path()+"/GPU-PRO.AppImage",incoming=temp.path()+"/candidate.AppImage";
  QByteArray oldImage(1024,'o'),newImage(2048,'n');
  newImage.replace(0,4,QByteArray("\x7f" "ELF",4));newImage[4]=2;newImage[5]=1;newImage[19]=0;newImage.replace(8,3,QByteArray("AI\x02",3));newImage[18]=62;
  put(imagePath,oldImage);put(incoming,newImage);QFile::setPermissions(imagePath,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
  UpdateManager imageManager(temp.path(),temp.path()+"/image-data");imageManager.setFormat("appimage",imagePath);
  error.clear();check(!imageManager.installImage(incoming,QByteArray(64,'0'),"0.6.0",error)&&read(imagePath)==oldImage,"AppImage checksum refusal leaves current binary intact");
  error.clear();check(imageManager.installImage(incoming,sha(incoming),"0.6.0",error),"AppImage atomic update with complete backup");
  check(read(imagePath)==newImage&&(QFile::permissions(imagePath)&QFileDevice::ExeOwner),"AppImage replaced and remains executable");
  error.clear();check(imageManager.restore({},error)&&read(imagePath)==oldImage,"AppImage backup restores previous executable");
  put(incoming,"not-an-AppImage");error.clear();check(!imageManager.installImage(incoming,sha(incoming),"0.6.0",error)&&read(imagePath)==oldImage,"reject a non-AppImage despite matching checksum");
  UpdateManager otherImage(temp.path(),temp.path()+"/image-data");otherImage.setFormat("appimage",temp.path()+"/different.AppImage");error.clear();check(!otherImage.restore({},error),"AppImage backup cannot overwrite a different installation");
  QString staged;error.clear();check(!imageManager.stageDeb(incoming,sha(incoming),"0.6.0",staged,error),"reject non-DEB before asking for administrative installation");
  if(QFileInfo("/usr/bin/dpkg-deb").isExecutable()) {
    QString tree=temp.path()+"/deb-tree",deb=temp.path()+"/sample.deb";
    put(tree+"/DEBIAN/control","Package: gpu-pro\nVersion: 0.6.0\nArchitecture: amd64\nMaintainer: Test <test@example.invalid>\nDescription: Offline package fixture\n");
    QProcess build;build.start("/usr/bin/dpkg-deb",{"--build","--root-owner-group",tree,deb});build.waitForFinished(10000);
    error.clear();check(build.exitCode()==0&&imageManager.stageDeb(deb,sha(deb),"0.6.0",staged,error),"verified DEB is staged without installing or invoking authentication");
    check(read(staged)==read(deb),"staged DEB preserves downloaded content");error.clear();check(!imageManager.stageDeb(deb,sha(deb),"9.9.9",staged,error),"DEB version mismatch is refused before administrative action");
  }
  return errors?1:0;
}
