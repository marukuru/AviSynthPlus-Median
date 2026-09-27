#ifdef MEDIAN_ENABLE_SIMD
#include "simd_blend.h"
namespace median {
struct Fma3 {
  static __m256d madd(__m256d a, __m256d b, __m256d c) { return _mm256_fmadd_pd(a, b, c); }
};
int row_f32_fma3(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high)
{
  return blend_float_row<Fma3>(p, d, w, n, low, high);
}
}
#endif
