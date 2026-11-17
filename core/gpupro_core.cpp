#include "gpupro_core.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>
#include <limits>
namespace { std::atomic_bool cancel{false}; }
GppFrameStats gpp_frame_stats(const double *values,int count) {
 GppFrameStats r{};if(!values||count<1||count>1000000)return r;
 std::vector<double> v;v.reserve(count);double sum=0;
 for(int i=0;i<count;++i)if(std::isfinite(values[i])&&values[i]>0&&values[i]<=10000){v.push_back(values[i]);sum+=values[i];}
 if(v.empty())return r;r.count=int(v.size());r.current=1000./v.back();r.mean=1000.*v.size()/sum;
 std::sort(v.begin(),v.end());r.maximum=1000./v.front();
 if(v.size()>=100){const int n=std::max(1,int(std::ceil(v.size()*.01)));double tail=0;for(int i=0;i<n;++i)tail+=v[v.size()-1-i];r.low=1000.*n/tail;r.lowAvailable=1;}return r;
}
GppBalance gpp_balance(double cpu,double gpu) {
 GppBalance r{};if(!std::isfinite(cpu)||!std::isfinite(gpu)||cpu<=0||gpu<=0)return r;
 r.modelFps=1000./std::max(cpu,gpu);r.gpuPotentialFps=1000./gpu;r.cpuPotentialFps=1000./cpu;
 r.gpuLostFps=std::max(0.,r.gpuPotentialFps-r.modelFps);r.cpuLostFps=std::max(0.,r.cpuPotentialFps-r.modelFps);
 r.gpuLossFromCpuPct=100.*r.gpuLostFps/r.gpuPotentialFps;r.cpuLossFromGpuPct=100.*r.cpuLostFps/r.cpuPotentialFps;r.valid=1;return r;
}
void gpp_cancel_benchmark(){cancel=true;}
GppCpuResult gpp_cpu_benchmark(int mode,int threads,int milliseconds) {
 GppCpuResult result{};if(mode<0||mode>2)return result;
 threads=std::clamp(threads,1,int(std::max(1u,std::thread::hardware_concurrency())));
 milliseconds=std::clamp(milliseconds,50,30000);cancel=false;
 using Clock=std::chrono::steady_clock;std::atomic_bool ready{false};
 std::vector<std::uint64_t> counts(threads);std::vector<double> checks(threads);std::vector<std::thread> workers;
 Clock::time_point deadline;
 for(int t=0;t<threads;++t)workers.emplace_back([&,t]{
  while(!ready.load(std::memory_order_acquire))std::this_thread::yield();
  std::uint64_t x=0x9e3779b97f4a7c15ULL+unsigned(t),n=0;double d=1.234+double(t)*.001,answer=0;
  while(!cancel.load(std::memory_order_relaxed)&&Clock::now()<deadline) {
   for(int i=0;i<1024;++i) {
    if(mode==0){x^=x>>12;x^=x<<25;x^=x>>27;x*=0x2545F4914F6CDD1DULL;}
    else if(mode==1){d=std::sqrt(d+1.0000001)+std::sin(d)*.01;}
    else {const unsigned p=100003+unsigned((n+i)%20000);bool prime=true;for(unsigned f=2;f*f<=p;++f)if(p%f==0){prime=false;break;}answer+=prime?1.:0.;}
   }
   n+=1024;
  }
  counts[t]=n;checks[t]=mode==0?double(x&0x1fffffffffffffULL):mode==1?d:answer;
 });
 const auto start=Clock::now();deadline=start+std::chrono::milliseconds(milliseconds);ready.store(true,std::memory_order_release);
 for(auto &thread:workers)thread.join();
 result.seconds=std::chrono::duration<double>(Clock::now()-start).count();result.threads=threads;result.cancelled=cancel?1:0;
 for(int i=0;i<threads;++i){result.operations+=counts[i];result.checksum+=checks[i];}
 result.operationsPerSecond=result.seconds>0?double(result.operations)/result.seconds:0;return result;
}
