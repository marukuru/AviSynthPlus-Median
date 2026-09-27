#ifndef MEDIAN_TEST_CPU_FLAGS_H
#define MEDIAN_TEST_CPU_FLAGS_H
#include "avs/cpuid.h"
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif
inline int host_cpu_flags()
{
  int flags = 0;
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
  __builtin_cpu_init();
  if (__builtin_cpu_supports("sse2")) flags |= CPUF_SSE2;
  if (__builtin_cpu_supports("sse4.1")) flags |= CPUF_SSE4_1;
  if (__builtin_cpu_supports("avx")) flags |= CPUF_AVX;
  if (__builtin_cpu_supports("avx2")) flags |= CPUF_AVX2;
  if (__builtin_cpu_supports("fma")) flags |= CPUF_FMA3;
  if (__builtin_cpu_supports("fma4")) flags |= CPUF_FMA4;
  if (__builtin_cpu_supports("avx512f")) flags |= CPUF_AVX512F;
  if (__builtin_cpu_supports("avx512dq")) flags |= CPUF_AVX512DQ;
  if (__builtin_cpu_supports("avx512bw")) flags |= CPUF_AVX512BW;
  if (__builtin_cpu_supports("avx512vl")) flags |= CPUF_AVX512VL;
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
  int regs[4];
  __cpuid(regs, 1);
  if (regs[3] & (1 << 26)) flags |= CPUF_SSE2;
  if (regs[2] & (1 << 19)) flags |= CPUF_SSE4_1;
  if ((regs[2] & (1 << 27)) && (regs[2] & (1 << 28)) && (_xgetbv(0) & 6) == 6) {
    flags |= CPUF_AVX;
    if (regs[2] & (1 << 12)) flags |= CPUF_FMA3;
    __cpuid(regs, 0x80000000);
    if (static_cast<unsigned int>(regs[0]) >= 0x80000001U) {
      __cpuid(regs, 0x80000001);
      if (regs[2] & (1 << 16)) flags |= CPUF_FMA4;
    }
    __cpuidex(regs, 7, 0);
    if (regs[1] & (1 << 5)) flags |= CPUF_AVX2;
    if ((_xgetbv(0) & 0xe6) == 0xe6) {
      if (regs[1] & (1 << 16)) flags |= CPUF_AVX512F;
      if (regs[1] & (1 << 17)) flags |= CPUF_AVX512DQ;
      if (regs[1] & (1 << 30)) flags |= CPUF_AVX512BW;
      if (regs[1] & (1U << 31)) flags |= CPUF_AVX512VL;
    }
  }
#endif
  return flags;
}
#endif
