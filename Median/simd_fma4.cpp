#ifdef MEDIAN_ENABLE_SIMD
#ifdef _MSC_VER
#include <ammintrin.h>
#else
#include <x86intrin.h>
#endif
#include "simd_blend.h"
namespace median {
struct Fma4 {
  static __m256d madd(__m256d a, __m256d b, __m256d c) { return _mm256_macc_pd(a, b, c); }
};
int row_f32_fma4(const uint8_t* const* p, uint8_t* d, int w, int n, int low, int high)
{
  return blend_float_row<Fma4>(p, d, w, n, low, high);
}
}
#endif
