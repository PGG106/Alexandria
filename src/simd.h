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
    __m256i v1 = _mm256_hadd_epi32(vec, vec); // 0, 2, 4, 6, 1, 3, 5, 7
    __m256i v2 = _mm256_hadd_epi32(v1, v1); // 0, 4, 1, 5, 2, 6, 3, 7
    // Step 2: Extract the final result
    int result = _mm256_extract_epi32(v2, 0) + _mm256_extract_epi32(v2, 4);
    return result;
}
#endif