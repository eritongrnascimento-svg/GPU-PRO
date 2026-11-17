#include "codec.h"
#ifdef _WIN32
#include "windows_devices.h"
QProcessEnvironment nativeEnvironment(){return QProcessEnvironment::systemEnvironment();}
#else
#include "driver_packages.h"
#endif
namespace {
int cudaOrdinal(const QString &pci) {
  QLibrary cuda(
#ifdef _WIN32
    "nvcuda.dll"
#else
    "libcuda.so.1"
#endif
  );
  if (!cuda.load())
    return -1;
  auto init = reinterpret_cast<int (*)(unsigned)>(cuda.resolve("cuInit"));
#ifdef _WIN32
  auto count=reinterpret_cast<int(*)(int*)>(cuda.resolve("cuDeviceGetCount"));auto luid=reinterpret_cast<int(*)(char*,unsigned*,int)>(cuda.resolve("cuDeviceGetLuid"));int n=0;if(!init||!count||!luid||init(0)!=0||count(&n)!=0)return -1;for(int i=0;i<n;i++){LUID id{};unsigned mask=0;if(luid(reinterpret_cast<char*>(&id),&mask,i)==0&&WinDevices::luid(id)==pci.toStdString())return i;}return -1;
#else
  auto byPci =
      reinterpret_cast<int (*)(int *, const char *)>(cuda.resolve("cuDeviceGetByPCIBusId"));
  int id = -1;
  if (!init || !byPci || init(0) != 0 || byPci(&id, pci.toLatin1().constData()) != 0)
    return -1;
  return id;
#endif
}
} // namespace
CodecRunner::CodecRunner(QObject *p) : QObject(p) {
  timeout.setSingleShot(true);
  connect(&timeout, &QTimer::timeout, this, [this] {
    cancelled = true;
    errorLog = "Tempo limite excedido";
    process.kill();
  });
  connect(&process, &QProcess::readyReadStandardError, this, [this] {
    errorLog += QString::fromUtf8(process.readAllStandardError());
    if (errorLog.size() > 16000)
      errorLog = errorLog.right(16000);
  });
  connect(&process, &QProcess::readyReadStandardOutput, this, [this] {
    stdoutBuffer += QString::fromUtf8(process.readAllStandardOutput());
    int pos;
    while ((pos = stdoutBuffer.indexOf('\n')) >= 0) {
      QString line = stdoutBuffer.left(pos).trimmed();
      stdoutBuffer.remove(0, pos + 1);
      if (line.startsWith("frame=")) {
        frames = line.mid(6).trimmed().toInt();
        emit progress(stage == "measure" ? qMin(99, frames * 100 / 360) : 0,
                      stage == "measure" ? QString("%1 / 360 quadros").arg(frames)
                                         : "Preparando mídia fora da medição");
      }
    }
  });
  connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          &CodecRunner::finished);
  connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
    if (e == QProcess::FailedToStart)
      fail("Não foi possível iniciar FFmpeg: " + process.errorString());
  });
}
CodecRunner::~CodecRunner() {
  process.disconnect(this);
  process.kill();
  process.waitForFinished(2000);
}
void CodecRunner::run(QStringList args, QString phase) {
  process.setProcessEnvironment(nativeEnvironment());
  stage = phase;
  frames = 0;
  stdoutBuffer.clear();
  errorLog.clear();
  args.prepend("-y");
  args.prepend("-hide_banner");
  args << "-progress" << "pipe:1" << "-nostats";
  runArgs = args;
  timeout.start(180000);
  if (stage == "measure")
    elapsed.start();
  process.start("ffmpeg", args);
}
void CodecRunner::start(const Gpu &g, QString c, QString m) {
  if (busy())
    return;
  gpu = g;
  codec = c;
  mode = m;
  cancelled = false;
  active = true;
  if (!QStringList{"h264", "hevc", "av1"}.contains(c) ||
      !QStringList{"encode", "decode"}.contains(m)) {
    fail("Parâmetros de codec inválidos");
    return;
  }
  if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) {
    fail("FFmpeg não instalado");
    return;
  }
  if (g.vendor != "NVIDIA" && g.render.isEmpty()
#ifdef _WIN32
      && g.vendor!="AMD" && g.vendor!="Intel"
#endif
  ) {
    fail("GPU sem render node VA-API");
    return;
  }
  QProcess version;
  version.start("ffmpeg", {"-version"});
  if (version.waitForFinished(1500))
    ffmpegVersion =
        QString::fromUtf8(version.readAllStandardOutput()).section('\n', 0, 0).left(160);
  else {
    version.kill();
    version.waitForFinished(500);
    ffmpegVersion = "unknown";
  }
  temp = std::make_unique<QTemporaryDir>();
  if (!temp->isValid()) {
    fail("Falha ao criar diretório temporário");
    return;
  }
  emit progress(0, "Preparando 120 quadros 1080p; preparação não pontuada");
  QStringList args = {"-f",        "lavfi", "-i",       "testsrc2=size=1920x1080:rate=60",
                      "-frames:v", "120",   "-pix_fmt", "yuv420p"};
  if (mode == "encode")
    args << "-f" << "rawvideo" << temp->filePath("source.yuv");
  else {
    QString enc = c == "h264" ? "libx264" : c == "hevc" ? "libx265" : "libsvtav1";
    if(c=="av1"){QProcess encoders;encoders.setProcessEnvironment(nativeEnvironment());encoders.start("ffmpeg",{"-hide_banner","-encoders"});if(encoders.waitForFinished(5000)){auto list=QString::fromUtf8(encoders.readAllStandardOutput());if(!list.contains("libsvtav1")&&list.contains("libaom-av1"))enc="libaom-av1";}else{encoders.kill();encoders.waitForFinished(1000);}}
    args << "-c:v" << enc << "-threads" << "4";
    if (c == "hevc")
      args << "-x265-params" << "pools=4:frame-threads=2";
    if (enc == "libsvtav1")
      args << "-svtav1-params" << "lp=4";
    if(enc=="libaom-av1")args<<"-cpu-used"<<"8"<<"-row-mt"<<"1"<<"-crf"<<"45"<<"-b:v"<<"0";
    if (c != "av1")
      args << "-preset" << "ultrafast";
    else if(enc=="libsvtav1")
      args << "-preset" << "12";
    args << temp->filePath("source.mkv");
  }
  run(args, "prepare");
}
void CodecRunner::measured() {
  if (!active)
    return;
  if (cancelled) {
    fail("Teste cancelado pelo usuário");
    return;
  }
  QStringList args;
  QString hw = gpu.vendor == "NVIDIA" ? "cuda" : "vaapi";
#ifdef _WIN32
  if(gpu.vendor!="NVIDIA")hw="d3d11va";
#endif
  QString device = gpu.render;
#ifdef _WIN32
  if(gpu.vendor!="NVIDIA")device=gpu.sys;
#endif
  if (hw == "cuda") {
    int idx = cudaOrdinal(gpu.pci);
    if (idx < 0) {
      fail("CUDA não identificou a GPU PCI selecionada");
      return;
    }
    device = QString::number(idx);
  }
  QString encoderSuffix=hw=="cuda"?"_nvenc":"_vaapi";
#ifdef _WIN32
  if(gpu.vendor=="AMD")encoderSuffix="_amf";
  if(gpu.vendor=="Intel")encoderSuffix="_qsv";
  if(gpu.vendor=="Intel"&&mode=="encode"){args<<"-init_hw_device"<<"d3d11va=base:"+device<<"-init_hw_device"<<"qsv=bench@base"<<"-filter_hw_device"<<"bench";}else
#endif
  args << "-init_hw_device" << hw + "=bench:" + device << "-filter_hw_device"
       << "bench";
  if (mode == "encode") {
    args << "-stream_loop" << "2" << "-f" << "rawvideo" << "-pixel_format"
         << "yuv420p" << "-video_size" << "1920x1080" << "-framerate" << "60"
         << "-i" << temp->filePath("source.yuv") << "-vf"
         << "format=nv12,hwupload" << "-frames:v" << "360" << "-c:v"
         << codec + encoderSuffix << "-b:v" << "12M"
         << temp->filePath("encoded.mkv");
  } else {
    // Hardware output frames are mandatory; a software fallback cannot satisfy
    // this pipeline.
    args << "-hwaccel" << hw << "-hwaccel_device" << "bench"
         << "-hwaccel_output_format" << hw << "-stream_loop" << "2" << "-i"
         << temp->filePath("source.mkv") << "-vf" << "hwdownload,format=nv12" << "-frames:v"
         << "360" << "-an"
         << "-f" << "null" << "-";
  }
  emit progress(0, "Medindo pipeline por hardware na GPU " + gpu.pci);
  run(args, "measure");
}
void CodecRunner::finished(int code, QProcess::ExitStatus status) {
  timeout.stop();
  if (cancelled) {
    fail(errorLog.isEmpty() ? "Teste cancelado" : errorLog);
    return;
  }
  if (code != 0 || status != QProcess::NormalExit) {
    fail("FFmpeg recusou a operação de hardware ou o codec:\n" + errorLog);
    return;
  }
  if (stage == "prepare") {
    QTimer::singleShot(0, this, &CodecRunner::measured);
    return;
  }
  double seconds = elapsed.nsecsElapsed() / 1e9;
  if (frames != 360 || seconds <= 0) {
    fail("Contagem de quadros incompleta; resultado descartado");
    return;
  }
  QJsonObject r{{"valid", true},
                {"mode", mode},
                {"codec", codec},
                {"pci", gpu.pci},
                {"gpu", gpu.name},
                {"frames", frames},
                {"seconds", seconds},
                {"fps", frames / seconds},
                {"resolution", "1920x1080"},
                {"bitrate", mode == "encode" ? "12M" : "fixture CPU"},
                {"method", "codec-pipeline-v0.2"},
                {"ffmpegVersion", ffmpegVersion},
                {"arguments", QJsonArray::fromStringList(runArgs)},
                {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
  active = false;
  temp.reset();
  emit progress(100, "Concluído");
  emit result(r);
}
void CodecRunner::fail(QString message) {
  active = false;
  timeout.stop();
  temp.reset();
  emit result({{"valid", false}, {"error", message}});
}
void CodecRunner::cancel() {
  cancelled = true;
  errorLog = "Teste cancelado pelo usuário";
  if (process.state() != QProcess::NotRunning)
    process.kill();
  else if (active)
    fail(errorLog);
}
