#include "kernels.h"
#include "cpu_flags.h"
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

template<class T>
void benchmark(int depth, bool blend = false)
{
  median::PlaneJob job{};
  job.width = 1920; job.height = 1080; job.end_y = job.height;
  job.component_size = sizeof(T); job.components = 1;
  job.depth = depth; job.low = job.high = blend ? 1 : depth / 2;
  job.dst_pitch = job.width * sizeof(T);
  std::vector<T> out(job.width * job.height);
  std::vector<std::vector<T>> sources(depth, out);
  std::mt19937 rng(123);
  for (int i = 0; i < depth; ++i) {
    for (auto& v : sources[i]) v = sizeof(T) == 4 ? static_cast<T>((rng() % 4096) / 4096.0) : static_cast<T>(rng());
    job.src[i] = reinterpret_cast<const uint8_t*>(sources[i].data());
    job.src_pitch[i] = job.dst_pitch;
  }
  job.dst = reinterpret_cast<uint8_t*>(out.data());
  double scalar = 0;
  for (int opt : {1, 0}) {
    job.kernel = median::select_kernel(host_cpu_flags(), sizeof(T), opt);
    median::process_plane(job);
    auto start = std::chrono::steady_clock::now();
    for (int n = 0; n < 10; ++n) median::process_plane(job);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 10;
    if (opt == 1) scalar = ms;
    std::cout << sizeof(T) * 8 << " bit, " << depth << (blend ? " clips, blended, opt=" : " clips, opt=") << opt << ": " << ms << " ms/frame";
    if (opt == 0) std::cout << ", " << scalar / ms << "x versus scalar";
    std::cout << ", sample=" << +out[100] << '\n';
  }
}
int main()
{
  for (int depth : {3, 5, 9}) { benchmark<uint8_t>(depth); benchmark<uint16_t>(depth); benchmark<float>(depth); }
  benchmark<float>(5, true);
  benchmark<float>(25, true);
}
