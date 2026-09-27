#ifdef MEDIAN_ENABLE_SIMD
#include <emmintrin.h>
#include "simd_common.h"
namespace median {
struct U8 {
  using V = __m128i;
  static constexpr int lanes = 16, bytes = 1;
  static V load(const uint8_t* p) { return _mm_loadu_si128(reinterpret_cast<const V*>(p)); }
  static void store(uint8_t* p, V v) { _mm_storeu_si128(reinterpret_cast<V*>(p), v); }
  static bool valid(V) { return true; }
  static V min(V a, V b) { return _mm_min_epu8(a, b); }
  static V max(V a, V b) { return _mm_max_epu8(a, b); }
};
struct U16 : U8 {
  static constexpr int lanes = 8, bytes = 2;
  static V min(V a, V b) { return _mm_sub_epi16(a, _mm_subs_epu16(a, b)); }
  static V max(V a, V b) { return _mm_add_epi16(a, _mm_subs_epu16(b, a)); }
};
struct F32 {
  using V = __m128;
  static constexpr int lanes = 4, bytes = 4;
  static V load(const uint8_t* p) { return _mm_loadu_ps(reinterpret_cast<const float*>(p)); }
  static void store(uint8_t* p, V v) { _mm_storeu_ps(reinterpret_cast<float*>(p), v); }
  static bool valid(V v) { return _mm_movemask_ps(_mm_cmplt_ps(_mm_andnot_ps(_mm_set1_ps(-0.0f), v), _mm_castsi128_ps(_mm_set1_epi32(0x7f800000)))) == 15; }
  static V min(V a, V b) { return _mm_min_ps(a, b); }
  static V max(V a, V b) { return _mm_max_ps(a, b); }
};
int row_u8_sse2(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high) { return vector_row<U8>(p, d, w, n, low, high); }
int row_u16_sse2(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high) { return vector_row<U16>(p, d, w, n, low, high); }
int row_f32_sse2(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high) { return vector_row<F32>(p, d, w, n, low, high); }
}
#endif
