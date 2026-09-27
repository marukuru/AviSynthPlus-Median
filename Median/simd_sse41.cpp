#ifdef MEDIAN_ENABLE_SIMD
#include <smmintrin.h>
#include "simd_common.h"
namespace median {
struct U16SSE41 {
  using V = __m128i;
  static constexpr int lanes = 8, bytes = 2;
  static V load(const uint8_t* p) { return _mm_loadu_si128(reinterpret_cast<const V*>(p)); }
  static void store(uint8_t* p, V v) { _mm_storeu_si128(reinterpret_cast<V*>(p), v); }
  static bool valid(V) { return true; }
  static V min(V a, V b) { return _mm_min_epu16(a, b); }
  static V max(V a, V b) { return _mm_max_epu16(a, b); }
};
int row_u16_sse41(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high) { return vector_row<U16SSE41>(p, d, w, n, low, high); }
}
#endif
