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

inline vepi16 vec_zero_epi16() { return _mm512_setzero_si512(); }
inline vepi32 vec_zero_epi32() { return _mm512_setzero_si512(); }
inline vepi16 vec_set1_epi16 (const int16_t n) { return _mm512_set1_epi16(n); }
inline vepi16 vec_load_epi   (const vepi16 *src) { return _mm512_load_si512(src); }
inline void   vec_store_epi  (vepi16 *dst, const vepi16 vec) { _mm512_store_si512(dst, vec); }
inline vepi16 vec_max_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm512_max_epi16(vec0, vec1); }
inline vepi16 vec_min_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm512_min_epi16(vec0, vec1); }
inline vepi16 vec_mulhi_epi16(const vepi16 vec0, const vepi16 vec1) { return _mm512_mulhi_epi16(vec0, vec1); }
inline vepi16 vec_slli_epi16 (const vepi16 vec, const int shift) { return _mm512_slli_epi16(vec, shift); }
inline vepi8  vec_packus_permute_epi16(const vepi16 vec0, const vepi16 vec1) {
    const vepi8 packed = _mm512_packus_epi16(vec0, vec1);
    return _mm512_permutexvar_epi64(_mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7), packed);
}

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

inline vepi32 vec_set1_epi32(const int val) {
    return _mm512_set1_epi32(val);
}

inline vps32 vec_cvtepi32_ps(const vepi32 vec) { return _mm512_cvtepi32_ps(vec); }

inline vps32 vec_zero_ps () { return _mm512_setzero_ps(); }
inline vps32 vec_set1_ps (const float n) { return _mm512_set1_ps(n); }
inline vps32 vec_load_ps (const float *src) { return _mm512_load_ps(src); }
inline void  vec_store_ps(float *dst, const vps32 vec) { return _mm512_store_ps(dst, vec); }
inline vps32 vec_add_ps  (const vps32 vec0, const vps32 vec1) { return _mm512_add_ps(vec0, vec1); }
inline vps32 vec_mul_ps  (const vps32 vec0, const vps32 vec1) { return _mm512_mul_ps(vec0, vec1); }
inline vps32 vec_div_ps  (const vps32 vec0, const vps32 vec1) { return _mm512_div_ps(vec0, vec1); }
inline vps32 vec_max_ps  (const vps32 vec0, const vps32 vec1) { return _mm512_max_ps(vec0, vec1); }
inline vps32 vec_mul_add_ps(const vps32 vec0, const vps32 vec1, const vps32 vec2) { return _mm512_fmadd_ps(vec0, vec1, vec2); }
inline float vec_reduce_add_ps(const vps32 vec) { return _mm512_reduce_add_ps(vec); }

#elif defined(USE_AVX2)
using vepi8  = __m256i;
using vepi16 = __m256i;
using vepi32 = __m256i;
using vps32  = __m256;

inline vepi16 vec_zero_epi16() { return _mm256_setzero_si256(); }
inline vepi32 vec_zero_epi32() { return _mm256_setzero_si256(); }
inline vepi16 vec_set1_epi16 (const int16_t n) { return _mm256_set1_epi16(n); }
inline vepi16 vec_load_epi   (const vepi16 *src) { return _mm256_load_si256(src); }
inline void   vec_store_epi  (vepi16 *dst, const vepi16 vec) { _mm256_store_si256(dst, vec); }
inline vepi16 vec_max_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm256_max_epi16(vec0, vec1); }
inline vepi16 vec_min_epi16  (const vepi16 vec0, const vepi16 vec1) { return _mm256_min_epi16(vec0, vec1); }
inline vepi16 vec_mulhi_epi16(const vepi16 vec0, const vepi16 vec1) { return _mm256_mulhi_epi16(vec0, vec1); }
inline vepi16 vec_slli_epi16 (const vepi16 vec, const int shift) { return _mm256_slli_epi16(vec, shift); }
inline vepi8  vec_packus_permute_epi16(const vepi16 vec0, const vepi16 vec1) {
    const vepi8 packed = _mm256_packus_epi16(vec0, vec1);
    return _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0));
}

inline vepi32 vec_dpbusdx2_epi32(const vepi32 sum, const vepi8 vec0, const vepi8 vec1, const vepi8 vec2, const vepi8 vec3) {
    const vepi16 product16a = _mm256_maddubs_epi16(vec0, vec1);
    const vepi16 product16b = _mm256_maddubs_epi16(vec2, vec3);
    const vepi32 product32  = _mm256_madd_epi16(_mm256_add_epi16(product16a, product16b), _mm256_set1_epi16(1));
    return _mm256_add_epi32(sum, product32);
}

inline vepi32 vec_set1_epi32(const int val) {
    return _mm256_set1_epi32(val);
}

inline vps32 vec_cvtepi32_ps(const vepi32 vec) { return _mm256_cvtepi32_ps(vec); }

inline vps32 vec_zero_ps () { return _mm256_setzero_ps(); }
inline vps32 vec_set1_ps (const float n) { return _mm256_set1_ps(n); }
inline vps32 vec_load_ps (const float *src) { return _mm256_load_ps(src); }
inline void  vec_store_ps(float *dst, const vps32 vec) { return _mm256_store_ps(dst, vec); }
inline vps32 vec_add_ps  (const vps32 vec0, const vps32 vec1) { return _mm256_add_ps(vec0, vec1); }
inline vps32 vec_mul_ps  (const vps32 vec0, const vps32 vec1) { return _mm256_mul_ps(vec0, vec1); }
inline vps32 vec_div_ps  (const vps32 vec0, const vps32 vec1) { return _mm256_div_ps(vec0, vec1); }
inline vps32 vec_max_ps  (const vps32 vec0, const vps32 vec1) { return _mm256_max_ps(vec0, vec1); }
inline vps32 vec_mul_add_ps(const vps32 vec0, const vps32 vec1, const vps32 vec2) { return _mm256_fmadd_ps(vec0, vec1, vec2); }
inline float vec_reduce_add_ps(const vps32 vec) {
    const __m128 upper_128 = _mm256_extractf128_ps(vec, 1);
    const __m128 lower_128 = _mm256_castps256_ps128(vec);
    const __m128 sum_128 = _mm_add_ps(upper_128, lower_128);

    const __m128 upper_64 = _mm_movehl_ps(sum_128, sum_128);
    const __m128 sum_64 = _mm_add_ps(upper_64, sum_128);

    const __m128 upper_32 = _mm_shuffle_ps(sum_64, sum_64, 1);
    const __m128 sum_32 = _mm_add_ss(upper_32, sum_64);

    return _mm_cvtss_f32(sum_32);
}
#endif