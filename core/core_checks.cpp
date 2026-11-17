#include "gpupro_core.h"
#include <cmath>
#include <cstdio>
#include <limits>
int main(){
 int errors=0;auto check=[&](bool ok,const char *s){if(!ok){++errors;std::fprintf(stderr,"FAIL %s\n",s);}};
 auto cpu=gpp_balance(10,5);check(cpu.valid&&cpu.gpuLossFromCpuPct==50&&cpu.gpuLostFps==100&&cpu.cpuLostFps==0,"CPU-bound loss is measured against GPU potential");
 auto gpu=gpp_balance(5,10);check(gpu.cpuLossFromGpuPct==50&&gpu.cpuLostFps==100&&gpu.gpuLostFps==0,"GPU-bound loss is measured against CPU potential");
 check(!gpp_balance(0,10).valid&&!gpp_balance(NAN,10).valid,"invalid timing rejected");
 double v[100];for(auto &n:v)n=10;v[99]=50;auto f=gpp_frame_stats(v,100);check(f.count==100&&f.low==20&&f.maximum==100&&f.lowAvailable,"frame durations and low-tail");
 for(int mode=0;mode<3;++mode){auto r=gpp_cpu_benchmark(mode,1,60);check(r.operations>0&&r.operationsPerSecond>0&&std::isfinite(r.checksum)&&r.threads==1,"real CPU workload returns validated throughput");}
 return errors?1:0;
}
