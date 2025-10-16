#ifndef UTIL_H_
#define UTIL_H_

#ifdef _WIN32
    #include <windows.h>
    #include <winnt.h>
    #include <uxtheme.h>
    #include <dwmapi.h>
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

#define TAB_SIZE                    4

#define DEBUG                       1

#define M_PI                        3.14159265358979323846
#define M_PI_2                      1.57079632679489661923

#define NUM_ELEMS(x) ((sizeof(x))/(sizeof((x)[0])))

#define DEBUG_PRT(fmt, ...)\
    do{\
        if(DEBUG)\
            fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__);\
    }while(0)

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

#define EPSILON_F32  (1e-6f)
#define EPSILON_F64  (1e-12)
#define COMPARE_FLOATS(a,b,epsilon)     (fabs(a - b) <= epsilon * fabs(a))

#define MASK(w,m,f)                     (w ^= (-f ^ w) & m)            // Conditionally set or clear bits  if (f) is true ->  w |= m; else w &= ~m;
#define SIGN(v)                         ((v) > 0) - ((v) < 0)
#define ABS(v)                          ((v < 0) ? -(unsigned)v : v)

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

#define MAX_F32(a, b)  ((COMPARE_FLOATS((a), (b), EPSILON_F32) || (a) > (b)) ? (a) : (b))
#define MIN_F32(a, b)  ((COMPARE_FLOATS((a), (b), EPSILON_F32) || (a) < (b)) ? (a) : (b))

#define MAX_F64(a, b)  ((COMPARE_FLOATS((a), (b), EPSILON_F64) || (a) > (b)) ? (a) : (b))
#define MIN_F64(a, b)  ((COMPARE_FLOATS((a), (b), EPSILON_F64) || (a) < (b)) ? (a) : (b))

#define MAX3_F32(a, b, c)  (MAX_F32(MAX_F32((a), (b)), (c)))
#define MIN3_F32(a, b, c)  (MIN_F32(MIN_F32((a), (b)), (c)))

#define MAX3_F64(a, b, c)  (MAX_F64(MAX_F64((a), (b)), (c)))
#define MIN3_F64(a, b, c)  (MIN_F64(MIN_F64((a), (b)), (c)))

#define CEIL_DIV(x, y)                  (((x) + (y) - 1) / (y))
#define ALIGN_UP(x, y)                  ((((x) + (y) - 1) / (y)) * (y))

#define FPS(n)                          (1000/n)

#define RANGE_CONVERT(value, from_min, from_max, to_min, to_max) \
    (((value) - (from_min)) * ((to_max) - (to_min)) / ((from_max) - (from_min)) + (to_min))

#define NORMALIZE(val, min, max) (((val) - (min)) / ((max) - (min)))

#define WRAP_INDEX(pos, size)   (((pos) + (size)) % (size))
#define INC_WRAP(pos, size)     (((pos) + (1)) % (size))

#define FRAND_MAX                 32767  

#define RAND_FLOAT()                (f32)fast_rand() / ((f32)FRAND_MAX + 1.0f)
#define RAND_FLOAT_RANGE(min,max)   (min + (max-min) * (RAND_FLOAT()))

/*
    - start = starting value
    - end = ending value
    - t = progress from 0.0 to 1.0 (0% to 100%) 
 */
#define LERP_F32(start, end, t)     ((f32)((start) + ((end) - (start)) * (t)))
#define LERP_F64(start, end, t)     ((f64)((start) + ((end) - (start)) * (t)))

#define ClampTop(A,X)               MIN(A,X)
#define ClampBot(X,B)               MAX(X,B)
#define Clamp(A,X,B)                (((X)<(A))?(A):((X)>(B))?(B):(X))

#define Contains(x,min,max)         (min <= x && x <= max)
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

#ifdef _WIN32
    typedef HANDLE thread_handle_t;
    typedef DWORD (WINAPI *thread_func_t)(LPVOID);
    typedef LPVOID thread_func_param_t;
    typedef DWORD WINAPI thread_func_ret_t;
#else
    #include <pthread.h>
    typedef pthread_t thread_handle_t;
    typedef void* (*thread_func_t)(void*);
    typedef void* thread_func_param_t;
    typedef void* thread_func_ret_t;
#endif

global_variable u32 sign32     = 0x80000000;
global_variable u32 exponent32 = 0x7F800000;
global_variable u32 mantissa32 = 0x007FFFFF;

global_variable f32 big_golden32 = 1.61803398875f;
global_variable f32 small_golden32 = 0.61803398875f;

global_variable f32  max_f32 = FLT_MAX;
global_variable f64  max_f64 = DBL_MAX;

global_variable f32  min_f32 = FLT_MIN;
global_variable f64  min_f64 = DBL_MIN;

global_variable f32 pi32 = 3.1415926535897f;

global_variable f64 machine_epsilon64 = 4.94065645841247e-324;

global_variable u64 max_u64 = 0xffffffffffffffffull;
global_variable u32 max_u32 = 0xffffffff;
global_variable u16 max_u16 = 0xffff;
global_variable u8  max_u8  = 0xff;

global_variable i64 max_i64 = (i64)0x7fffffffffffffffll;
global_variable i32 max_i32 = (i32)0x7fffffff;
global_variable i16 max_i16 = (i16)0x7fff;
global_variable i8  max_i8  =  (i8)0x7f;

global_variable i64 min_i64 = (i64)0x8000000000000000ll;
global_variable i32 min_i32 = (i32)0x80000000;
global_variable i16 min_i16 = (i16)0x8000;
global_variable i8  min_i8  =  (i8)0x80;

typedef struct color4_t
{
    u8 r;
    u8 g;
    u8 b;
    u8 a;
}color4_t;

typedef struct vec2_t{
    u32 x;
    u32 y;
}vec2_t;

typedef struct vec2f_t{
    f32 x;
    f32 y;
}vec2f_t;

typedef struct vec3f_t
{
    f32 x;
    f32 y;
    f32 z;
}vec3f_t;

typedef struct vec4f_t
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;    
}vec4f_t;

typedef struct mat4x4_t
{
    f32 values[16];
}mat4x4_t;

typedef struct rect_t
{
    i32 x;
    i32 y;
    i32 w;
    i32 h;
}rect_t;

void  log_error(int error_code, const char* file, int line);
void *check_ptr (void *ptr, const char* file, int line);
void swap(int* a, int* b);
void fast_srand(int seed);
int fast_rand(void);

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

f64 get_time_difference(void *last_time);
void get_time(void *time);

thread_handle_t create_thread(thread_func_t func, thread_func_param_t data);
void join_thread(thread_handle_t thread);
int get_core_count(void);

#define LOG_ERROR(error_code)   log_error(error_code, __FILE__, __LINE__)
#define CHECK_PTR(ptr)          check_ptr(ptr, __FILE__, __LINE__)

#endif /* UTIL_H_ */