#ifndef F32X4_H
#define F32X4_H

#include <xmmintrin.h>  // SSE
#include <emmintrin.h>  // SSE2
#include <immintrin.h>  // AVX
#include <math.h>
#include <stdio.h>
#include <stdint.h>

typedef __m128  f32x4;
typedef __m256  f32x8;
typedef __m256d f64x4;

#if defined(_WIN32)
  #include <intrin.h>
  #define cpuid(leaf, a, b, c, d) __cpuid((int[]){a,b,c,d}, leaf)
  #define cpuid_count(leaf, sub, a, b, c, d) __cpuidex((int[]){a,b,c,d}, leaf, sub)
#else
  #include <cpuid.h>
  #define cpuid(leaf, a, b, c, d)            __get_cpuid(leaf, &a, &b, &c, &d)
  #define cpuid_count(leaf, sub, a, b, c, d) __get_cpuid_count(leaf, sub, &a, &b, &c, &d)
#endif

typedef struct {
    // --- f32x4 (SSE / SSE2) ---
    int sse;
    int sse2;
    int sse3;
    int sse4_1;
    int sse4_2;

    // --- f32x8 / f64x4 (AVX) ---
    int avx;
    int avx2;

    // --- FMA 
    int fma;

    int avx512f;
} SIMD_Caps;

static inline SIMD_Caps simd_query_caps(void)
{
    SIMD_Caps caps = {0};
    uint32_t eax, ebx, ecx, edx;

    // ── Leaf 1: SSE family + FMA ─────────────────────────────────────────────
    cpuid(1, eax, ebx, ecx, edx);
    caps.sse    = (edx >> 25) & 1;
    caps.sse2   = (edx >> 26) & 1;
    caps.sse3   = (ecx >>  0) & 1;
    caps.sse4_1 = (ecx >> 19) & 1;
    caps.sse4_2 = (ecx >> 20) & 1;
    caps.fma    = (ecx >> 12) & 1;
    caps.avx    = (ecx >> 28) & 1;

    // ── Leaf 7: AVX2 / AVX-512 ───────────────────────────────────────────────
    cpuid_count(7, 0, eax, ebx, ecx, edx);
    caps.avx2    = (ebx >>  5) & 1;
    caps.avx512f = (ebx >> 16) & 1;

    return caps;
}

// f32x4  — requires SSE + SSE2
// f32x4_hadd fallback requires SSE3
static inline int simd_supports_f32x4(const SIMD_Caps *c) { return c->sse && c->sse2; }
static inline int simd_supports_f32x4_hadd(const SIMD_Caps *c) { return simd_supports_f32x4(c) && c->sse3; }

// f32x8 / f64x4  — requires AVX
static inline int simd_supports_f32x8(const SIMD_Caps *c) { return c->avx; }
static inline int simd_supports_f64x4(const SIMD_Caps *c) { return c->avx; }

// FMA paths 
static inline int simd_supports_fma(const SIMD_Caps *c) { return c->fma; }

// AVX2 
static inline int simd_supports_avx2(const SIMD_Caps *c) { return c->avx2; }

static inline int simd_supports_all(const SIMD_Caps *c)
{
    return simd_supports_f32x4(c)
        && simd_supports_f32x8(c)
        && simd_supports_fma(c);
}

// ── Debug print ──────────────────────────────────────────────────────────────
static inline void simd_print_caps(const SIMD_Caps *c)
{
    printf("SIMD Capabilities:\n");
    printf("  SSE      : %s\n", c->sse     ? "yes" : "NO");
    printf("  SSE2     : %s\n", c->sse2    ? "yes" : "NO");
    printf("  SSE3     : %s\n", c->sse3    ? "yes" : "NO");
    printf("  SSE4.1   : %s\n", c->sse4_1  ? "yes" : "NO");
    printf("  SSE4.2   : %s\n", c->sse4_2  ? "yes" : "NO");
    printf("  AVX      : %s\n", c->avx     ? "yes" : "NO");
    printf("  AVX2     : %s\n", c->avx2    ? "yes" : "NO");
    printf("  FMA      : %s\n", c->fma     ? "yes" : "NO");
    printf("  AVX-512F : %s\n", c->avx512f ? "yes" : "NO");
    printf("\n");
    printf("  f32x4    : %s\n", simd_supports_f32x4(c)      ? "supported" : "MISSING");
    printf("  f32x4_hadd:%s\n", simd_supports_f32x4_hadd(c) ? "supported" : "fallback");
    printf("  f32x8    : %s\n", simd_supports_f32x8(c)      ? "supported" : "MISSING");
    printf("  f64x4    : %s\n", simd_supports_f64x4(c)      ? "supported" : "MISSING");
    printf("  FMA paths: %s\n", simd_supports_fma(c)        ? "supported" : "fallback");
}

#undef cpuid
#undef cpuid_count

// ============================================================================
// Construction & Loading
// ============================================================================

static inline f32x4 f32x4_set(float x, float y, float z, float w) 
{
    // ps : single percision fp
    return _mm_set_ps(w, z, y, x);
}

// fill with a single value
static inline f32x4 f32x4_set1(float x) 
{
    return _mm_set1_ps(x);
}

static inline f32x4 f32x4_zero(void) 
{
    return _mm_setzero_ps();
}

// un-aligned load from memory region
static inline f32x4 f32x4_load(const float *ptr) 
{
    return _mm_loadu_ps(ptr);
}

static inline f32x4 f32x4_load_aligned(const float *ptr) 
{
    return _mm_load_ps(ptr);
}

// un-aligned store to a memory region
static inline void f32x4_store(float *ptr, f32x4 v) 
{
    _mm_storeu_ps(ptr, v);
}

static inline void f32x4_store_aligned(float *ptr, f32x4 v) 
{
    _mm_store_ps(ptr, v);
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

static inline f32x4 f32x4_add(f32x4 a, f32x4 b) 
{
    return _mm_add_ps(a, b);
}

static inline f32x4 f32x4_sub(f32x4 a, f32x4 b) 
{
    return _mm_sub_ps(a, b);
}

static inline f32x4 f32x4_mul(f32x4 a, f32x4 b) 
{
    return _mm_mul_ps(a, b);
}

static inline f32x4 f32x4_div(f32x4 a, f32x4 b) 
{
    return _mm_div_ps(a, b);
}

static inline f32x4 f32x4_neg(f32x4 a) 
{
    return _mm_sub_ps(_mm_setzero_ps(), a);
}

static inline f32x4 f32x4_abs(f32x4 a) 
{
    f32x4 sign_mask = _mm_set1_ps(-0.0f);
    return _mm_andnot_ps(sign_mask, a);
}

// ============================================================================
// Fused Operations
// ============================================================================

#ifdef __FMA__
static inline f32x4 f32x4_madd(f32x4 a, f32x4 b, f32x4 c)
{
    // a * b + c
    return _mm_fmadd_ps(a, b, c);
}

static inline f32x4 f32x4_msub(f32x4 a, f32x4 b, f32x4 c)
{
    // a * b - c
    return _mm_fmsub_ps(a, b, c);
}

static inline f32x4 f32x4_nmadd(f32x4 a, f32x4 b, f32x4 c)
{
    // -(a * b) + c
    return _mm_fnmadd_ps(a, b, c);
}
#else
static inline f32x4 f32x4_madd(f32x4 a, f32x4 b, f32x4 c)
{
    return _mm_add_ps(_mm_mul_ps(a, b), c);
}

static inline f32x4 f32x4_msub(f32x4 a, f32x4 b, f32x4 c)
{
    return _mm_sub_ps(_mm_mul_ps(a, b), c);
}

static inline f32x4 f32x4_nmadd(f32x4 a, f32x4 b, f32x4 c)
{
    return _mm_sub_ps(c, _mm_mul_ps(a, b));
}
#endif

// ============================================================================
// Comparison Operations
// ============================================================================

static inline f32x4 f32x4_min(f32x4 a, f32x4 b) 
{
    return _mm_min_ps(a, b);
}

static inline f32x4 f32x4_max(f32x4 a, f32x4 b) 
{
    return _mm_max_ps(a, b);
}

static inline f32x4 f32x4_clamp(f32x4 v, f32x4 min_v, f32x4 max_v) 
{
    return _mm_min_ps(_mm_max_ps(v, min_v), max_v);
}

static inline f32x4 f32x4_cmp_eq(f32x4 a, f32x4 b) 
{
    return _mm_cmpeq_ps(a, b);
}

static inline f32x4 f32x4_cmp_neq(f32x4 a, f32x4 b) 
{
    return _mm_cmpneq_ps(a, b);
}

static inline f32x4 f32x4_cmp_lt(f32x4 a, f32x4 b) 
{
    return _mm_cmplt_ps(a, b);
}

static inline f32x4 f32x4_cmp_le(f32x4 a, f32x4 b) 
{
    return _mm_cmple_ps(a, b);
}

static inline f32x4 f32x4_cmp_gt(f32x4 a, f32x4 b) 
{
    return _mm_cmpgt_ps(a, b);
}

static inline f32x4 f32x4_cmp_ge(f32x4 a, f32x4 b) 
{
    return _mm_cmpge_ps(a, b);
}

// ============================================================================
// Bitwise Operations
// ============================================================================

static inline f32x4 f32x4_and(f32x4 a, f32x4 b) 
{
    return _mm_and_ps(a, b);
}

static inline f32x4 f32x4_or(f32x4 a, f32x4 b) 
{
    return _mm_or_ps(a, b);
}

static inline f32x4 f32x4_xor(f32x4 a, f32x4 b) 
{
    return _mm_xor_ps(a, b);
}

static inline f32x4 f32x4_andnot(f32x4 a, f32x4 b) 
{
    return _mm_andnot_ps(a, b);
}

// ============================================================================
// Math Functions
// ============================================================================

static inline f32x4 f32x4_sqrt(f32x4 a) 
{
    return _mm_sqrt_ps(a);
}

static inline f32x4 f32x4_rsqrt(f32x4 a) 
{
    // Fast reciprocal square root (approximate)
    return _mm_rsqrt_ps(a);
}

static inline f32x4 f32x4_rcp(f32x4 a) 
{
    // Fast reciprocal (approximate)
    return _mm_rcp_ps(a);
}

// ============================================================================
// Selection & Blending
// ============================================================================

static inline f32x4 f32x4_select(f32x4 mask, f32x4 a, f32x4 b) 
{
    // Where mask is all 1s, take a; otherwise take b
    return _mm_or_ps(_mm_and_ps(mask, a), _mm_andnot_ps(mask, b));
}

// ============================================================================
// Shuffling & Swizzling
// ============================================================================

// NOTE: indices are in logical left-to-right order [x=lane0, y=lane1, z=lane2, w=lane3]
// e.g. f32x4_swizzle(v, 0,0,0,0) splats lane 0 to all positions
#define f32x4_swizzle(v, x, y, z, w) \
    _mm_shuffle_ps(v, v, _MM_SHUFFLE(w, z, y, x))

static inline f32x4 f32x4_splat_x(f32x4 v) 
{
    return _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0));
}

static inline f32x4 f32x4_splat_y(f32x4 v) 
{
    return _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1));
}

static inline f32x4 f32x4_splat_z(f32x4 v) 
{
    return _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2));
}

static inline f32x4 f32x4_splat_w(f32x4 v) 
{
    return _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3));
}

// ============================================================================
// Horizontal Operations
// ============================================================================

static inline f32x4 f32x4_hadd(f32x4 a, f32x4 b) 
{
    // Horizontal add (requires SSE3, fallback provided)
#ifdef __SSE3__
    return _mm_hadd_ps(a, b);
#else
    f32x4 tmp1 = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0));
    f32x4 tmp2 = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1));
    return _mm_add_ps(tmp1, tmp2);
#endif
}

static inline float f32x4_sum(f32x4 v) 
{
    f32x4 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
    f32x4 sums = _mm_add_ps(v, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
}

static inline float f32x4_dot(f32x4 a, f32x4 b) 
{
    f32x4 mul = _mm_mul_ps(a, b);
    return f32x4_sum(mul);
}

// ============================================================================
// Vector Math
// ============================================================================

static inline float f32x4_length_sq(f32x4 v) 
{
    return f32x4_dot(v, v);
}

static inline float f32x4_length(f32x4 v) 
{
    return sqrtf(f32x4_length_sq(v));
}

static inline f32x4 f32x4_normalize(f32x4 v) 
{
    float len = f32x4_length(v);
    if (len > 0.0f) 
{
        return f32x4_div(v, f32x4_set1(len));
    }
    return f32x4_zero();
}

static inline f32x4 f32x4_normalize_fast(f32x4 v) 
{
    // Fast normalize using rsqrt
    f32x4 len_sq = f32x4_set1(f32x4_length_sq(v));
    f32x4 inv_len = f32x4_rsqrt(len_sq);
    return f32x4_mul(v, inv_len);
}

static inline f32x4 f32x4_lerp(f32x4 a, f32x4 b, float t) 
{
    f32x4 t_vec = f32x4_set1(t);
    return f32x4_madd(f32x4_sub(b, a), t_vec, a);
}

// Cross product (only uses xyz components)
static inline f32x4 f32x4_cross(f32x4 a, f32x4 b) 
{
    f32x4 a_yzx = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
    f32x4 b_yzx = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));
    f32x4 c = _mm_sub_ps(_mm_mul_ps(a, b_yzx), _mm_mul_ps(a_yzx, b));
    return _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 0, 2, 1));
}

// ============================================================================
// Extract Components
// ============================================================================

static inline float f32x4_get_x(f32x4 v) 
{
    return _mm_cvtss_f32(v);
}

static inline float f32x4_get_y(f32x4 v) 
{
    return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)));
}

static inline float f32x4_get_z(f32x4 v) 
{
    return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)));
}

static inline float f32x4_get_w(f32x4 v) 
{
    return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3)));
}

// ============================================================================
// Utility
// ============================================================================

static inline int f32x4_movemask(f32x4 v) 
{
    return _mm_movemask_ps(v);
}

static inline int f32x4_all_true(f32x4 v) 
{
    return _mm_movemask_ps(v) == 0xF;
}

static inline int f32x4_any_true(f32x4 v) 
{
    return _mm_movemask_ps(v) != 0;
}

static inline f32x4 f32x4_sort(f32x4 v)
{
    // Sorting network for 4 elements (6 comparators)
    // Optimal depth-3 network: [0,2][1,3] -> [0,1][2,3] -> [1,2]

    f32x4 a, b, lo, hi;

    // Step 1: compare [0,2] and [1,3]
    a  = v;
    b  = _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 0, 3, 2)); // [2,3,0,1]
    lo = _mm_min_ps(a, b);
    hi = _mm_max_ps(a, b);
    v  = _mm_blend_ps(lo, hi, 0b1100); // [lo0, lo1, hi2, hi3]

    // Step 2: compare [0,1] and [2,3]
    a  = v;
    b  = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1)); // [1,0,3,2]
    lo = _mm_min_ps(a, b);
    hi = _mm_max_ps(a, b);
    v  = _mm_blend_ps(lo, hi, 0b1010); // [lo0, hi1, lo2, hi3]

    // Step 3: compare [1,2]
    a  = v;
    b  = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 1, 2, 0)); // [0,2,1,3]
    lo = _mm_min_ps(a, b);
    hi = _mm_max_ps(a, b);
    v  = _mm_shuffle_ps(lo, hi, _MM_SHUFFLE(3, 1, 2, 0));

    return v;
}

#endif // f32x4_H

#ifndef F32X8_H
#define F32X8_H

// ============================================================================
// Construction & Loading
// ============================================================================

static inline f32x8 f32x8_set(float v0, float v1, float v2, float v3, 
                               float v4, float v5, float v6, float v7) 
{
    return _mm256_set_ps(v7, v6, v5, v4, v3, v2, v1, v0);
}

static inline f32x8 f32x8_set1(float x) 
{
    return _mm256_set1_ps(x);
}

static inline f32x8 f32x8_zero(void) 
{
    return _mm256_setzero_ps();
}

static inline f32x8 f32x8_load(const float *ptr) 
{
    return _mm256_loadu_ps(ptr);
}

static inline f32x8 f32x8_load_aligned(const float *ptr) 
{
    return _mm256_load_ps(ptr);
}

static inline void f32x8_store(float *ptr, f32x8 v) 
{
    _mm256_storeu_ps(ptr, v);
}

static inline void f32x8_store_aligned(float *ptr, f32x8 v) 
{
    _mm256_store_ps(ptr, v);
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

static inline f32x8 f32x8_add(f32x8 a, f32x8 b) 
{
    return _mm256_add_ps(a, b);
}

static inline f32x8 f32x8_sub(f32x8 a, f32x8 b) 
{
    return _mm256_sub_ps(a, b);
}

static inline f32x8 f32x8_mul(f32x8 a, f32x8 b) 
{
    return _mm256_mul_ps(a, b);
}

static inline f32x8 f32x8_div(f32x8 a, f32x8 b) 
{
    return _mm256_div_ps(a, b);
}

static inline f32x8 f32x8_neg(f32x8 a) 
{
    return _mm256_sub_ps(_mm256_setzero_ps(), a);
}

static inline f32x8 f32x8_abs(f32x8 a) 
{
    f32x8 sign_mask = _mm256_set1_ps(-0.0f);
    return _mm256_andnot_ps(sign_mask, a);
}

// ============================================================================
// Fused Operations (FMA3 required)
// ============================================================================

#ifdef __FMA__
static inline f32x8 f32x8_madd(f32x8 a, f32x8 b, f32x8 c) 
{
    // a * b + c
    return _mm256_fmadd_ps(a, b, c);
}

static inline f32x8 f32x8_msub(f32x8 a, f32x8 b, f32x8 c) 
{
    // a * b - c
    return _mm256_fmsub_ps(a, b, c);
}

static inline f32x8 f32x8_nmadd(f32x8 a, f32x8 b, f32x8 c) 
{
    // -(a * b) + c
    return _mm256_fnmadd_ps(a, b, c);
}
#else
static inline f32x8 f32x8_madd(f32x8 a, f32x8 b, f32x8 c) 
{
    return _mm256_add_ps(_mm256_mul_ps(a, b), c);
}

static inline f32x8 f32x8_msub(f32x8 a, f32x8 b, f32x8 c) 
{
    return _mm256_sub_ps(_mm256_mul_ps(a, b), c);
}

static inline f32x8 f32x8_nmadd(f32x8 a, f32x8 b, f32x8 c) 
{
    return _mm256_sub_ps(c, _mm256_mul_ps(a, b));
}
#endif

// ============================================================================
// Comparison Operations
// ============================================================================

static inline f32x8 f32x8_min(f32x8 a, f32x8 b) 
{
    return _mm256_min_ps(a, b);
}

static inline f32x8 f32x8_max(f32x8 a, f32x8 b) 
{
    return _mm256_max_ps(a, b);
}

static inline f32x8 f32x8_clamp(f32x8 v, f32x8 min_v, f32x8 max_v) 
{
    return _mm256_min_ps(_mm256_max_ps(v, min_v), max_v);
}

static inline f32x8 f32x8_cmp_eq(f32x8 a, f32x8 b) 
{
    return _mm256_cmp_ps(a, b, _CMP_EQ_OQ);
}

static inline f32x8 f32x8_cmp_neq(f32x8 a, f32x8 b) 
{
    return _mm256_cmp_ps(a, b, _CMP_NEQ_OQ);
}

static inline f32x8 f32x8_cmp_lt(f32x8 a, f32x8 b) 
{
    return _mm256_cmp_ps(a, b, _CMP_LT_OQ);
}

static inline f32x8 f32x8_cmp_le(f32x8 a, f32x8 b) 
{
    return _mm256_cmp_ps(a, b, _CMP_LE_OQ);
}

static inline f32x8 f32x8_cmp_gt(f32x8 a, f32x8 b) 
{
    return _mm256_cmp_ps(a, b, _CMP_GT_OQ);
}

static inline f32x8 f32x8_cmp_ge(f32x8 a, f32x8 b) 
{
    return _mm256_cmp_ps(a, b, _CMP_GE_OQ);
}

// ============================================================================
// Bitwise Operations
// ============================================================================

static inline f32x8 f32x8_and(f32x8 a, f32x8 b) 
{
    return _mm256_and_ps(a, b);
}

static inline f32x8 f32x8_or(f32x8 a, f32x8 b) 
{
    return _mm256_or_ps(a, b);
}

static inline f32x8 f32x8_xor(f32x8 a, f32x8 b) 
{
    return _mm256_xor_ps(a, b);
}

static inline f32x8 f32x8_andnot(f32x8 a, f32x8 b) 
{
    return _mm256_andnot_ps(a, b);
}

// ============================================================================
// Math Functions
// ============================================================================

static inline f32x8 f32x8_sqrt(f32x8 a) 
{
    return _mm256_sqrt_ps(a);
}

static inline f32x8 f32x8_rsqrt(f32x8 a) 
{
    return _mm256_rsqrt_ps(a);
}

static inline f32x8 f32x8_rcp(f32x8 a) 
{
    return _mm256_rcp_ps(a);
}

// ============================================================================
// Selection & Blending
// ============================================================================

static inline f32x8 f32x8_select(f32x8 mask, f32x8 a, f32x8 b) 
{
    return _mm256_blendv_ps(b, a, mask);
}

static inline f32x8 f32x8_if_gt(f32x8 a, f32x8 b, f32x8 true_val, f32x8 false_val) 
{
    return f32x8_select(f32x8_cmp_gt(a, b), true_val, false_val);
}

static inline f32x8 f32x8_if_lt(f32x8 a, f32x8 b, f32x8 true_val, f32x8 false_val) 
{
    return f32x8_select(f32x8_cmp_lt(a, b), true_val, false_val);
}

static inline f32x8 f32x8_if_eq(f32x8 a, f32x8 b, f32x8 true_val, f32x8 false_val) 
{
    return f32x8_select(f32x8_cmp_eq(a, b), true_val, false_val);
}


// ============================================================================
// Shuffling & Permuting
// ============================================================================

// Extract lower/upper 128-bit lanes as f32x4 (SSE)
static inline __m128 f32x8_get_low(f32x8 v) 
{
    return _mm256_castps256_ps128(v);
}

static inline __m128 f32x8_get_high(f32x8 v) 
{
    return _mm256_extractf128_ps(v, 1);
}

// Combine two 128-bit vectors into one 256-bit
static inline f32x8 f32x8_set_lanes(__m128 lo, __m128 hi) 
{
    return _mm256_set_m128(hi, lo);
}

// Broadcast one lane to all positions
static inline f32x8 f32x8_broadcast_lane(f32x8 v, int lane)
{
    switch(lane & 7)
    {
        case 0: return _mm256_permute_ps(_mm256_permute2f128_ps(v, v, 0x00), 0x00);
        case 1: return _mm256_permute_ps(_mm256_permute2f128_ps(v, v, 0x00), 0x55);
        case 2: return _mm256_permute_ps(_mm256_permute2f128_ps(v, v, 0x00), 0xAA);
        case 3: return _mm256_permute_ps(_mm256_permute2f128_ps(v, v, 0x00), 0xFF);
        case 4: return _mm256_permute_ps(_mm256_permute2f128_ps(v, v, 0x11), 0x00);
        case 5: return _mm256_permute_ps(_mm256_permute2f128_ps(v, v, 0x11), 0x55);
        case 6: return _mm256_permute_ps(_mm256_permute2f128_ps(v, v, 0x11), 0xAA);
        case 7: return _mm256_permute_ps(_mm256_permute2f128_ps(v, v, 0x11), 0xFF);
        default: return v;
    }
}

// ============================================================================
// Horizontal Operations
// ============================================================================

static inline f32x8 f32x8_hadd(f32x8 a, f32x8 b) 
{
    return _mm256_hadd_ps(a, b);
}

static inline float f32x8_sum(f32x8 v) 
{
    // Sum all 8 elements
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 sum4 = _mm_add_ps(lo, hi);
    __m128 shuf = _mm_shuffle_ps(sum4, sum4, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(sum4, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
}

static inline float f32x8_dot(f32x8 a, f32x8 b) 
{
    f32x8 mul = _mm256_mul_ps(a, b);
    return f32x8_sum(mul);
}

// ============================================================================
// Extract Individual Components
// ============================================================================

static inline float f32x8_get(f32x8 v, int index) 
{
    float arr[8] __attribute__((aligned(32)));
    _mm256_store_ps(arr, v);
    return arr[index & 7];
}

static inline float f32x8_get_0(f32x8 v) 
{
    return _mm_cvtss_f32(_mm256_castps256_ps128(v));
}

static inline float f32x8_get_1(f32x8 v) 
{
    __m128 lo = _mm256_castps256_ps128(v);
    return _mm_cvtss_f32(_mm_shuffle_ps(lo, lo, 1));
}

static inline float f32x8_get_4(f32x8 v) 
{
    return _mm_cvtss_f32(_mm256_extractf128_ps(v, 1));
}

// ============================================================================
// Utility
// ============================================================================

static inline int f32x8_movemask(f32x8 v) 
{
    return _mm256_movemask_ps(v);
}

static inline int f32x8_all_true(f32x8 v) 
{
    return _mm256_movemask_ps(v) == 0xFF;
}

static inline int f32x8_any_true(f32x8 v) 
{
    return _mm256_movemask_ps(v) != 0;
}


static inline f32x8 f32x8_sort(f32x8 v)
{
    f32x8 a, b, lo, hi;

    #define CMP_SWAP(reg, shuf, blend_mask)          \
        a  = reg;                                     \
        b  = _mm256_shuffle_ps(reg, reg, shuf);       \
        lo = _mm256_min_ps(a, b);                     \
        hi = _mm256_max_ps(a, b);                     \
        reg = _mm256_blend_ps(lo, hi, blend_mask);

    // Step 1: [0,1][2,3][4,5][6,7]
    CMP_SWAP(v, _MM_SHUFFLE(2,3,0,1), 0b10101010);

    // Step 2: [0,2][1,3][4,6][5,7]
    CMP_SWAP(v, _MM_SHUFFLE(1,0,3,2), 0b11001100);

    // Step 3: [1,2][5,6]
    CMP_SWAP(v, _MM_SHUFFLE(2,3,0,1), 0b01100110);

    // Step 4: [0,4][1,5][2,6][3,7]  (cross-lane)
    a  = v;
    b  = _mm256_permute2f128_ps(v, v, 0x01);   // swap 128-bit lanes
    lo = _mm256_min_ps(a, b);
    hi = _mm256_max_ps(a, b);
    v  = _mm256_blend_ps(lo, hi, 0b11110000);

    // Step 5: [2,4][3,5]  (cross-lane)
    a  = v;
    b  = _mm256_permute_ps(v, _MM_SHUFFLE(1,0,3,2));
    b  = _mm256_permute2f128_ps(b, b, 0x01);
    lo = _mm256_min_ps(a, b);
    hi = _mm256_max_ps(a, b);
    v  = _mm256_blend_ps(lo, hi, 0b00111100);

    // Step 6: [1,2][3,4][5,6]
    CMP_SWAP(v, _MM_SHUFFLE(2,3,0,1), 0b01100110);

    #undef CMP_SWAP

    return v;
}

// ============================================================================
// Print Helper
// ============================================================================


static inline void f32x8_print(f32x8 v, const char *name) 
{
    float arr[8] __attribute__((aligned(32)));
    f32x8_store_aligned(arr, v);
    printf("%s: [%.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f]\n", 
           name, arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7]);
}

static inline void f32x8_print_detailed(f32x8 v, const char *name) 
{
    float arr[8];
    f32x8_store(arr, v);
    printf("%s:\n", name);
    for (int i = 0; i < 8; i++) 
{
        printf("  [%d]: %.6f\n", i, arr[i]);
    }
}

#endif // F32X8_H


#ifndef F64X4_H
#define F64X4_H


// ============================================================================
// Construction & Loading
// ============================================================================

static inline f64x4 f64x4_set(double v0, double v1, double v2, double v3) 
{
    return _mm256_set_pd(v3, v2, v1, v0);
}

static inline f64x4 f64x4_set1(double x) 
{
    return _mm256_set1_pd(x);
}

static inline f64x4 f64x4_zero(void) 
{
    return _mm256_setzero_pd();
}

static inline f64x4 f64x4_load(const double *ptr) 
{
    return _mm256_loadu_pd(ptr);
}

static inline f64x4 f64x4_load_aligned(const double *ptr) 
{
    return _mm256_load_pd(ptr);
}

static inline void f64x4_store(double *ptr, f64x4 v) 
{
    _mm256_storeu_pd(ptr, v);
}

static inline void f64x4_store_aligned(double *ptr, f64x4 v) 
{
    _mm256_store_pd(ptr, v);
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

static inline f64x4 f64x4_add(f64x4 a, f64x4 b) 
{
    return _mm256_add_pd(a, b);
}

static inline f64x4 f64x4_sub(f64x4 a, f64x4 b) 
{
    return _mm256_sub_pd(a, b);
}

static inline f64x4 f64x4_mul(f64x4 a, f64x4 b) 
{
    return _mm256_mul_pd(a, b);
}

static inline f64x4 f64x4_div(f64x4 a, f64x4 b) 
{
    return _mm256_div_pd(a, b);
}

static inline f64x4 f64x4_neg(f64x4 a) 
{
    return _mm256_sub_pd(_mm256_setzero_pd(), a);
}

static inline f64x4 f64x4_abs(f64x4 a) 
{
    f64x4 sign_mask = _mm256_set1_pd(-0.0);
    return _mm256_andnot_pd(sign_mask, a);
}

// ============================================================================
// Fused Operations (FMA3 required)
// ============================================================================

#ifdef __FMA__
static inline f64x4 f64x4_madd(f64x4 a, f64x4 b, f64x4 c) 
{
    // a * b + c
    return _mm256_fmadd_pd(a, b, c);
}

static inline f64x4 f64x4_msub(f64x4 a, f64x4 b, f64x4 c) 
{
    // a * b - c
    return _mm256_fmsub_pd(a, b, c);
}

static inline f64x4 f64x4_nmadd(f64x4 a, f64x4 b, f64x4 c) 
{
    // -(a * b) + c
    return _mm256_fnmadd_pd(a, b, c);
}
#else
static inline f64x4 f64x4_madd(f64x4 a, f64x4 b, f64x4 c) 
{
    return _mm256_add_pd(_mm256_mul_pd(a, b), c);
}

static inline f64x4 f64x4_msub(f64x4 a, f64x4 b, f64x4 c) 
{
    return _mm256_sub_pd(_mm256_mul_pd(a, b), c);
}

static inline f64x4 f64x4_nmadd(f64x4 a, f64x4 b, f64x4 c) 
{
    return _mm256_sub_pd(c, _mm256_mul_pd(a, b));
}
#endif

// ============================================================================
// Comparison Operations
// ============================================================================

static inline f64x4 f64x4_min(f64x4 a, f64x4 b) 
{
    return _mm256_min_pd(a, b);
}

static inline f64x4 f64x4_max(f64x4 a, f64x4 b) 
{
    return _mm256_max_pd(a, b);
}

static inline f64x4 f64x4_clamp(f64x4 v, f64x4 min_v, f64x4 max_v) 
{
    return _mm256_min_pd(_mm256_max_pd(v, min_v), max_v);
}

static inline f64x4 f64x4_cmp_eq(f64x4 a, f64x4 b) 
{
    return _mm256_cmp_pd(a, b, _CMP_EQ_OQ);
}

static inline f64x4 f64x4_cmp_neq(f64x4 a, f64x4 b) 
{
    return _mm256_cmp_pd(a, b, _CMP_NEQ_OQ);
}

static inline f64x4 f64x4_cmp_lt(f64x4 a, f64x4 b) 
{
    return _mm256_cmp_pd(a, b, _CMP_LT_OQ);
}

static inline f64x4 f64x4_cmp_le(f64x4 a, f64x4 b) 
{
    return _mm256_cmp_pd(a, b, _CMP_LE_OQ);
}

static inline f64x4 f64x4_cmp_gt(f64x4 a, f64x4 b) 
{
    return _mm256_cmp_pd(a, b, _CMP_GT_OQ);
}

static inline f64x4 f64x4_cmp_ge(f64x4 a, f64x4 b) 
{
    return _mm256_cmp_pd(a, b, _CMP_GE_OQ);
}

// ============================================================================
// Bitwise Operations
// ============================================================================

static inline f64x4 f64x4_and(f64x4 a, f64x4 b) 
{
    return _mm256_and_pd(a, b);
}

static inline f64x4 f64x4_or(f64x4 a, f64x4 b) 
{
    return _mm256_or_pd(a, b);
}

static inline f64x4 f64x4_xor(f64x4 a, f64x4 b) 
{
    return _mm256_xor_pd(a, b);
}

static inline f64x4 f64x4_andnot(f64x4 a, f64x4 b) 
{
    return _mm256_andnot_pd(a, b);
}

// ============================================================================
// Math Functions
// ============================================================================

static inline f64x4 f64x4_sqrt(f64x4 a) 
{
    return _mm256_sqrt_pd(a);
}

// Note: There is no _mm256_rsqrt_pd or _mm256_rcp_pd in AVX
// These would need to be implemented with division or Newton-Raphson

static inline f64x4 f64x4_rcp(f64x4 a) 
{
    return _mm256_div_pd(_mm256_set1_pd(1.0), a);
}

static inline f64x4 f64x4_rsqrt(f64x4 a) 
{
    return _mm256_div_pd(_mm256_set1_pd(1.0), _mm256_sqrt_pd(a));
}

// ============================================================================
// Selection & Blending
// ============================================================================

static inline f64x4 f64x4_select(f64x4 mask, f64x4 a, f64x4 b) 
{
    return _mm256_blendv_pd(b, a, mask);
}

// ============================================================================
// Shuffling & Permuting
// ============================================================================

// Extract lower/upper 128-bit lanes as vec2d (SSE)
static inline __m128d f64x4_get_low(f64x4 v) 
{
    return _mm256_castpd256_pd128(v);
}

static inline __m128d f64x4_get_high(f64x4 v) 
{
    return _mm256_extractf128_pd(v, 1);
}

// Combine two 128-bit vectors into one 256-bit
static inline f64x4 f64x4_set_lanes(__m128d lo, __m128d hi) 
{
    return _mm256_set_m128d(hi, lo);
}

// Broadcast one lane to all positions
static inline f64x4 f64x4_broadcast_lane(f64x4 v, int lane)
{
    switch(lane & 3)
    {
        case 0: return _mm256_permute_pd(_mm256_permute2f128_pd(v, v, 0x00), 0x0);
        case 1: return _mm256_permute_pd(_mm256_permute2f128_pd(v, v, 0x00), 0xF);
        case 2: return _mm256_permute_pd(_mm256_permute2f128_pd(v, v, 0x11), 0x0);
        case 3: return _mm256_permute_pd(_mm256_permute2f128_pd(v, v, 0x11), 0xF);
        default: return v;
    }
}
// ============================================================================
// Horizontal Operations
// ============================================================================

static inline f64x4 f64x4_hadd(f64x4 a, f64x4 b) 
{
    return _mm256_hadd_pd(a, b);
}

static inline double f64x4_sum(f64x4 v) 
{
    // Sum all 4 elements
    __m128d lo = _mm256_castpd256_pd128(v);
    __m128d hi = _mm256_extractf128_pd(v, 1);
    __m128d sum2 = _mm_add_pd(lo, hi);
    __m128d shuf = _mm_shuffle_pd(sum2, sum2, 1);
    __m128d sums = _mm_add_sd(sum2, shuf);
    return _mm_cvtsd_f64(sums);
}

static inline double f64x4_dot(f64x4 a, f64x4 b) 
{
    f64x4 mul = _mm256_mul_pd(a, b);
    return f64x4_sum(mul);
}

// ============================================================================
// Extract Individual Components
// ============================================================================

static inline double f64x4_get(f64x4 v, int index) 
{
    double arr[4] __attribute__((aligned(32)));
    _mm256_store_pd(arr, v);
    return arr[index & 3];
}

static inline double f64x4_get_0(f64x4 v) 
{
    return _mm_cvtsd_f64(_mm256_castpd256_pd128(v));
}

static inline double f64x4_get_1(f64x4 v) 
{
    __m128d lo = _mm256_castpd256_pd128(v);
    return _mm_cvtsd_f64(_mm_shuffle_pd(lo, lo, 1));
}

static inline double f64x4_get_2(f64x4 v) 
{
    return _mm_cvtsd_f64(_mm256_extractf128_pd(v, 1));
}

// ============================================================================
// Utility
// ============================================================================

static inline int f64x4_movemask(f64x4 v) 
{
    return _mm256_movemask_pd(v);
}

static inline int f64x4_all_true(f64x4 v) 
{
    return _mm256_movemask_pd(v) == 0xF;
}

static inline int f64x4_any_true(f64x4 v) 
{
    return _mm256_movemask_pd(v) != 0;
}

// ============================================================================
// Print Helper
// ============================================================================


static inline void f64x4_print(f64x4 v, const char *name) 
{
    double arr[4] __attribute__((aligned(32)));
    f64x4_store_aligned(arr, v);
    printf("%s: [%.6f, %.6f, %.6f, %.6f]\n", 
           name, arr[0], arr[1], arr[2], arr[3]);
}

static inline void f64x4_print_detailed(f64x4 v, const char *name) 
{
    double arr[4];
    f64x4_store(arr, v);
    printf("%s:\n", name);
    for (int i = 0; i < 4; i++) {
        printf("  [%d]: %.12f\n", i, arr[i]);
    }
}

#endif // F64X4_H