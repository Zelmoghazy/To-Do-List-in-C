#ifndef UTIL_H_
#define UTIL_H_

#ifdef _WIN32
    #include <windows.h>
    #include <winnt.h>
    #include <uxtheme.h>
    #include <dwmapi.h>
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <locale.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <float.h>

#include <immintrin.h> 

#define DEBUG            1
#define LOG              1

/* OS */
# if defined(_WIN32)
    # define OS_WINDOWS 1
# elif defined(__gnu_linux__)
    # define OS_LINUX 1
# elif defined(__APPLE__) && defined(__MACH__)
    # define OS_MAC 1
# else
    # error cannot recognize the OS
# endif

/* Compiler */
#if defined(__clang__)
    #define COMPILER_CLANG 1
#elif defined(_MSC_VER)
    #define COMPILER_CL 1
#elif defined(__GNUC__)
    #define COMPILER_GCC 1
#else
    #error unrecognized compiler !
#endif

/* Architecture */
# if defined(__amd64__) || defined(_M_AMD64)
    # define ARCH_X64 1
# elif defined(__i386__) || defined(_M_I86)
    # define ARCH_X86 1
# elif defined(__arm__) || defined(_M_ARM)
    # define ARCH_ARM 1
# elif defined(__aarch64__)
    # define ARCH_ARM64 1
# else
    # error unrecognized Architecture
# endif

/* Pack structs */
#if defined(_MSC_VER)
    #define NO_PADDING_BEGIN __pragma(pack(push, 1))
    #define NO_PADDING_END   __pragma(pack(pop))
    #define NO_PADDING
#elif defined(__GNUC__) || defined(__clang__)
    /* GCC / Clang */
    #define NO_PADDING_BEGIN
    #define NO_PADDING_END
    #define NO_PADDING __attribute__((packed))

#else
    #define NO_PADDING_BEGIN
    #define NO_PADDING_END
    #define NO_PADDING
#endif

/* Force inline */
#if defined(_MSC_VER)
  #define FORCE_INLINE __forceinline
#else
  #define FORCE_INLINE __attribute__((always_inline)) inline
#endif

/* Dont inline */
#if defined(_MSC_VER)
    #define NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    #define NOINLINE __attribute__((noinline))
#else
    #define NOINLINE
#endif

/* Align */
#if defined(_MSC_VER)
    #define ALIGN_TO(x) __declspec(align(x))
#elif defined(__GNUC__) || defined(__clang__)
    #define ALIGN_TO(x) __attribute__((aligned(x)))
#else
    #define ALIGN_TO(x)
#endif


#if defined(_MSC_VER)
  #ifdef BUILDING_DLL
    #define MYAPI __declspec(dllexport)
  #else
    #define MYAPI __declspec(dllimport)
  #endif
#else
  #define MYAPI __attribute__((visibility("default")))
#endif

/* Mark deprecated */
#if defined(_MSC_VER)
    #define DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__) || defined(__clang__)
    #define DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
    #define DEPRECATED(msg)
#endif

/* warn if result not used */
#if defined(_MSC_VER)
    #define WARN_UNUSED_RESULT _Check_return_
#elif defined(__GNUC__) || defined(__clang__)
    #define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
    #define WARN_UNUSED_RESULT
#endif

#if defined(_MSC_VER)
    #define RESTRICT_RET __declspec(restrict)
    #define RESTRICT     __restrict
#elif defined(__GNUC__) || defined(__clang__)
    #define RESTRICT_RET __attribute__((malloc))
    #define RESTRICT     __restrict__
#else
    #define RESTRICT_RET
    #define RESTRICT
#endif

/* Thread Local */
#ifdef _WIN32
    #define THREAD_LOCAL __declspec(thread)
#else
    #define THREAD_LOCAL __thread
#endif

#if defined(_MSC_VER)
    #define SELECTANY __declspec(selectany)
#elif defined(__GNUC__) || defined(__clang__)
    #define SELECTANY __attribute__((weak))
#else
    #define SELECTANY
#endif

/* Helper for GCC/Clang pragma stringification */
#if defined(__GNUC__) || defined(__clang__)
    #define DO_PRAGMA(X) _Pragma(#X)
#endif

/* Warning Push/Pop and Disable */
#if defined(_MSC_VER)
    #define WARNINGS_PUSH __pragma(warning(push))
    #define WARNINGS_POP  __pragma(warning(pop))
    #define DISABLE_WARNING_MSVC(warnCode) __pragma(warning(disable: warnCode))
    #define DISABLE_WARNING_GCC(warnStr)
    #define DISABLE_WARNING_CLANG(warnStr)

#elif defined(__clang__)
    #define WARNINGS_PUSH DO_PRAGMA(clang diagnostic push)
    #define WARNINGS_POP  DO_PRAGMA(clang diagnostic pop)
    #define DISABLE_WARNING_MSVC(warnCode)
    #define DISABLE_WARNING_GCC(warnStr)
    #define DISABLE_WARNING_CLANG(warnStr) DO_PRAGMA(clang diagnostic ignored warnStr)

#elif defined(__GNUC__)
    #define WARNINGS_PUSH DO_PRAGMA(GCC diagnostic push)
    #define WARNINGS_POP  DO_PRAGMA(GCC diagnostic pop)
    #define DISABLE_WARNING_MSVC(warnCode)
    #define DISABLE_WARNING_GCC(warnStr)   DO_PRAGMA(GCC diagnostic ignored warnStr)
    #define DISABLE_WARNING_CLANG(warnStr)

#else
    #define WARNINGS_PUSH
    #define WARNINGS_POP
    #define DISABLE_WARNING_MSVC(warnCode)
    #define DISABLE_WARNING_GCC(warnStr)
    #define DISABLE_WARNING_CLANG(warnStr)
#endif

#define TAB_SIZE                    4

#define M_PI                        3.14159265358979323846
#define M_PI_2                      1.57079632679489661923
#define TAU                         6.28318530717958647692
#define EULER                       2.71828182846 
#define SQRT2_OVER_2                0.70710678118f
#define SQRT3                       1.73205080757f

#define ArrayCount(x)               ((sizeof(x))/(sizeof((x)[0])))

/* Very useful when wanting to use pointers as ids  */
#define IntFromPtr(p)               (u64)((u8*)p - (u8*)0)
#define PtrFromInt(n)               (void*)((u8*)0 + (n))

#define Member(T,m)                 (((T*)0)->m)
#define OffsetOfMember(T,m)         IntFromPtr(&Member(T,m))

#define MemoryZero(p,z)             memset((p), 0, (z))
#define MemoryZeroStruct(p)         MemoryZero((p), sizeof(*(p)))
#define MemoryZeroArray(p)          MemoryZero((p), sizeof(p))
#define MemoryZeroTyped(p,c)        MemoryZero((p), sizeof(*(p))*(c))

#define MemoryMatch(a,b,z)          (memcmp((a),(b),(z)) == 0)

#define MemoryCopy(d,s,z)           memmove((d), (s), (z))
#define MemoryCopyStruct(d,s)       MemoryCopy((d),(s),\
                                    MIN(sizeof(*(d)),sizeof(*(s))))

#define MemoryCopyArray(d,s)        MemoryCopy((d),(s),Min(sizeof(s),sizeof(d)))
#define MemoryCopyTyped(d,s,c)      MemoryCopy((d),(s),\
                                    MIN(sizeof(*(d)),sizeof(*(s)))*(c))


/*
    Mainly to solve this scenario :
    --------------------------------
    #define FOO(n)   foo(n);bar(n)

    void foobar(int n) {
        if (n)
            FOO(n);
    }

    Which would be expanded to:

    void foobar(int n) {
        if (n)
            foo(n);bar(n);
    }

    which may not be what is intended
 */
#define Stmnt(S)                    do{ S }while(0)


#define AssertBreak()               (*(int*)0=0)  // should reliably crash

#if DEBUG
    #define Assert(c) Stmnt(if((!c)){AssertBreak();})
#else
    #define Assert(c)
#endif


#define DEBUG_PRT(fmt, ...)\
    do{\
        if(DEBUG)\
            fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__);\
    }while(0)

#define Stringify_(S) #S
#define Stringify(S) Stringify_(S)

#define Glue_(A,B) A##B
#define Glue(A,B) Glue_(A,B)

#define ENUM_GEN(ENUM)     ENUM,
#define STRING_GEN(STRING) #STRING,

/* 
// Add enum list here 
#define FOREACH_ENUM(ENUM) \
    ENUM(ENUM_COUNT) 

typedef enum ENUM_T {
    FOREACH_ENUM(ENUM_GEN)
}ENUM_T;

const char* enum_strings[] = {
    FOREACH_ENUM(STRING_GEN)
}; 
*/

#define FOREACH(item, array)                                 \
    for (int _keep = 1,                                      \
             _count = 0,                                     \
             _size = sizeof(array) / sizeof *(array);        \
         _keep && _count != _size;                           \
         _keep = !_keep, _count++)                           \
        for (item = (array) + _count; _keep; _keep = !_keep)


#define SET(A, n, v)                               \
    do{                                            \
        size_t i_, n_;                             \
        for (n_ = (n), i_ = 0; n_ > 0; --n_, ++i_) \
            (A)[i_] = (v);                         \
    } while (0)

#define SWAP(x, y, T) \
    do{               \
        T tmp = (x);  \
        (x) = (y);    \
        (y) = tmp;    \
    } while (0)

#define DEFER_BLOCK(begin, end) \
    for(int _defer_ = ((begin), 0); !_defer_; _defer_ = 1, (end))

#define EPSILON_F32                     (1e-6f)
#define EPSILON_F64                     (1e-12)
#define COMPARE_FLOATS(a,b,epsilon)     (fabs(a - b) <= epsilon * fabs(a))

#define MASK(w,m,f)                     (w ^= (-f ^ w) & m)            // Conditionally set or clear bits  if (f) is true ->  w |= m; else w &= ~m;
#define SIGN(v)                         ((v) > 0) - ((v) < 0)
#define ABS(v)                          ((v < 0) ? -(unsigned)v : v)

#define NEG(v,f)                        ((v ^ -f) + f)
#define TOGGLE(B)                       ((B)^=1)

#define KB(n)                           (((u64)(n)) << 10)
#define MB(n)                           (((u64)(n)) << 20)
#define GB(n)                           (((u64)(n)) << 30)
#define TB(n)                           (((u64)(n)) << 40)

#define Thousand(n)                     ((n)*1000)
#define Million(n)                      ((n)*1000000)
#define Billion(n)                      ((n)*1000000000)

#define MAX(a, b)                       ((a) > (b) ? (a) : (b))
#define MIN(a, b)                       ((a) < (b) ? (a) : (b))

#define MAX3(a,b,c)                     ((a) > (b) ? ((a) > (c) ? (a) : (c)) : ((b) > (c) ? (b) : (c)))
#define MIN3(a,b,c)                     ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

#define MAX_F32(a, b)                   ((COMPARE_FLOATS((a), (b), EPSILON_F32) || (a) > (b)) ? (a) : (b))
#define MIN_F32(a, b)                   ((COMPARE_FLOATS((a), (b), EPSILON_F32) || (a) < (b)) ? (a) : (b))

#define MAX_F64(a, b)                   ((COMPARE_FLOATS((a), (b), EPSILON_F64) || (a) > (b)) ? (a) : (b))
#define MIN_F64(a, b)                   ((COMPARE_FLOATS((a), (b), EPSILON_F64) || (a) < (b)) ? (a) : (b))

#define MAX3_F32(a, b, c)               (MAX_F32(MAX_F32((a), (b)), (c)))
#define MIN3_F32(a, b, c)               (MIN_F32(MIN_F32((a), (b)), (c)))

#define MAX3_F64(a, b, c)               (MAX_F64(MAX_F64((a), (b)), (c)))
#define MIN3_F64(a, b, c)               (MIN_F64(MIN_F64((a), (b)), (c)))

#define CEIL_DIV(x, y)                  (((x) + (y) - 1) / (y))
/* 
    round x up to the nearest multiple of y
 */
#define ALIGN_UP(x, y)                  ((((x) + (y) - 1) / (y)) * (y))

#define FPS(n)                          (1000/n)

// handles overflow
#define MIDPOINT(start, end)            ((start) + ((end) - (start)) / 2)

#define LEN_BETWEEN(start, end, include_end) \
    ((end) >= (start) ? (size_t)((end) - (start)) + ((include_end) ? 1 : 0) : 0)

#define EDGE_INTERSECT_FP(y, y1, y2, x1, x2) \
    ((((y) - (y1)) * ((x2) - (x1)) * 256) / ((y2) - (y1)) + (x1) * 256)

#define RANGE_CONVERT(value, from_min, from_max, to_min, to_max) \
    (((value) - (from_min)) * ((to_max) - (to_min)) / ((from_max) - (from_min)) + (to_min))

// Normalize a value from arbitrary range to [0 -> 1]
#define NORMALIZE(val, min, max)             (((val) - (min)) / ((max) - (min)))

// handle negative numbers
#define RING_WRAP_INDEX(pos, size)           (((pos) + (size)) % (size))

// always prefer the pow of 2 if you can
#define RING_INC_WRAP(pos, size)             (((pos) + (1)) % (size))
#define RING_INC_WRAP_POW2(pos, size)        (((pos) + 1) & ((size) - 1))

#define RING_IN_RANGE_WRAP(a,start,end)      (((end) < (start)) ? \
                                             ((a) >= (start) || (a) <= (end)) : \
                                             ((a) >= (start) && (a) <= (end)) )

#define RING_COPY_OUT(dst, ring, start, count, capacity, type)                 \
    do {                                                                       \
        size_t _first = (capacity) - (start);                                  \
        if (_first >= (count)) {                                               \
            memcpy((dst), &(ring)[(start)], (count) * sizeof(type));           \
        } else {                                                               \
            size_t _second = (count) - _first;                                 \
            memcpy((dst), &(ring)[(start)], _first * sizeof(type));            \
            memcpy((dst) + _first, (ring), _second * sizeof(type));            \
        }                                                                      \
    } while (0)

#define RING_COPY_IN(ring, src, start, count, capacity, type)                  \
    do {                                                                       \
        size_t _first = (capacity) - (start);                                  \
        if (_first >= (count)) {                                               \
            memcpy(&(ring)[(start)], (src), (count) * sizeof(type));           \
        } else {                                                               \
            size_t _second = (count) - _first;                                 \
            memcpy(&(ring)[(start)], (src), _first * sizeof(type));            \
            memcpy((ring), ((type*)(src)) + _first, _second * sizeof(type));   \
        }                                                                      \
    } while (0)

#define FRAND_MAX                       32767  

#define RAND_FLOAT()                    (f32)fast_rand() / ((f32)FRAND_MAX + 1.0f)
#define RAND_FLOAT_RANGE(min,max)       (min + (max-min) * (RAND_FLOAT()))

/*
    - start = starting value
    - end = ending value
    - t = progress from 0.0 to 1.0 (0% to 100%) 
 */
#define LERP_F32(start, end, t)         ((f32)((start) + ((end) - (start)) * (t)))
#define LERP_F64(start, end, t)         ((f64)((start) + ((end) - (start)) * (t)))

#define BILERP(n00, n10, n01, n11, u, v) \
    (LERP_F32(LERP_F32((n00), (n10), (u)), \
              LERP_F32((n01), (n11), (u)), (v)))

#define TRILERP(n000, n100, n010, n110, n001, n101, n011, n111, u, v, w) \
    (LERP_F32(BILERP((n000), (n100), (n010), (n110), (u), (v)), \
              BILERP((n001), (n101), (n011), (n111), (u), (v)), (w)))

/*
    value = LERP(a, b, t)
    t = INVERSE_LERP(a, b, value) 
*/
#define INVERSE_LERP(a, b, value)   (((value) - (a)) / ((b) - (a)))

// Start from a base value and modify it using influences.
#define WEIGHTED2(base, a, wa, b, wb)  ((base) + (a)*(wa) + (b)*(wb))
#define WEIGHTED3(base,a,wa,b,wb,c,wc) ((base)+(a)*(wa)+(b)*(wb)+(c)*(wc))

#define fractionOf(x)               (x - floorf(x))
#define oneMinusFractionOf(x)       (1 - fractionOf(x))

#define ClampTop(A,X)               MIN(A,X)
#define ClampBot(X,B)               MAX(X,B)
#define Clamp(A,X,B)                (((X)<(A))?(A):((X)>(B))?(B):(X))

#define Contains(x,min,max)         (min <= x && x <= max)
#define Inside(x,min,max)           (min <= x && x < max)
#define Surrounds(x,min,max)        (min < x && x < max)

#define Compose64Bit(a,b)           ((((u64)a) << 32) | ((u64)b))
#define Compose32Bit(a,b)           ((((u32)a) << 16) | ((u32)b))
#define AlignPow2(x,b)              (((x) + (b) - 1)&(~((b) - 1)))
#define AlignDownPow2(x,b)          ((x)&(~((b) - 1)))
#define AlignPadPow2(x,b)           ((0-(x)) & ((b) - 1))
#define IsPow2(x)                   ((x)!=0 && ((x)&((x)-1))==0)
#define IsPow2OrZero(x)             ((((x) - 1)&(x)) == 0)

#define ExtractBit(word, idx)       (((word) >> (idx)) & 1)
#define Extract8(word, pos)         (((word) >> ((pos)*8))  & max_u8)
#define Extract16(word, pos)        (((word) >> ((pos)*16)) & max_u16)
#define Extract32(word, pos)        (((word) >> ((pos)*32)) & max_u32)

#define abs_s64(v)                  (u64)llabs(v)

#define sqrt_f32(v)                 sqrtf(v)
#define cbrt_f32(v)                 cbrtf(v)
#define mod_f32(a, b)               fmodf((a), (b))
#define pow_f32(b, e)               powf((b), (e))
#define ceil_f32(v)                 ceilf(v)
#define floor_f32(v)                floorf(v)
#define round_f32(v)                roundf(v)
#define abs_f32(v)                  fabsf(v)
#define radians_from_turns_f32(v)   ((v)*(2*3.1415926535897f))
#define turns_from_radians_f32(v)   ((v)/(2*3.1415926535897f))
#define degrees_from_turns_f32(v)   ((v)*360.f)
#define turns_from_degrees_f32(v)   ((v)/360.f)
#define degrees_from_radians_f32(v) (degrees_from_turns_f32(turns_from_radians_f32(v)))
#define radians_from_degrees_f32(v) (radians_from_turns_f32(turns_from_degrees_f32(v)))
#define sin_f32(v)                  sinf(radians_from_turns_f32(v))
#define cos_f32(v)                  cosf(radians_from_turns_f32(v))
#define tan_f32(v)                  tanf(radians_from_turns_f32(v))

#define sqrt_f64(v)                 sqrt(v)
#define cbrt_f64(v)                 cbrt(v)
#define mod_f64(a, b)               fmod((a), (b))
#define pow_f64(b, e)               pow((b), (e))
#define ceil_f64(v)                 ceil(v)
#define floor_f64(v)                floor(v)
#define round_f64(v)                round(v)
#define abs_f64(v)                  fabs(v)
#define radians_from_turns_f64(v)   ((v)*(2*3.1415926535897))
#define turns_from_radians_f64(v)   ((v)/(2*3.1415926535897))
#define degrees_from_turns_f64(v)   ((v)*360.0)
#define turns_from_degrees_f64(v)   ((v)/360.0)
#define degrees_from_radians_f64(v) (degrees_from_turns_f64(turns_from_radians_f64(v)))
#define radians_from_degrees_f64(v) (radians_from_turns_f64(turns_from_degrees_f64(v)))
#define sin_f64(v)                  sin(radians_from_turns_f64(v))
#define cos_f64(v)                  cos(radians_from_turns_f64(v))
#define tan_f64(v)                  tan(radians_from_turns_f64(v))

typedef uint8_t     u8;
typedef int8_t      i8;

typedef uint16_t    u16;
typedef int16_t     i16;

typedef uint32_t    u32;
typedef int32_t     i32;

typedef uint64_t    u64;
typedef int64_t     i64;

typedef float       f32;
typedef double      f64;

#define fn                  static
#define internal            static
#define local_persist       static
#define global_variable     static

#define c_linkage           extern "c"

#ifdef _WIN32
    typedef HANDLE thread_handle_t;
    typedef DWORD (WINAPI *thread_func_t)(LPVOID);
    typedef LPVOID thread_func_param_t;
    typedef DWORD thread_func_ret_t;
    typedef CONDITION_VARIABLE cond_handle_t;
    typedef CRITICAL_SECTION mutex_handle_t;
    typedef HANDLE pipe_handle;
    typedef HANDLE event_handle;
    typedef volatile LONG atomic_int_t;
#else
    typedef pthread_t thread_handle_t;
    typedef void* (*thread_func_t)(void*);
    typedef void* thread_func_param_t;
    typedef void* thread_func_ret_t;
    typedef pthread_mutex_t mutex_handle_t;
    typedef pthread_cond_t cond_handle_t;
    typedef int pipe_handle;
    typedef struct {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        bool signaled;
    }event_handle;
    typedef atomic_int atomic_int_t;
#endif

static const u32 sign32     = 0x80000000;
static const u32 exponent32 = 0x7F800000;
static const u32 mantissa32 = 0x007FFFFF;

static const f32 big_golden32 = 1.61803398875f;
static const f32 small_golden32 = 0.61803398875f;

static const f32 pi32 = 3.1415926535897f;

static const f64 machine_epsilon64 = 4.94065645841247e-324;

static const u8  max_u8  = 0xff;
static const u16 max_u16 = 0xffff;
static const u32 max_u32 = 0xffffffff;
static const u64 max_u64 = 0xffffffffffffffffull;

static const i8  min_i8  =  (i8)0x80;
static const i16 min_i16 = (i16)0x8000;
static const i32 min_i32 = (i32)0x80000000;
static const i64 min_i64 = (i64)0x8000000000000000ll;

static const i8  max_i8  =  (i8)0x7f;
static const i16 max_i16 = (i16)0x7fff;
static const i32 max_i32 = (i32)0x7fffffff;
static const i64 max_i64 = (i64)0x7fffffffffffffffll;

static const f32 max_f32 = FLT_MAX;
static const f64 max_f64 = DBL_MAX;

static const f32 min_f32 = FLT_MIN;
static const f64 min_f64 = DBL_MIN;

typedef struct vec2_t{
    i32 x,y;
}vec2_t;

typedef struct vec2f_t
{
    f32 x, y;
}vec2f_t;

typedef struct vec3f_t
{
    f32 x, y, z;
}vec3f_t;

typedef struct vec4f_t
{
    f32 x, y, z, w;    
}vec4f_t;

typedef struct mat4x4_t
{
    f32 values[16];
}mat4x4_t;

#define MAT(m,r,c) (m)[(r)*4+(c)]
#define SWAP_ROWS(a, b) {f32 *_tmp = a; (a)=(b); (b)=_tmp; }

/* 
    3D rotations fundamentally have 3 degrees of freedom 
    but can be represented in multiple ways, each with tradeoffs.

    - Rotation matrices (3×3, storing 9 numbers)
        are intuitive and easy to compose but suffer from numerical drift over time, 
        requiring expensive re-orthonormalization to maintain the six constraints
        that keep rows unit-length and perpendicular. 

    - Euler angles (yaw/pitch/roll, 3 numbers)
        are memory-efficient and drift-free since they have no constraints, 
        but suffer from gimbal lock and can't easily compose or interpolate rotations. 

    - Axis-angle representation (unit axis + angle, 4 numbers) 
        elegantly captures Euler's theorem that any rotation is a single spin around some axis 
        and avoids gimbal lock, but still can't easily compose rotations or interpolate smoothly,
        plus has multiple representations for the same rotation.

    This leads us to quaternions (4 numbers with unit constraint), which combine the best properties:
        no gimbal lock, easy composition through multiplication, smooth interpolation via SLERP, 
        and only minimal drift requiring occasional cheap normalization—making them the preferred
        choice for storing and updating orientations in modern game engines and physics simulations.

    There are several notations that we can use to represent quaternions.
    The two most popular notations are :

    - complex number notation 
        w + xi + yj + zk (where i^2 = j^2 = k = -1 and ij = k = -ij with real w, x, y, z)

    - 4D vector notation
        [w, v] (where v = (x, y, z) is called a “vector” and w is called a “scalar”)

    the rotation of a vector v by a unit quaternion q can be represented as
        v´ = q v q-1 (where v = [0, v])
        The result, a rotated vector v´, will always have a 0 scalar value for w
*/
typedef struct
{
	f32 x,y,z,w;
} quat_t;

typedef enum {
    EASE_LINEAR,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_IN_SINE,
    EASE_OUT_SINE,
    EASE_IN_OUT_SINE,
    EASE_OUT_BOUNCE
}easing_type;

typedef struct {
    u64         id;
    f32         start;
    f32         current;
    f32         target;
    f64         duration;
    f64         elapsed;
    easing_type easing;
    bool        done;
}animation_t;

typedef struct {
    animation_t steps[8];  // fixed max chain length
    i32  step_count;
    i32  current_step;
    u64  chain_id;               // unique ID for the chain itself
} anim_chain_t;

#define ANIM_RANK_MAX_MEMBERS 16
#define ANIM_RANK_MAX         32

typedef struct {
    u64 rank_id;
    u64 member_ids[ANIM_RANK_MAX_MEMBERS];
    i32 member_count;
} anim_rank_t;

#define POLY_MORPH_MAX_VERTS 32
typedef struct {
    u64         id_base; 
    i32         count;   
    i32         start_x[POLY_MORPH_MAX_VERTS];
    i32         start_y[POLY_MORPH_MAX_VERTS];
    i32         target_x[POLY_MORPH_MAX_VERTS];
    i32         target_y[POLY_MORPH_MAX_VERTS];
    i32         duration;
    easing_type easing;
} poly_morph_t;

#define ANIMATION_MAX_ITEMS 32

extern animation_t animation_items[ANIMATION_MAX_ITEMS];
extern i32 animation_item_count;

extern anim_chain_t anim_chains[32];
extern i32 anim_chain_count;

anim_rank_t anim_ranks[ANIM_RANK_MAX];
i32         anim_rank_count;

typedef void (*job_func_t)(void* data);

typedef struct 
{
    job_func_t func;
    void* data;
}job_t;

typedef struct 
{
    thread_handle_t* threads;      

    mutex_handle_t queue_mutex;

    cond_handle_t work_available;
    cond_handle_t idle_condition;

    job_t* jobs;                   
    atomic_int_t active_jobs; 

    bool should_terminate;
} thread_pool_t;

typedef struct
{
    #ifdef _WIN32
        DWORD pid;
        HANDLE pipe_fd;
        HANDLE hProcess;
        char cmd[256];
        int active;
    #else
        pid_t pid;
        int pipe_fd;
        char cmd[256];
        int active;
    #endif    
}process_t;

void print_spaces(const char* message, i32 spaces);
void log_color(char *text, char c);
void log_error(i32 error_code, const char* file, i32 line);
void *check_ptr (void *ptr, const char* file, i32 line);
void dump_memory(void *ptr, i32 size);
void membroadcast(void *dst, const void *src, size_t element_size, size_t count);

void fast_srand(u32 seed);
i32 fast_rand(void);
f32 f_randf(u32 index);
i32 f_randi(u32 index);
f32 f_randnf(u32 index);

u32 hash(u32 x);
u32 unhash(u32 x);
u32 djb2_hash(const char *str);
u32 djb2_hash_append(u32 seed, const char *str);
u32 fnv1a_hash(const char *str);
u64 arith_mod(u64 x, u64 y);
f32 d_sqrt(f32 number);
f32 smoothstep(f32 edge0, f32 edge1, f32 x); 
f32 sine_deg(i32 x);
f32 cosine_deg(i32 x);

vec2f_t vec2f(float x, float y);
vec2f_t vec2f_add(vec2f_t a, vec2f_t b);
vec2f_t vec2f_sub(vec2f_t a, vec2f_t b);
vec2f_t vec2f_mul(vec2f_t a, vec2f_t b);
vec2f_t vec2f_div(vec2f_t a, vec2f_t b);

vec3f_t vec3f_add(vec3f_t a, vec3f_t b);
vec3f_t vec3f_sub(vec3f_t a, vec3f_t b);
vec3f_t vec3f_mul(vec3f_t a, vec3f_t b);
vec3f_t vec3f_scale(vec3f_t v, f32 scalar);
f32 vec3f_dot(vec3f_t a, vec3f_t b);
vec3f_t vec3f_cross(vec3f_t a, vec3f_t b);
f32 vec3f_length_sq(vec3f_t v);
f32 vec3f_length(vec3f_t v);
vec3f_t vec3f_unit(vec3f_t v);
vec3f_t vec3f_lerp(vec3f_t a, vec3f_t b, float s);
vec3f_t vec3f_random();
vec3f_t vec3f_random_range(float min, float max);
vec3f_t vec3f_random_direction();
vec3f_t vec3f_random_direction_2d();
bool vec3f_is_near_zero(vec3f_t vec);
vec3f_t vec3f_reflect(vec3f_t v, vec3f_t n);
vec3f_t vec3f_refract(vec3f_t v, vec3f_t n, float e);

mat4x4_t mat4x4_mult(mat4x4_t const *m, mat4x4_t const *n);
mat4x4_t mat4x4_mult_simd(mat4x4_t const *m, mat4x4_t const *n);
mat4x4_t mat4x4_transpose(mat4x4_t const *m);
bool mat4x4_invert(mat4x4_t const *m, mat4x4_t *out);
mat4x4_t mat_perspective(f32 n, f32 f, f32 fovY, f32 aspect_ratio);
mat4x4_t mat_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far);
mat4x4_t mat_identity(void);
mat4x4_t mat_scale(vec3f_t s);
mat4x4_t mat_scale_const_xyz(f32 x, f32 y, f32 z);
mat4x4_t mat_scale_const(f32 s);
mat4x4_t mat_translate(vec3f_t s);
mat4x4_t mat_rotate_xy(f32 angle);
mat4x4_t mat_rotate_yz(f32 angle);
mat4x4_t mat_rotate_zx(f32 angle);
vec3f_t mat_direction_from_angles(f32 yaw, f32 pitch);
mat4x4_t mat_rotation_from_direction(vec3f_t front, vec3f_t world_up);
mat4x4_t mat_rotation_from_angles(f32 yaw, f32 pitch);
mat4x4_t mat_look_at(vec3f_t eye, vec3f_t center, vec3f_t up);

quat_t quat_from_mat4x4(mat4x4_t const *mat);
mat4x4_t quat_to_mat4x4(quat_t const *quat);
quat_t quat_mult(quat_t const *q1, quat_t const *q2);
quat_t quat_mult2(quat_t *quat1, quat_t *quat2);
void quat_normalize(quat_t *quat);
void quat_from_euler(vec3f_t *rot, quat_t *quat);
void quat_from_euler2(vec3f_t *rot, quat_t *quat);
void quat_to_axis_angle(quat_t *quat, quat_t *axisAngle);
void quat_slerp(quat_t *quat1, quat_t *quat2, float slerp, quat_t *result);


f64 apply_easing(f64 t, easing_type easing);
void animation_start(u64 id, f32 start, f32 target, f32 duration, easing_type easing);
void animation_stop(u64 id);
void animation_update(f64 dt);
bool animation_get(u64 id, f32 *current);
void animation_pingpong(u64 id, f32 start, f32 target, f32 duration, easing_type easing);
void animation_chain_update(void);
void animation_rank_start(u64 id, animation_t *members, i32 member_count);
void animation_rank_stop(u64 id);
bool animation_rank_get(u64 id, f32 *out_values, i32 out_count);
void animation_rank_update(void);


void morph_start(poly_morph_t *m,
                 int *src_x, int *src_y, int n,
                 int *dst_x, int *dst_y, int dst_count,
                 f32 duration, easing_type easing);

bool morph_sample(poly_morph_t *m, f32 *out_x, f32 *out_y);

void morph_stop(poly_morph_t *m);

f64 get_time_difference(void *last_time);
void get_time(void *time);

thread_handle_t create_thread(thread_func_t func, thread_func_param_t data);
void join_thread(thread_handle_t thread);
int get_core_count(void);
void mutex_init(mutex_handle_t* mutex);
void mutex_destroy(mutex_handle_t* mutex);
void mutex_lock(mutex_handle_t* mutex);
void mutex_unlock(mutex_handle_t* mutex);
void cond_init(cond_handle_t* cond);
void cond_destroy(cond_handle_t* cond);
void cond_wait(cond_handle_t* cond, mutex_handle_t* mutex);
void cond_signal(cond_handle_t* cond);
void cond_broadcast(cond_handle_t* cond);
void event_create(event_handle *event);
void event_destroy(event_handle *event);
bool event_wait(event_handle *event);
bool event_activate(void *event);
thread_func_ret_t thread_loop(thread_func_param_t param);
thread_pool_t* threadpool_create(void);
void threadpool_destroy(thread_pool_t* pool);
void threadpool_queue_job(thread_pool_t* pool, job_func_t func, void* data);
bool threadpool_busy(thread_pool_t* pool);
void threadpool_wait(thread_pool_t* pool);

int set_non_blocking(pipe_handle fd);
int spawn_process_async(process_t *proc, const char *command);
void check_process_output_async(process_t *proc);
int check_process_status_async(process_t *proc);

const char* get_file_extension(const char *filepath);
unsigned char* read_file(const char* font_path);
void skip_whitespace_and_commas(const char **p);
int parse_float(const char **p, float *out);
int parse_two_floats(const char **p, float *x, float *y);


#define LOG_ERROR(error_code)   log_error(error_code, __FILE__, __LINE__)
#define CHECK_PTR(ptr)          check_ptr(ptr, __FILE__, __LINE__)

#endif /* UTIL_H_ */