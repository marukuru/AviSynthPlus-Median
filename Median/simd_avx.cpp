#ifdef MEDIAN_ENABLE_SIMD
#include <immintrin.h>
#include <limits>
#include "simd_common.h"
namespace median {
struct F32AVX {
  using V = __m256;
  static constexpr int lanes = 8, bytes = 4;
  static V load(const uint8_t* p) { return _mm256_loadu_ps(reinterpret_cast<const float*>(p)); }
  static void store(uint8_t* p, V v) { _mm256_storeu_ps(reinterpret_cast<float*>(p), v); }
  static bool valid(V v) { return _mm256_movemask_ps(_mm256_cmp_ps(_mm256_andnot_ps(_mm256_set1_ps(-0.0f), v), _mm256_set1_ps(std::numeric_limits<float>::infinity()), _CMP_LT_OQ)) == 255; }
  static V min(V a, V b) { return _mm256_min_ps(a, b); }
  static V max(V a, V b) { return _mm256_max_ps(a, b); }
};
int row_f32_avx(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high) { return vector_row<F32AVX>(p, d, w, n, low, high); }
}
#endif
