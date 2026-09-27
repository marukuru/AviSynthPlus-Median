#include "kernels.h"
#include "opt_med.h"
#include <algorithm>
#include <type_traits>
#include <cmath>
#include "avs/cpuid.h"
#include "simd.h"

namespace median {
namespace {
template<typename T>
T reduce(T* values, const PlaneJob& job)
{
  const int count = job.depth - job.low - job.high;
  if constexpr (std::is_same<T, uint8_t>::value) {
    if (count == 1 && job.low == job.high) {
      switch (job.depth) {
      case 3: return opt_med3(values);
      case 5: return opt_med5(values);
      case 7: return opt_med7(values);
      case 9: return opt_med9(values);
      }
    }
  }
  if (count != job.depth) {
    std::sort(values, values + job.depth, [](T a, T b) {
      if constexpr (std::is_floating_point<T>::value)
        return std::isnan(b) ? !std::isnan(a) : a < b;
      else return a < b;
    });
  }
  if (count == 1) return values[job.low];
  // Float planes must retain fractional and negative chroma values.
  using Sum = typename std::conditional<std::is_floating_point<T>::value, double, uint32_t>::type;
  Sum sum = 0;
  for (int i = job.low; i < job.depth - job.high; ++i) sum += values[i];
  return static_cast<T>(sum / count);
}

template<typename T>
void process(const PlaneJob& job)
{
  const int pixels = job.width / job.components;
  int begin = 0, end = job.width;
  for (int i = 0; i < job.depth; ++i) {
    begin = std::max(begin, job.dx[i] * job.components);
    end = std::min(end, job.width + job.dx[i] * job.components);
  }
  begin = std::clamp(begin, 0, job.width);
  end = std::clamp(end, begin, job.width);
  for (int y = job.start_y; y < job.end_y; ++y) {
    const T* rows[max_depth];
    for (int i = 0; i < job.depth; ++i) {
      const int sy = std::clamp(y - job.dy[i], 0, job.height - 1);
      rows[i] = reinterpret_cast<const T*>(job.src[i] + sy * job.src_pitch[i]);
    }
    T* dst = reinterpret_cast<T*>(job.dst + y * job.dst_pitch);
    auto border = [&](int x) {
      T values[max_depth];
      for (int i = 0; i < job.depth; ++i) {
        const int sx = std::clamp(x / job.components - job.dx[i], 0, pixels - 1);
        values[i] = rows[i][sx * job.components + x % job.components];
      }
      dst[x] = reduce(values, job);
    };
    for (int x = 0; x < begin; ++x) border(x);
    if (end > begin) {
      const uint8_t* aligned[max_depth];
      for (int i = 0; i < job.depth; ++i)
        aligned[i] = reinterpret_cast<const uint8_t*>(rows[i] + begin - job.dx[i] * job.components);
      int done = 0;
      if (job.kernel)
        done = job.kernel(aligned, reinterpret_cast<uint8_t*>(dst + begin), end - begin, job.depth, job.low, job.high);
      for (int x = done; x < end - begin; ++x) {
        T values[max_depth];
        for (int i = 0; i < job.depth; ++i) values[i] = reinterpret_cast<const T*>(aligned[i])[x];
        dst[begin + x] = reduce(values, job);
      }
    }
    for (int x = end; x < job.width; ++x) border(x);
    if (job.copy_every)
      for (int x = job.copy_every - 1; x < job.width; x += job.copy_every) dst[x] = rows[0][x];
  }
}
}
bool supports_opt(int flags, int opt)
{
  if (opt == 0 || opt == 1) return true;
#ifdef MEDIAN_ENABLE_SIMD
  const int requirements[] = {0, 0, CPUF_SSE2, CPUF_SSE2 | CPUF_SSE4_1,
    CPUF_SSE2 | CPUF_SSE4_1 | CPUF_AVX, CPUF_SSE2 | CPUF_SSE4_1 | CPUF_AVX | CPUF_AVX2,
    CPUF_SSE2 | CPUF_SSE4_1 | CPUF_AVX | CPUF_AVX2 | CPUF_FMA3,
    CPUF_SSE2 | CPUF_SSE4_1 | CPUF_AVX | CPUF_AVX2 | CPUF_FMA4,
    CPUF_SSE2 | CPUF_SSE4_1 | CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 |
      CPUF_AVX512F | CPUF_AVX512DQ | CPUF_AVX512BW | CPUF_AVX512VL};
  return opt >= 2 && opt <= 8 && (flags & requirements[opt]) == requirements[opt];
#else
  (void)flags;
  return false;
#endif
}

RowKernel select_kernel(int flags, int cs, int opt)
{
#ifdef MEDIAN_ENABLE_SIMD
  int level = opt;
  if (level == 0) {
    for (int candidate : {8, 6, 7, 5, 4, 3, 2, 1})
      if (supports_opt(flags, candidate)) { level = candidate; break; }
  }
  if (!supports_opt(flags, level)) return nullptr;
  if (level == 8) return cs == 1 ? row_u8_avx512 : cs == 2 ? row_u16_avx512 : row_f32_avx512;
  if (level == 6 && cs == 4) return row_f32_fma3;
  if (level == 7 && cs == 4) return row_f32_fma4;
  if (level >= 5 && (flags & CPUF_AVX2))
    return cs == 1 ? row_u8_avx2 : cs == 2 ? row_u16_avx2 : row_f32_avx;
  if (level >= 4 && cs == 4 && (flags & CPUF_AVX)) return row_f32_avx;
  if (level >= 3 && cs == 2 && (flags & CPUF_SSE4_1)) return row_u16_sse41;
  if (level >= 2 && (flags & CPUF_SSE2))
    return cs == 1 ? row_u8_sse2 : cs == 2 ? row_u16_sse2 : row_f32_sse2;
#else
  (void)flags; (void)cs; (void)opt;
#endif
  return nullptr;
}

void process_plane(const PlaneJob& job)
{
  switch (job.component_size) {
  case 1: process<uint8_t>(job); break;
  case 2: process<uint16_t>(job); break;
  case 4: process<float>(job); break;
  }
}
}
