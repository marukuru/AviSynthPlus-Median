#ifdef MEDIAN_ENABLE_SIMD
#include <immintrin.h>
#include "simd_common.h"
namespace median {
struct U8AVX2 {
  using V = __m256i;
  static constexpr int lanes = 32, bytes = 1;
  static V load(const uint8_t* p) { return _mm256_loadu_si256(reinterpret_cast<const V*>(p)); }
  static void store(uint8_t* p, V v) { _mm256_storeu_si256(reinterpret_cast<V*>(p), v); }
  static bool valid(V) { return true; }
  static V min(V a, V b) { return _mm256_min_epu8(a, b); }
  static V max(V a, V b) { return _mm256_max_epu8(a, b); }
};
struct U16AVX2 : U8AVX2 {
  static constexpr int lanes = 16, bytes = 2;
  static V min(V a, V b) { return _mm256_min_epu16(a, b); }
  static V max(V a, V b) { return _mm256_max_epu16(a, b); }
};
int row_u8_avx2(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high) { return vector_row<U8AVX2>(p, d, w, n, low, high); }
int row_u16_avx2(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high) { return vector_row<U16AVX2>(p, d, w, n, low, high); }
}
#endif
