#ifdef MEDIAN_ENABLE_SIMD
#include <immintrin.h>
#include <limits>
#include "simd_common.h"
#include "simd.h"
namespace median {
struct U8AVX512 {
  using V = __m512i;
  static constexpr int lanes = 64, bytes = 1;
  static V load(const uint8_t* p) { return _mm512_loadu_si512(p); }
  static void store(uint8_t* p, V v) { _mm512_storeu_si512(p, v); }
  static bool valid(V) { return true; }
  static V min(V a, V b) { return _mm512_min_epu8(a, b); }
  static V max(V a, V b) { return _mm512_max_epu8(a, b); }
};
struct U16AVX512 : U8AVX512 {
  static constexpr int lanes = 32, bytes = 2;
  static V min(V a, V b) { return _mm512_min_epu16(a, b); }
  static V max(V a, V b) { return _mm512_max_epu16(a, b); }
};
struct F32AVX512 {
  using V = __m512;
  static constexpr int lanes = 16, bytes = 4;
  static V load(const uint8_t* p) { return _mm512_loadu_ps(p); }
  static void store(uint8_t* p, V v) { _mm512_storeu_ps(p, v); }
  static bool valid(V v) { return _mm512_cmp_ps_mask(_mm512_andnot_ps(_mm512_set1_ps(-0.0f), v), _mm512_set1_ps(std::numeric_limits<float>::infinity()), _CMP_LT_OQ) == 0xffff; }
  static V min(V a, V b) { return _mm512_min_ps(a, b); }
  static V max(V a, V b) { return _mm512_max_ps(a, b); }
};
int row_u8_avx512(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high) { return vector_row<U8AVX512>(p, d, w, n, low, high); }
int row_u16_avx512(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high) { return vector_row<U16AVX512>(p, d, w, n, low, high); }
int row_f32_avx512(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high)
{
  if (low != high || n != 2 * low + 1) return row_f32_fma3(p, d, w, n, low, high);
  return vector_row<F32AVX512>(p, d, w, n, low, high);
}
}
#endif
