#ifndef MEDIAN_SIMD_BLEND_H
#define MEDIAN_SIMD_BLEND_H
#include <immintrin.h>
#include <limits>
#include "simd.h"
namespace median {
template<class Fma>
int blend_float_row(const uint8_t* const* rows, uint8_t* dst, int width, int depth, int low, int high)
{
  const int count = depth - low - high;
  if (count == 1 && low == high) return row_f32_avx(rows, dst, width, depth, low, high);
  const __m256d divisor = _mm256_set1_pd(count);
  const __m256d inverse = _mm256_set1_pd(1.0 / count);
  int x = 0;
  for (; x + 8 <= width; x += 8) {
    __m256 values[max_depth];
    for (int i = 0; i < depth; ++i) {
      values[i] = _mm256_loadu_ps(reinterpret_cast<const float*>(rows[i]) + x);
      auto abs = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), values[i]);
      if (_mm256_movemask_ps(_mm256_cmp_ps(abs, _mm256_set1_ps(std::numeric_limits<float>::infinity()), _CMP_LT_OQ)) != 255) return x;
    }
    if (low || high) {
      for (int pass = 0; pass < depth; ++pass) {
        for (int i = pass & 1; i + 1 < depth; i += 2) {
          auto minimum = _mm256_min_ps(values[i], values[i + 1]);
          values[i + 1] = _mm256_max_ps(values[i], values[i + 1]);
          values[i] = minimum;
        }
      }
    }
    __m256d lo = _mm256_setzero_pd(), hi = _mm256_setzero_pd();
    for (int i = low; i < depth - high; ++i) {
      lo = _mm256_add_pd(lo, _mm256_cvtps_pd(_mm256_castps256_ps128(values[i])));
      hi = _mm256_add_pd(hi, _mm256_cvtps_pd(_mm256_extractf128_ps(values[i], 1)));
    }
    // Refine reciprocal division using a fused residual; retain double precision
    // until the final float conversion, including negative chroma samples.
    auto divide = [&](const __m256d sum) {
      auto q = _mm256_mul_pd(sum, inverse);
      auto remainder = Fma::madd(_mm256_sub_pd(_mm256_setzero_pd(), q), divisor, sum);
      return Fma::madd(remainder, inverse, q);
    };
    auto result = _mm256_castps128_ps256(_mm256_cvtpd_ps(divide(lo)));
    result = _mm256_insertf128_ps(result, _mm256_cvtpd_ps(divide(hi)), 1);
    _mm256_storeu_ps(reinterpret_cast<float*>(dst) + x, result);
  }
  return x;
}
}
#endif
