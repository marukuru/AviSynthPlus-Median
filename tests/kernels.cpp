#include "kernels.h"
#include "cpu_flags.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
template<typename T>
void check(int depth, int components, int low, int high, bool shifted, bool copy, int opt, int pixels)
{
  median::PlaneJob job{};
  job.width = pixels * components;
  job.kernel = median::select_kernel(host_cpu_flags(), sizeof(T), opt);
  job.height = 7;
  job.component_size = sizeof(T);
  job.components = components;
  job.depth = depth;
  job.low = low;
  job.high = high;
  job.copy_every = copy && (components == 2 || components == 4) ? components : 0;
  job.start_y = 0;
  job.end_y = job.height;
  job.dst_pitch = (job.width + 19) * sizeof(T);
  std::vector<std::vector<T>> source(depth);
  std::vector<T> output((job.width + 19) * job.height + 16, T(42));
  job.dst = reinterpret_cast<uint8_t*>(output.data());
  std::mt19937 rng(42);
  for (int i = 0; i < depth; ++i) {
    const int stride = job.width + 3 + i * 7;
    source[i].resize(stride * job.height);
    for (auto& value : source[i])
      value = sizeof(T) == 4 ? static_cast<T>((int(rng() % 4096) - 2048) / 2048.0) : static_cast<T>(rng());
    job.src[i] = reinterpret_cast<const uint8_t*>(source[i].data());
    job.src_pitch[i] = stride * sizeof(T);
    job.dx[i] = shifted && i ? i % 7 - 3 : 0;
    job.dy[i] = shifted && i ? i % 3 - 1 : 0;
  }
  median::process_plane(job);
  for (int y = 0; y < job.height; ++y) {
    for (int x = 0; x < job.width; ++x) {
      std::vector<double> values;
      for (int i = 0; i < depth; ++i) {
        const int sx = std::clamp(x / components - job.dx[i], 0, pixels - 1) * components + x % components;
        const int sy = std::clamp(y - job.dy[i], 0, job.height - 1);
        values.push_back(source[i][sy * (job.src_pitch[i] / sizeof(T)) + sx]);
      }
      double expected = values[0];
      if (!job.copy_every || (x + 1) % job.copy_every) {
        if (low || high) std::sort(values.begin(), values.end());
        expected = 0;
        for (int i = low; i < depth - high; ++i) expected += values[i];
        expected /= depth - low - high;
        if (sizeof(T) != 4) expected = std::floor(expected);
      }
      if (std::abs(output[y * (job.width + 19) + x] - expected) > 0.00000012)
        throw std::runtime_error("Kernel differs from independent pixel reference");
    }
    for (int x = job.width; x < job.width + 19; ++x)
      if (output[y * (job.width + 19) + x] != T(42)) throw std::runtime_error("Row padding overwritten");
  }
  for (size_t x = (job.width + 19) * job.height; x < output.size(); ++x)
    if (output[x] != T(42)) throw std::runtime_error("Frame guard overwritten");
}
}
int main()
{
  try {
    if (median::supports_opt(0, 2) || median::supports_opt(CPUF_AVX512F, 8) || median::supports_opt(CPUF_AVX2, 6))
      throw std::runtime_error("CPU dispatch accepted incomplete capabilities");
#ifdef MEDIAN_ENABLE_SIMD
    const int avx512 = CPUF_SSE2 | CPUF_SSE4_1 | CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_AVX512F | CPUF_AVX512DQ | CPUF_AVX512BW | CPUF_AVX512VL;
    if (!median::supports_opt(avx512, 8)) throw std::runtime_error("AVX512 dispatch unavailable");
    for (int bit : {CPUF_SSE2, CPUF_SSE4_1, CPUF_AVX, CPUF_AVX2, CPUF_FMA3, CPUF_AVX512F, CPUF_AVX512DQ, CPUF_AVX512BW, CPUF_AVX512VL})
      if (median::supports_opt(avx512 & ~bit, 8)) throw std::runtime_error("AVX512 prerequisite not checked");
#endif
    for (int opt = 0; opt <= 8; ++opt) {
      if (!median::supports_opt(host_cpu_flags(), opt)) continue;
      for (int pixels : {1, 15, 32, 71})
    for (int depth : {3, 4, 5, 7, 9, 13, 25})
      for (int components : {1, 2, 3, 4})
        for (bool shifted : {false, true})
          for (bool copy : {false, true})
            for (int mode = 0; mode < 3; ++mode) {
              int low = mode == 0 ? (depth - 1) / 2 : mode == 1 ? 1 : 0;
              int high = mode == 0 ? depth / 2 : mode == 1 ? 1 : 0;
              check<uint8_t>(depth, components, low, high, shifted, copy, opt, pixels);
              check<uint16_t>(depth, components, low, high, shifted, copy, opt, pixels);
              check<float>(depth, components, low, high, shifted, copy, opt, pixels);
            }
    }
    std::cout << "Kernel references, shifts, strides, tails and guards passed\n";
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
