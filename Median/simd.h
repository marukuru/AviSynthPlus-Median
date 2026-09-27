#ifndef MEDIAN_SIMD_H
#define MEDIAN_SIMD_H
#include "kernels.h"
namespace median {
int row_u8_sse2(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_u16_sse2(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_f32_sse2(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_u16_sse41(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_f32_avx(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_u8_avx2(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_u16_avx2(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_u8_avx512(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_u16_avx512(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_f32_avx512(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_f32_fma3(const uint8_t* const*, uint8_t*, int, int, int, int);
int row_f32_fma4(const uint8_t* const*, uint8_t*, int, int, int, int);
}
#endif
