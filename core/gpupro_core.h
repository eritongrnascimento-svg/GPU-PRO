#pragma once
#include <cstdint>
#ifdef _WIN32
#ifdef GPUPRO_EXPORTS
#define GPP_API __declspec(dllexport)
#else
#define GPP_API
#endif
#else
#define GPP_API __attribute__((visibility("default")))
#endif
extern "C" {
struct GppFrameStats { double current,mean,maximum,low; int count,lowAvailable; };
struct GppBalance { double modelFps,gpuPotentialFps,cpuPotentialFps,gpuLossFromCpuPct,cpuLossFromGpuPct,gpuLostFps,cpuLostFps; int valid; };
struct GppCpuResult { double seconds,operationsPerSecond,checksum; std::uint64_t operations; int threads,cancelled; };
GPP_API GppFrameStats gpp_frame_stats(const double *times,int count);
GPP_API GppBalance gpp_balance(double cpuMs,double gpuMs);
GPP_API GppCpuResult gpp_cpu_benchmark(int mode,int threads,int milliseconds);
GPP_API void gpp_cancel_benchmark();
}
