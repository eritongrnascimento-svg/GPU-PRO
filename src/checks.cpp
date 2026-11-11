#include "gpu.h"
#include "driver_stack.h"
#include "intel_specs.h"
#include "system_settings.h"
#include <i915_drm.h>
#include "cpu_telemetry.h"
#include "frame_log.h"
#include "driver_packages.h"
#include "repository.h"
#include <cmath>
#include <cstdio>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  int failures = 0;
  auto check = [&](bool ok, const char *message) {
    if (!ok) {
      fprintf(stderr, "FAIL %s\n", message);
      ++failures;
    }
  };
  QTemporaryDir cpuFixture;
  auto write=[&](QString path,QByteArray data) {QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);check(f.open(QIODevice::WriteOnly),"open telemetry fixture");f.write(data);};
  QString proc=cpuFixture.filePath("stat"), sys=cpuFixture.filePath("sys");
  write(proc,"cpu 100 0 100 800 0 0 0 0 50 0\n");
  write(sys+"/hwmon/hwmon0/name","coretemp");
  write(sys+"/hwmon/hwmon0/temp1_label","Package id 0");
  write(sys+"/hwmon/hwmon0/temp1_input","55000");
  CpuTelemetry cpu;
  auto first=cpu.sample(proc,sys);
  check(!first.contains("cpuUsage") && first["cpuTemp"].toDouble()==55,"CPU first sample does not invent utilization; package temperature in Celsius");
  write(proc,"cpu 125 0 125 850 0 0 0 0 80 0\n");
  auto second=cpu.sample(proc,sys);
  check(second["cpuUsage"].toDouble()==50,"CPU deltas exclude guest double counting");
  write(sys+"/hwmon/hwmon0/temp1_input","not-a-temperature");
  check(!cpu.sample(proc,sys).contains("cpuTemp"),"missing CPU temperature stays unavailable");
  QString csv=cpuFixture.filePath("frames.csv");
  write(csv,"OS,CPU,GPU\nLinux,CPU,GPU\nfps,frametime,cpu_load\n60,16.67,20\n30,33.");
  FrameLog frames;
  auto initial=frames.poll(csv);
  check(initial.size()==1 && initial[0].toDouble()==16.67,"CSV header detection and incomplete row handling");
  {QFile f(csv);check(f.open(QIODevice::Append),"append frame log fixture");f.write("33,30\n60,nan,20\n60,-1,20\n");}
  auto appended=frames.poll(csv);
  check(appended.size()==1 && appended[0].toDouble()==33.33,"CSV live append accepts only real finite positive frame times");
  check(frames.poll(csv).isEmpty(),"CSV samples never duplicate when no frames arrive");
  write(csv,"fps,frametime\n100,10\n");
  check(frames.poll(csv).size()==1,"CSV truncation resets reader and header");
  QJsonArray repositoryPackages{
    QJsonObject{{"manager","pacman"},{"name","linux-cachyos-nvidia-open"},{"version","7.3.3-1"},{"repo","native"},{"depends",QJsonArray{"nvidia-utils=620.70.06"}}},
    QJsonObject{{"manager","pacman"},{"name","nvidia-utils"},{"version","620.70.06-2"},{"repo","native"}},
    QJsonObject{{"manager","pacman"},{"name","nvidia-container-toolkit"},{"version","1.0"},{"repo","native"}},
    QJsonObject{{"manager","pacman"},{"name","linux-other-nvidia-open"},{"version","7.3.3-1"},{"repo","native"},{"depends",QJsonArray{"nvidia-utils=620.70.06"}}}
  };
  auto bundles=completeDriverProfiles(repositoryPackages,QJsonArray{QJsonObject{{"name","linux-cachyos"},{"version","7.3.3-1"}}},"NVIDIA");
  check(bundles.size()==1&&bundles[0].toObject()["targets"].toArray().size()==2,"complete drivers pair module and userspace; exclude containers and uninstalled kernels");
  repositoryPackages.removeAt(1);check(completeDriverProfiles(repositoryPackages,QJsonArray{QJsonObject{{"name","linux-cachyos"}}},"NVIDIA").isEmpty(),"a module without matching userspace is not offered as a complete driver");
  QByteArray topology(sizeof(drm_i915_query_topology_info)+4,0);auto *t=reinterpret_cast<drm_i915_query_topology_info*>(topology.data());
  t->max_slices=1;t->max_subslices=1;t->max_eus_per_subslice=8;t->subslice_offset=1;t->subslice_stride=1;t->eu_offset=2;t->eu_stride=1;t->data[0]=1;t->data[1]=1;t->data[2]=0x3f;
  check(parseIntelTopology(topology)["executionUnits"].toInt()==6,"Intel counts enabled execution units, not CUDA-equivalent guesses");
  check(parseIntelTopology(QByteArray(2,0)).isEmpty(),"truncated Intel topology is rejected");
  auto audio=parseAudioInventory(QJsonArray{QJsonObject{{"name","hdmi"},{"description","Monitor"},{"properties",QJsonObject{{"device.api","alsa"}}}}},QJsonArray{QJsonObject{{"name","hdmi.monitor"}},QJsonObject{{"name","filter-input"},{"properties",QJsonObject{}}}},"Default Sink: hdmi\nDefault Source: filter-input\n","HDA NVIDIA");
  check(audio["outputs"].toArray().size()==1&&audio["inputs"].toArray().size()==1&&audio["inputs"].toArray()[0].toObject()["virtual"].toBool(),"audio distinguishes virtual input from physical microphone and excludes monitor taps");
  check(audio["output"]=="hdmi"&&audio["input"]=="filter-input","audio default destinations parsed without renumbering ALSA cards");
  QString table="Package (2)            Old Version  New Version  Net Change\n\nlegacy                 1.0-1                     -1.0 MiB\nrepo/driver                        2.0-1         1.0 MiB\n\nTotal Installed Size: 1.0 MiB\n";
  // Fixed columns are derived from the native header; unknown layouts fail closed.
  int oc=table.indexOf("Old Version"),nc=table.indexOf("New Version"),net=table.indexOf("Net Change");
  QString header=table.section('\n',0,0),oldRow=QString("legacy").leftJustified(oc)+QString("1.0-1").leftJustified(nc-oc)+QString().leftJustified(net-nc)+"-1.0 MiB",newRow=QString("repo/driver").leftJustified(oc)+QString().leftJustified(nc-oc)+QString("2.0-1").leftJustified(net-nc)+"1.0 MiB";
  auto parsedPlan=parsePacmanPreview(header+'\n'+oldRow+'\n'+newRow+"\nTotal Installed Size: 1.0 MiB\n",QJsonArray{QJsonObject{{"name","legacy"},{"version","1.0-1"}}});
  check(parsedPlan["valid"].toBool()&&parsedPlan["add"].toArray().size()==1&&parsedPlan["remove"].toArray().size()==1,"native transaction preview accounts for both additions and removals");
  check(!parsePacmanPreview(header+'\n'+newRow,QJsonArray{} )["valid"].toBool(),"incomplete native package table never enables installation");
  QTemporaryDir preference;auto oldDesktop=qgetenv("XDG_CURRENT_DESKTOP"),oldSession=qgetenv("XDG_SESSION_TYPE");qputenv("XDG_CURRENT_DESKTOP","KDE");qputenv("XDG_SESSION_TYPE","wayland");
  Gpu intel;intel.pci="0000:00:02.0";Gpu nv;nv.pci="0000:01:00.0";
  auto gpuPlan=gpuSessionPlan({intel,nv},nv.pci,preference.path());
  check(gpuPlan["valid"].toBool()&&gpuPlan["links"].toArray().size()==2&&gpuPlan["content"].toString().contains("KWIN_DRM_DEVICES"),"session plan preserves both GPUs and uses stable aliases");
  check(!QFileInfo::exists(gpuPlan["target"].toString()),"building a GPU preference plan never applies it");
  auto oldConfig=qgetenv("XDG_CONFIG_HOME");qputenv("XDG_CONFIG_HOME",preference.path().toUtf8());
  auto fixturePlan=gpuPlan;QJsonArray links;
  for(auto v:gpuPlan["links"].toArray()){auto link=v.toObject();QString fake=preference.filePath(link["path"].toString().section('/',-1)+"-device");write(fake,"fake DRM endpoint for filesystem test");link["target"]=fake;links.append(link);}fixturePlan["links"]=links;
  QString backup=preference.filePath("backup");
  check(applyGpuSession(fixturePlan,backup)["valid"].toBool(),"GPU session preference writes only reviewed user configuration with backup");
  check(restoreGpuSession(backup)["valid"].toBool()&&!QFileInfo::exists(gpuPlan["target"].toString()),"GPU session undo restores absence of a previous preference");
  write(gpuPlan["target"].toString(),"user-owned unrelated script");
  check(!applyGpuSession(fixturePlan,backup)["valid"].toBool(),"GPU session setup preserves an unrelated existing user file");
  qputenv("XDG_CONFIG_HOME",oldConfig);
  qputenv("XDG_CURRENT_DESKTOP",oldDesktop);qputenv("XDG_SESSION_TYPE",oldSession);
  check(displayMetric({}, "temp") == "N/D", "missing sensor must not be zero");
  check(displayMetric({{"temp", 0}}, "temp") == "0 °C", "zero is a valid reading");
  auto stage = [](double cpu, double gpu) {return QJsonObject{{"valid",true},{"width",1920},{"height",1080},{"sceneResults",QJsonArray{QJsonObject{{"cpuWorkMs",cpu},{"gpuMeanMs",gpu}}}}};};
  auto cpuBound = estimateBottleneck(stage(10,5),stage(10,5));
  check(cpuBound["cpuLossPct"].toDouble()==50 && cpuBound["gpuLossPct"].toDouble()==0,"CPU estimate references GPU potential");
  auto gpuBound = estimateBottleneck(stage(5,10),stage(5,10));
  check(gpuBound["gpuLossPct"].toDouble()==50 && gpuBound["cpuLossPct"].toDouble()==0,"GPU estimate references CPU potential");
  check(estimateBottleneck(stage(5,5),stage(5,5))["cpuLossPct"].toDouble()==0,"balanced synthetic stage");
  check(!estimateBottleneck(stage(0,5),stage(5,5))["valid"].toBool(),"missing CPU time must not produce percentage");
  auto arch = parseDriverPackages("pacman","extra nvidia-utils 615.78.08-2 [installed: 620.70.06-2]\ncustom nvidia-utils 620.70.06-2 [installed]\nextra bash 5.2\n", "NVIDIA");
  check(arch.size()==2 && arch[0].toObject()["installed"]=="620.70.06-2","read available versions without inventing repository history");
  auto command = driverTransaction(arch[0].toObject(),false);
  check(command.contains("-Syu") && command.last()=="extra/nvidia-utils=615.78.08-2" && !command.contains("--noconfirm"),"Arch driver update pins version and preserves full upgrade/native confirmation");
  check(driverTransaction({{"manager","apt"},{"name","bash"},{"version","5.2"}},false).isEmpty(),"driver actions cannot target arbitrary package");
  check(driverTransaction({{"manager","apt"},{"name","nvidia-driver-580"},{"version","580;touch /tmp/file"}},false).isEmpty(),"reject malformed package version");
  auto apt=parseDriverPackages("apt","nvidia-driver-580 | 580.1-0ubuntu1 | http://archive.ubuntu.com/ubuntu noble/restricted amd64 Packages\n","NVIDIA");
  check(apt.size()==1 && driverTransaction(apt[0].toObject(),true).contains("--simulate"),"APT versions and simulation");
  QJsonObject generic{{"manager","packagekit"},{"name","nvidia-drivers"},{"version","580.1"},{"packageId","nvidia-drivers;580.1;x86_64;native-repo"}};
  check(driverTransaction(generic,false).last()=="nvidia-drivers;580.1;x86_64;native-repo","PackageKit pins native package ID and repository");
  generic["packageId"]="bash;580.1;x86_64;native-repo";
  check(driverTransaction(generic,false).isEmpty(),"PackageKit rejects unrelated package IDs");
  auto c = OverlayConfig::fromJson({{"font", 999},
                                    {"width", 0},
                                    {"opacity", -1},
                                    {"layout", "invalid"},
                                    {"fields", QJsonArray{"temp", "temp", "not-a-sensor"}}});
  check(c.font == 40 && c.width == 220 && c.opacity == 0 && c.layout == "list", "config bounds");
  check(c.fields == QStringList{"temp"}, "field allowlist and deduplication");
  check(OverlayConfig::fromJson(c.json()).json() == c.json(), "overlay settings roundtrip");
  OverlayConfig geo;
  QRect screen(-1920, 0, 1920, 1080);
  geo.anchor = "bottom-right";
  auto rect = overlayRect(geo, screen);
  check(rect.right() == -25 && rect.bottom() == 1055,
        "anchor on a monitor with negative coordinates");
  geo.x = 50000;
  geo.y = -50000;
  rect = overlayRect(geo, screen);
  check(screen.contains(rect), "HUD is clamped to the monitor");
  geo.layout = "compact";
  geo.width = 1600;
  QRect small(0, 0, 640, 480);
  check(small.contains(overlayRect(geo, small)), "compact HUD fits a small monitor");
  auto s = summarize({10, 10, 20, 0, -2, std::numeric_limits<double>::quiet_NaN()});
  check(s.frames == 3 && std::abs(s.mean - 75) < .001 && s.p99 == 20 && s.low == 50,
        "frametime statistics use frame durations and reject invalid samples");
  check(summarize({}).frames == 0, "no samples means no score");
  auto amd =
      parseAmdOverdrive("OD_SCLK:\n0: 500Mhz\n1: 2500Mhz\nOD_MCLK:\n1: 1000Mhz\nOD_RANGE:\nSCLK: "
                        "500Mhz 3000Mhz\nMCLK: 600Mhz 1200Mhz\n");
  check(amd["coreMin"].toInt() == 500 && amd["coreMax"].toInt() == 3000 &&
            amd["coreCurrent"].toInt() == 2500 && amd["memMin"].toInt() == 600,
        "modern AMD clock range parser");
  check(parseAmdOverdrive("OD_SCLK:\n0: 500Mhz 800mV\n1: 2000Mhz "
                          "1000mV\nOD_RANGE:\nSCLK: 500Mhz 3000Mhz")
            .isEmpty(),
        "legacy voltage tables must not use modern clock commands");
  check(parseAmdOverdrive("OD_SCLK:\n1: 2500Mhz").isEmpty(),
        "missing clock ranges disables writes");
  auto centered =
      OverlayConfig::fromJson({{"anchor", "top-center"}, {"width", 360}, {"x", 0}, {"y", 20}});
  QRect monitor(100, -1080, 1920, 1080);
  auto top = overlayRect(centered, monitor);
  check(centered.anchor == "top-center" && top.x() == 880 && top.y() == -1060,
        "top-center anchor and vertical offset on negative monitor");
  QTemporaryDir dir;
  QString hw = dir.filePath("device/hwmon/hwmon0/");
  QDir().mkpath(hw);
  auto sensorFile = [&](QString name, QByteArray data) { QFile file(hw + name); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size(); };
  check(sensorFile("temp1_label", "edge") && sensorFile("temp1_input", "46000") &&
        sensorFile("temp2_label", "junction") && sensorFile("temp2_input", "67000") &&
        sensorFile("temp3_label", "memory junction") && sensorFile("temp3_input", "78000"), "fixture hwmon");
  Gpu fixture; fixture.vendor = "AMD"; fixture.sys = dir.filePath("device"); fixture.pci = "0000:ff:00.0";
  GpuBackend sensors;
  auto readings = sensors.sample(fixture);
  check(readings["temp"].toDouble() == 46 && readings["hotspot"].toDouble() == 67 && readings["memoryTemp"].toDouble() == 78,
        "memory junction must not overwrite GPU hotspot");
  QFile::remove(hw + "temp2_input");
  check(!sensors.sample(fixture).contains("hotspot"), "missing hotspot must remain unavailable even with memory junction");
  QString err;
  QJsonObject obj{{"message", "GPU · teste"}};
  check(saveJson(dir.filePath("deep/test.json"), obj, &err), "atomic save");
  check(QJsonDocument::fromJson(readText(dir.filePath("deep/test.json")).toUtf8()).object() == obj,
        "JSON roundtrip");
  {
    Repository database(dir.filePath("tests.sqlite"));
    check(database.error.isEmpty(), "SQLite schema creation");
    QStringList ids;
    for (int i = 0; i < 5; ++i) {
      QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      ids << id;
      check(database.add("graphics", "test-device",
                         {{"id", id},
                          {"valid", true},
                          {"score", i + 1},
                          {"api", "Vulkan"},
                          {"timestamp", QString("2026-11-10T10:00:0%1Z").arg(i)},
                          {"rawLog", "/home/private-user/game"},
                          {"uuid", "GPU-PRIVATE"},
                          {"gpu", QJsonObject{{"name", "Example GPU"},
                                              {"deviceId", "0x0001"},
                                              {"vendor", "NVIDIA"},
                                              {"uuid", "PRIVATE"},
                                              {"sys", "/home/private"}}}}),
            "persist test");
    }
    auto history = database.recent("graphics", "test-device");
    check(history.size() == 3 && history[0].toObject()["score"].toInt() == 5 &&
              history[2].toObject()["score"].toInt() == 3,
          "three newest tests per GPU/category");
    auto feedback = database.feedback();
    QString error;
    check(validFeedback(feedback, error), "exported feedback validates");
    auto bytes = QJsonDocument(feedback).toJson();
    check(!bytes.contains("PRIVATE") && !bytes.contains("private-user") &&
              !bytes.contains("rawLog") && !bytes.contains("/home/"),
          "feedback strips identity and raw logs");
    check(database.importFeedback(feedback, error) == 0,
          "existing IDs cannot be imported over local records");
    Repository other(dir.filePath("friend.sqlite"));
    check(other.importFeedback(feedback, error) == 5 && other.importFeedback(feedback, error) == 0,
          "import validation and deduplication");
    check(other.recent("graphics", "test-device").isEmpty(),
          "imported results never become local verified history");
    feedback["schema"] = "unknown";
    check(other.importFeedback(feedback, error) == -1, "unknown schema rejected");
  }
  auto clean = sanitizeResult(
      {{"unit", "GB/s"},
       {"gpu", QJsonObject{{"name", "GPU"}, {"uuid", "secret"}, {"serial", "secret"}}},
       {"telemetry", QJsonArray{QJsonObject{
                         {"temp", 45}, {"processName", "private-game"}, {"processPid", 123}}}}});
  check(clean["unit"].toString() == "GB/s" &&
            !QJsonDocument(clean).toJson().contains("private-game"),
        "units retained, process identifiers removed");
  printf("Offline checks: %s\n", failures ? "FAILED" : "PASS");
  return failures ? 1 : 0;
}
