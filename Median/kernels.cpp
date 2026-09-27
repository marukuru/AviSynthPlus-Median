#include "kernels.h"
#include "opt_med.h"
#include <algorithm>
#include <type_traits>

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
  if (count != job.depth) std::sort(values, values + job.depth);
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
  for (int y = job.start_y; y < job.end_y; ++y) {
    const T* rows[max_depth];
    for (int i = 0; i < job.depth; ++i) {
      const int sy = std::clamp(y - job.dy[i], 0, job.height - 1);
      rows[i] = reinterpret_cast<const T*>(job.src[i] + sy * job.src_pitch[i]);
    }
    T* dst = reinterpret_cast<T*>(job.dst + y * job.dst_pitch);
    for (int x = 0; x < job.width; ++x) {
      if (job.copy_every && (x + 1) % job.copy_every == 0) {
        dst[x] = rows[0][x];
        continue;
      }
      T values[max_depth];
      for (int i = 0; i < job.depth; ++i) {
        const int sx = std::clamp(x / job.components - job.dx[i], 0, pixels - 1);
        values[i] = rows[i][sx * job.components + x % job.components];
      }
      dst[x] = reduce(values, job);
    }
  }
}
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
