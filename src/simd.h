#pragma once
#include <cstdint>

#if defined(USE_SIMD)
#include <immintrin.h>
#endif

#if defined(USE_AVX512)
using vepi8  = __m512i;
using vepi16 = __m512i;
using vepi32 = __m512i;
using vps32  = __m512;
using v128i  = __m128i;

inline vepi16  vec_zero_epi16() { return _mm512_setzero_si512(); }
inline vepi32  vec_zero_epi32() { return _mm512_setzero_si512(); }
inline vepi16  vec_set1_epi16 (const int16_t n) { return _mm512_set1_epi16(n); }
inline vepi32 vec_set1_epi32 (const int32_t n) { return _mm512_set1_epi32(n); }
inline vepi16  vec_loadu_epi  (const vepi16 *src) { return _mm512_loadu_si512(src); }
inline void    vec_storeu_epi (vepi16 *dst, const vepi16 vec) { _mm512_storeu_si512(dst, vec); }
inline vepi16  vec_add_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm512_add_epi16(vec0, vec1); }
inline vepi16  vec_sub_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm512_sub_epi16(vec0, vec1); }
inline vepi16  vec_max_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm512_max_epi16(vec0, vec1); }
inline vepi16  vec_min_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm512_min_epi16(vec0, vec1); }
inline vepi16  vec_mullo_epi16(const vepi16 vec0, const vepi16 vec1) { return _mm512_mullo_epi16(vec0, vec1); }
inline vepi32  vec_madd_epi16 (const vepi16 vec0, const vepi16 vec1) { return _mm512_madd_epi16(vec0, vec1); }
inline vepi32  vec_add_epi32  (const vepi32 vec0, const vepi32 vec1) { return _mm512_add_epi32(vec0, vec1); }
inline int32_t vec_reduce_add_epi32(const vepi32 vec) { return _mm512_reduce_add_epi32(vec); }


inline vepi32 vec_dpbusdx2_epi32(const vepi32 sum, const vepi8 vec0, const vepi8 vec1, const vepi8 vec2, const vepi8 vec3) {
#if defined(USE_VNNI512)
    return _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(sum, vec0, vec1), vec2, vec3);
#else
    const vepi16 product16a = _mm512_maddubs_epi16(vec0, vec1);
    const vepi16 product16b = _mm512_maddubs_epi16(vec2, vec3);
    const vepi32 product32  = _mm512_madd_epi16(_mm512_add_epi16(product16a, product16b), _mm512_set1_epi16(1));
    return _mm512_add_epi32(sum, product32);
#endif
}

inline vepi32 vec_dpbusd_epi32(const vepi32 sum, const vepi8 vec0, const vepi8 vec1) {
#if defined(USE_VNNI512)
    return _mm512_dpbusd_epi32(sum, vec0, vec1);
#else
    const vepi16 product16 = _mm512_maddubs_epi16(vec0, vec1);
    const vepi32 product32 = _mm512_madd_epi16(product16, _mm512_set1_epi16(1));
    return _mm512_add_epi32(sum, product32);
#endif
}

inline vps32 vec_cvtepi32_ps(const vepi32 vec) { return _mm512_cvtepi32_ps(vec); }

inline vps32 vec_zero_ps () { return _mm512_setzero_ps(); }
inline vps32 vec_set1_ps (const float n) { return _mm512_set1_ps(n); }
inline vps32 vec_load_ps (const float *src) { return _mm512_load_ps(src); }
inline void  vec_store_ps(float *dst, const vps32 vec) { return _mm512_store_ps(dst, vec); }
inline vps32 vec_add_ps  (const vps32 vec0, const vps32 vec1) { return _mm512_add_ps(vec0, vec1); }
inline vps32 vec_mul_ps  (const vps32 vec0, const vps32 vec1) { return _mm512_mul_ps(vec0, vec1); }
inline vps32 vec_min_ps  (const vps32 vec0, const vps32 vec1) { return _mm512_min_ps(vec0, vec1); }
inline vps32 vec_max_ps  (const vps32 vec0, const vps32 vec1) { return _mm512_max_ps(vec0, vec1); }
inline vps32 vec_mul_add_ps(const vps32 vec0, const vps32 vec1, const vps32 vec2) { return _mm512_fmadd_ps(vec0, vec1, vec2); }
inline float vec_reduce_add_ps(const vps32 *vecs) { return _mm512_reduce_add_ps(vecs[0]); }

#elif defined(USE_AVX2)
using vepi16 = __m256i;
using vepi32 = __m256i;

inline vepi16  vec_zero_epi16() { return _mm256_setzero_si256(); }
inline vepi32  vec_zero_epi32() { return _mm256_setzero_si256(); }
inline vepi16  vec_set1_epi16 (const int16_t n) { return _mm256_set1_epi16(n); }
inline vepi16  vec_loadu_epi  (const vepi16 *src) { return _mm256_loadu_si256(src); }
inline void    vec_storeu_epi (vepi16 *dst, const vepi16 vec) { _mm256_storeu_si256(dst, vec); }
inline vepi16  vec_add_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm256_add_epi16(vec0, vec1); }
inline vepi16  vec_sub_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm256_sub_epi16(vec0, vec1); }
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
#else
inline float reduce_add(float *sums, const int length) {
    if (length == 2) return sums[0] + sums[1];
    for (int i = 0; i < length / 2; ++i)
        sums[i] += sums[i + length / 2];

    return reduce_add(sums, length / 2);
}
#endif