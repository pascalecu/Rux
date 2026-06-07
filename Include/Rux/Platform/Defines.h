#pragma once

#if defined(__SSE2__) || defined(_M_X64)
#  define RUX_FEATURE_SSE2 1
#else
#  define RUX_FEATURE_SSE2 0
#endif

#ifdef __AVX__
#  define RUX_FEATURE_AVX 1
#else
#  define RUX_FEATURE_AVX 0
#endif

#ifdef __AVX2__
#  define RUX_FEATURE_AVX2 1
#else
#  define RUX_FEATURE_AVX2 0
#endif

#ifdef __AVX512F__
#  define RUX_FEATURE_AVX512 1
#else
#  define RUX_FEATURE_AVX512 0
#endif

#ifdef __ARM_NEON
#  define RUX_FEATURE_NEON 1
#else
#  define RUX_FEATURE_NEON 0
#endif

#ifdef __riscv_vector
#  define RUX_FEATURE_RVV 1
#else
#  define RUX_FEATURE_RVV 0
#endif

#ifdef __ARM_FEATURE_SVE
#  define RUX_FEATURE_SVE 1
#else
#  define RUX_FEATURE_SVE 0
#endif
