#pragma once
#include <cstdint>

#if defined(USE_SIMD)
#include <immintrin.h>
#endif

#if defined(USE_AVX512)
using vepi16 = __m512i;
using vepi32 = __m512i; 

inline vepi16  vec_zero_epi16() { return _mm512_setzero_si512(); }
inline vepi32  vec_zero_epi32() { return _mm512_setzero_si512(); }
inline vepi16  vec_set1_epi16 (const int16_t n) { return _mm512_set1_epi16(n); }
inline vepi16  vec_loadu_epi  (const vepi16 *src) { return _mm512_loadu_si512(src); }
inline vepi16  vec_max_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm512_max_epi16(vec0, vec1); }
inline vepi16  vec_min_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm512_min_epi16(vec0, vec1); }
inline vepi16  vec_mullo_epi16(const vepi16 vec0, const vepi16 vec1) { return _mm512_mullo_epi16(vec0, vec1); }
inline vepi32  vec_madd_epi16 (const vepi16 vec0, const vepi16 vec1) { return _mm512_madd_epi16(vec0, vec1); }
inline vepi32  vec_add_epi32  (const vepi32 vec0, const vepi32 vec1) { return _mm512_add_epi32(vec0, vec1); }
inline int32_t vec_reduce_add_epi32(const vepi32 vec) { return _mm512_reduce_add_epi32(vec); }

#elif defined(USE_AVX2)
using vepi16 = __m256i;
using vepi32 = __m256i; 

inline vepi16  vec_zero_epi16() { return _mm256_setzero_si256(); }
inline vepi32  vec_zero_epi32() { return _mm256_setzero_si256(); }
inline vepi16  vec_set1_epi16 (const int16_t n) { return _mm256_set1_epi16(n); }
inline vepi16  vec_loadu_epi  (const vepi16 *src) { return _mm256_loadu_si256(src); }
inline vepi16  vec_max_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm256_max_epi16(vec0, vec1); }
inline vepi16  vec_min_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm256_min_epi16(vec0, vec1); }
inline vepi16  vec_mullo_epi16(const vepi16 vec0, const vepi16 vec1) { return _mm256_mullo_epi16(vec0, vec1); }
inline vepi32  vec_madd_epi16 (const vepi16 vec0, const vepi16 vec1) { return _mm256_madd_epi16(vec0, vec1); }
inline vepi32  vec_add_epi32  (const vepi32 vec0, const vepi32 vec1) { return _mm256_add_epi32(vec0, vec1); }
inline int32_t vec_reduce_add_epi32(const vepi32 vec) {
    // Split the __m256i into 2 __m128i vectors, and add them together
    __m128i lower128 = _mm256_castsi256_si128(vec);
    __m128i upper128 = _mm256_extracti128_si256(vec, 1);
    __m128i sum128   = _mm_add_epi32(lower128, upper128);

    // Store the high 64 bits of sum128 in the low 64 bits of this vector,
    // then add them together (only the lowest 64 bits are relevant)
    __m128i upper64 = _mm_unpackhi_epi64(sum128, sum128);
    __m128i sum64   = _mm_add_epi32(upper64, sum128);

    // Store the high 32 bits of the relevant part of sum64 in the low 32 bits of the vector,
    // and then add them together (only the lowest 32 bits are relevant)
    __m128i upper32 = _mm_shuffle_epi32(sum64, 1);
    __m128i sum32   = _mm_add_epi32(upper32, sum64);

    // Return the bottom 32 bits of sum32
    return _mm_cvtsi128_si32(sum32);
}
#endif