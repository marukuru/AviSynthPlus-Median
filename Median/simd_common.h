#ifndef MEDIAN_SIMD_COMMON_H
#define MEDIAN_SIMD_COMMON_H
#include "kernels.h"

namespace median {
template<class Ops>
inline typename Ops::V middle(typename Ops::V* p, int depth)
{
  auto swap = [&](int a, int b) {
    auto low = Ops::min(p[a], p[b]);
    p[b] = Ops::max(p[a], p[b]);
    p[a] = low;
  };
  switch (depth) {
  case 3: return Ops::max(Ops::min(p[0], p[1]), Ops::min(Ops::max(p[0], p[1]), p[2]));
  case 5:
    swap(0, 1); swap(3, 4); swap(0, 3); swap(1, 4);
    swap(1, 2); swap(2, 3); swap(1, 2);
    return p[2];
  case 7:
    swap(0, 5); swap(0, 3); swap(1, 6); swap(2, 4);
    swap(0, 1); swap(3, 5); swap(2, 6); swap(2, 3);
    swap(3, 6); swap(4, 5); swap(1, 4); swap(1, 3);
    swap(3, 4);
    return p[3];
  case 9:
    swap(1, 2); swap(4, 5); swap(7, 8); swap(0, 1);
    swap(3, 4); swap(6, 7); swap(1, 2); swap(4, 5);
    swap(7, 8); swap(0, 3); swap(5, 8); swap(4, 7);
    swap(3, 6); swap(1, 4); swap(2, 5); swap(4, 7);
    swap(4, 2); swap(6, 4); swap(4, 2);
    return p[4];
  default:
    // Odd-even sorting network for the larger supported stacks.
    for (int pass = 0; pass < depth; ++pass)
      for (int i = pass & 1; i + 1 < depth; i += 2) swap(i, i + 1);
    return p[depth / 2];
  }
}

template<class Ops>
int vector_row(const uint8_t* const* rows, uint8_t* dst, int width, int depth, int low, int high)
{
  if (low != high || depth != 2 * low + 1) return 0;
  int x = 0;
  for (; x + Ops::lanes <= width; x += Ops::lanes) {
    typename Ops::V values[max_depth];
    for (int i = 0; i < depth; ++i) {
      values[i] = Ops::load(rows[i] + x * Ops::bytes);
      if (!Ops::valid(values[i])) return x; // Non-finite floats use the scalar path.
    }
    Ops::store(dst + x * Ops::bytes, middle<Ops>(values, depth));
  }
  return x;
}
}
#endif
