#include "gpu.h"
#include "frame_log.h"
#include <cmath>
#include <cstdio>
int main(int argc,char **argv) {
  QCoreApplication app(argc,argv);int errors=0;
  auto check=[&](bool ok,const char *message){if(!ok){++errors;fprintf(stderr,"FAIL %s\n",message);}};
  auto put=[&](QString p,QByteArray b){QDir().mkpath(QFileInfo(p).absolutePath());QFile f(p);check(f.open(QIODevice::WriteOnly),"fixture open");check(f.write(b)==b.size(),"fixture write");};
  QJsonArray times;for(int i=0;i<99;++i)times.append(10.);times.append(50.);
  const auto stats=frameMetrics(times);
  check(std::abs(stats["fps"].toDouble()-20.)<.0001,"current FPS uses latest sample");
  check(std::abs(stats["fpsAvg"].toDouble()-100000./1040.)<.0001,"mean FPS uses durations rather than arithmetic mean of rates");
  check(stats["fpsMax"].toDouble()==100.&&stats["fpsLow"].toDouble()==20.,"maximum and slowest one percent");
  check(!frameMetrics(QJsonArray{10.,20.}).contains("fpsLow"),"insufficient low samples stay unavailable");
  check(frameMetrics(QJsonArray{0.,-1.,QJsonValue(),QString("NaN")}).isEmpty(),"invalid samples never become FPS");
  QTemporaryDir tmp;QString csv=tmp.filePath("frames.csv");FrameLog reader;
  put(csv,"fps,frametime\n100,10\n50,20\n");auto first=reader.poll(csv);
  check(reader.resetDetected&&first.size()==2,"CSV source starts a fresh series");
  check(reader.poll(csv).isEmpty()&&!reader.resetDetected,"CSV does not duplicate rows");
  {QFile f(csv);check(f.open(QIODevice::Append),"append");f.write("fps,frametime\n25,40\n");}
  auto next=reader.poll(csv);check(reader.resetDetected&&next.size()==1&&next[0].toDouble()==40.,"repeated CSV header resets series");
  put(csv,"\"fps\",\"frametime\"\n100,\"10\"\n50,\"20");reader.reset();
  auto quoted=reader.poll(csv);check(quoted.size()==1&&quoted[0].toDouble()==10.,"quoted fields and partial lines");
  {QFile f(csv);check(f.open(QIODevice::Append),"append incomplete");f.write("\"\n");}
  check(reader.poll(csv).size()==1,"partial CSV completed once");
  reader.poll("");check(reader.resetDetected,"removed CSV resets active series");
  const QString od="OD_VDDGFX_OFFSET:\n-25mV\nOD_RANGE:\nVDDGFX_OFFSET: -100mV 50mV\n";
  const auto offset=amdVoltageOffset(od);
  check(offset["current"].toInt()==-25&&offset["min"].toInt()==-100&&offset["max"].toInt()==50,"independent AMD voltage offset range");
  check(amdVoltageOffset("OD_VDDC_CURVE:\n0: 500MHz 800mV\n").isEmpty(),"legacy voltage tables are not treated as offsets");
  check(amdVoltageOffset("OD_VDDGFX_OFFSET:\n-200mV\nVDDGFX_OFFSET: -100mV 50mV\n").isEmpty(),"reject offset outside reported range");
  Gpu gpu;gpu.vendor="AMD";gpu.driver="amdgpu";gpu.sys=tmp.filePath("device");
  QString hw=gpu.sys+"/hwmon/hwmon0/";
  put(hw+"name","amdgpu");put(hw+"power1_cap_min","50000000");put(hw+"power1_cap_max","300000000");put(hw+"power1_cap","220000000");
  put(hw+"pwm1","128");put(hw+"pwm1_enable","2");put(hw+"pwm1_min","64");put(hw+"pwm1_max","255");
  put(gpu.sys+"/pp_od_clk_voltage",od.toUtf8());Nvml nv;
  auto caps=gpuTuningCaps(gpu,nv);
  check(caps["power"].toObject()["min"].toInt()==50&&caps["power"].toObject()["current"].toInt()==220,"power cap microW to W");
  check(caps["fan0"].toObject()["min"].toInt()==26&&caps["fan0"].toObject()["automatic"].toBool(),"fan bounds and automatic mode");
  check(caps["voltageOffset"].toObject()["current"].toInt()==-25,"AMD voltage capability");
  QString error;
  check(!setGpuTuning(gpu,nv,"power",301,error)&&readText(hw+"power1_cap")=="220000000","out of range power performs no write");
  check(setGpuTuning(gpu,nv,"power",150,error)&&readText(hw+"power1_cap")=="150000000","manual watts are written as microW and verified");
  check(!setGpuTuning(gpu,nv,"fan0",25,error)&&readText(hw+"pwm1_enable")=="2","fan duty below driver minimum is refused");
  check(setGpuTuning(gpu,nv,"fan0",50,error)&&readText(hw+"pwm1")=="128"&&readText(hw+"pwm1_enable")=="1","manual fan duty and policy verified");
  check(setGpuTuning(gpu,nv,"fan0",-1,error)&&readText(hw+"pwm1_enable")=="2","automatic cooling restoration");
  check(!setGpuTuning(gpu,nv,"voltageOffset",-101,error)&&readText(gpu.sys+"/pp_od_clk_voltage")==od.trimmed(),"voltage bounds refused before any write");
  check(!setGpuTuning(gpu,nv,"arbitraryPath",1,error),"unrecognized tuning rejected");
  gpu.vendor="Intel";check(gpuTuningCaps(gpu,nv).isEmpty(),"Intel does not inherit AMD writes");
  fprintf(stdout,"Tuning and frame checks: %d failure(s)\n",errors);return errors?1:0;
}
