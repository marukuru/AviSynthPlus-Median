#ifndef MEDIAN_KERNELS_H
#define MEDIAN_KERNELS_H

#include <cstddef>
#include <cstdint>

namespace median {
constexpr int max_depth = 25;
struct PlaneJob {
  const uint8_t* src[max_depth];
  std::ptrdiff_t src_pitch[max_depth];
  uint8_t* dst;
  std::ptrdiff_t dst_pitch;
  int dx[max_depth], dy[max_depth];
  int width, height, component_size, components;
  int copy_every; // Copy every Nth component from clip 0 when chroma=false.
  int depth, low, high;
  int start_y, end_y;
};
void process_plane(const PlaneJob& job);
}
#endif
