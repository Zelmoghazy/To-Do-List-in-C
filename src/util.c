#include "../include/util.h"
#include "GLFW/glfw3.h"

void print_spaces(const char* message, int spaces)
{
    printf("%-*s ",spaces,message);
}

void log_color(char *text, char c)
{
    switch (c)
    {
        case 'r':
        {
            printf("\033[1;31m");
        }
        break;
        case 'y':
        {
            printf("\033[1;33m");
        }
        break;
        case 'b':
        {
            printf("\033[0;34m");
        }
        break;
        case 'p':
        {
            printf("\033[0;35m");
        }
        break;
        case 'g':
        {
            printf("\033[0;32m");
        }
        break;
        default:
        {
            printf("\033[0m");
        }
        break;
    }
    printf("%s\n", text);
    printf("\033[0m");
}

void log_error(i32 error_code, const char* file, i32 line)
{
    if (error_code != 0){
        fprintf(stderr,"Error %d, In file: %s, Line : %d \n", error_code, file, line);
    }
}

void* check_ptr(void *ptr, const char* file, i32 line)
{
    if(ptr == NULL){
        fprintf(stderr, "Error ! Null Pointer Detected, In file: %s, Line : %d \n", file, line);
    }
    return ptr;
}

void dump_memory(void *ptr, i32 size)
{
    int i;
    byte *byte_ptr = (byte *)ptr;
    printf("\n");
    print_spaces("Address",20);
    print_spaces("Bytes",18);
    print_spaces("Characters",10);
    printf("\n");
    print_spaces("--------",8);
    print_spaces("-----------------------------",30);
    print_spaces("----------",10);
    printf("\n");
    while(size > 0)
    {
        printf("%8X ", (u32)byte_ptr);
        for (i = 0; i < 10 && i < size; i++){
            printf("%.2X ", *(byte_ptr + i));
        }
        /* Compensate for size less than 10 bytes */
        for (; i < 10; i++){
            printf("   ");
        }
        printf(" ");
        for (i = 0; i < 10 && i < size; i++)
        {
            byte ch = *(byte_ptr + i);
            if (!isprint(ch))
                ch = '.';
            printf("%c", ch);
        }
        printf("\n");
        byte_ptr += 10;
        size -= 10;
    }
    return;
}

__declspec(thread) u32 g_seed;

void fast_srand(u32 seed) 
{
    g_seed = seed;
}

// Output value in range [0, 32767]
i32 fast_rand(void) 
{
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16)&0x7FFF;
}

u32 hash(u32 x)
{
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

u32 unhash(u32 x)
{
    x = ((x >> 16) ^ x) * 0x119de1f3;
    x = ((x >> 16) ^ x) * 0x119de1f3;
    x = (x >> 16) ^ x;
    return x;
}

u32 djb2_hash(const char *str) 
{
    u64 hash = 5381;
    for (; *str != '\0'; str++) {
        byte c = *str;
        hash = ((hash << 5) + hash) ^ c;
    }
    return hash;
}

u32 fnv1a_hash(const char *str) 
{
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

u64 arith_mod(u64 x, u64 y) 
{
    if (-13 / 5 == -2 &&        // Check division truncates to zero
        (x < 0) != (y < 0) &&   // Check x and y have different signs
        (x % y) != 0)
        return x % y + y;
    else
        return x % y;
}

f32 d_sqrt(f32 number)
{
    i32 i;
    f32 x, y;
    x = number * 0.5;
    y  = number;
    i  = * (i32 * ) &y;
    i  = 0x5f3759df - (i >> 1);
    y  = * ( f32 * ) &i;
    y  = y * (1.5 - (x * y * y));
    y  = y * (1.5 - (x * y * y));
    return number * y;
}

static inline f32 smoothstep(f32 edge0, f32 edge1, f32 x) 
{
    f32 t = Clamp(0.0f, (x - edge0) / (edge1 - edge0), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

vec2f_t vec2f(f32 x, f32 y)
{
    vec2f_t vec2 = {
        .x = x,
        .y = y,
    };
    return  vec2;
}

vec2f_t vec2f_add(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x + b.x;
    vec2.y = a.y + b.y;
    return vec2;
}

vec2f_t vec2f_sub(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x - b.x;
    vec2.y = a.y - b.y;
    return vec2;
}

vec2f_t vec2f_mul(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x * b.x;
    vec2.y = a.y * b.y;
    return vec2;
}

vec2f_t vec2f_div(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x / b.x;
    vec2.y = a.y / b.y;
    return vec2;
}

vec3f_t vec3f_add(vec3f_t a, vec3f_t b) 
{
    vec3f_t result = {a.x + b.x, a.y + b.y, a.z + b.z};
    return result;
}

vec3f_t vec3f_sub(vec3f_t a, vec3f_t b) 
{
    vec3f_t result = {a.x - b.x, a.y - b.y, a.z - b.z};
    return result;
}

vec3f_t vec3f_mul(vec3f_t a, vec3f_t b) 
{
    vec3f_t result = {a.x * b.x, a.y * b.y, a.z * b.z};
    return result;
}

vec3f_t vec3f_scale(vec3f_t v, f32 scalar) 
{
    vec3f_t result = {v.x * scalar, v.y * scalar, v.z * scalar};
    return result;
}

f32 vec3f_dot(vec3f_t a, vec3f_t b) 
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3f_t vec3f_cross(vec3f_t a, vec3f_t b) 
{
    vec3f_t result = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return result;
}

f32 vec3f_length_sq(vec3f_t v) 
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

f32 vec3f_length(vec3f_t v) 
{
    return sqrt_f32((vec3f_length_sq(v)));
}

vec3f_t vec3f_unit(vec3f_t v) 
{
    f32 len = vec3f_length(v);
    
    if (len > 0.0f) {
        f32 inv_len = 1.0f / len;
        return (vec3f_t){ v.x * inv_len, v.y * inv_len, v.z * inv_len };
    }
    return (vec3f_t){ 0.0f, 0.0f, 0.0f }; // Return zero vector if input is zero
}

vec3f_t vec3f_normalize(vec3f_t v) 
{
    f32 len = vec3f_length(v);
    if (len > 0.0f) {
        f32 inv_len = 1.0f / len;
        return vec3f_scale(v, inv_len);
    }
    return v; // return zero vector if length is zero
}

// returns an linearly interpolated vector based on blend factor 0.0 -> 1.0
vec3f_t vec3f_lerp(vec3f_t a, vec3f_t b, f32 blend_factor)
{
    return vec3f_add(vec3f_scale(a,(1.0f-blend_factor)), vec3f_scale(b,blend_factor));
}

vec3f_t vec3f_random()
{
    return (vec3f_t){RAND_FLOAT(),RAND_FLOAT(),RAND_FLOAT()};
}

vec3f_t vec3f_random_range(f32 min, f32 max)
{
    return (vec3f_t){RAND_FLOAT_RANGE(min, max),RAND_FLOAT_RANGE(min, max),RAND_FLOAT_RANGE(min, max)};
}

// direction vector of random points in a unit sphere
vec3f_t vec3f_random_direction()
{
    for(size_t i = 0; i < 100; i++)
    {
        vec3f_t point_in_cube = vec3f_random_range(-1,1);
        f32 lensq = vec3f_length_sq(point_in_cube);
        // check if the point is inside a unit sphere
        if(1e-15 < lensq && lensq <= 1)
        {
            // normalize to unit length
            return vec3f_scale(point_in_cube,1.0f/sqrt_f32(lensq));
        }
    }
    return (vec3f_t){0.0f,0.0f,0.0f};
}

vec3f_t vec3f_random_direction_2d(void)
{
    for(size_t i = 0; i < 100; i++)
    {
        vec3f_t point_in_plane = {RAND_FLOAT_RANGE(-1, 1), RAND_FLOAT_RANGE(-1, 1), 0.0f};
        f32 lensq = vec3f_length_sq(point_in_plane);

        // check if the point is inside a unit sphere
        if(1e-15 < lensq && lensq <= 1)
        {
            return point_in_plane;
        }
    }
    return (vec3f_t){0.0f,0.0f,0.0f};
}

bool vec3f_is_near_zero(vec3f_t vec)
{
    f32 s = 1e-8;
    return (abs_f32(vec.x) < s) && (abs_f32(vec.y) < s) && (abs_f32(vec.z) < s);
}

/*
    n : surface normal
    v = parallel_part + perpendicular_part
    reflected = parallel_part - perpendicular_part
    we basically kept the parallel part and flipped the perpendicular part
    v - 2 * perpendicular part = parallel_part - perpendicular_part
    v - 2(v.n) * n

 */
vec3f_t vec3f_reflect(vec3f_t v, vec3f_t n)
{
    return vec3f_sub(v, vec3f_scale(n, 2.0f*vec3f_dot(v, n)));
}

// Snell's Law
vec3f_t vec3f_refract(vec3f_t v, vec3f_t n, float e)
{
    float cos_theta = fmin(vec3f_dot(vec3f_scale(v, -1.0f), n), 1.0f);              // incident angle
    vec3f_t r_out_perp = vec3f_scale(vec3f_add(v, vec3f_scale(n, cos_theta)), e);
    vec3f_t r_out_parallel = vec3f_scale(n, -1.0f*sqrt_f32(abs_f32(1.0f-vec3f_length_sq(r_out_perp))));
    return vec3f_add(r_out_perp, r_out_parallel);
}

mat4x4_t mat4x4_mult(mat4x4_t const *m, mat4x4_t const *n)
{
    f32 const m00 = m->values[0];
    f32 const m01 = m->values[1];
    f32 const m02 = m->values[2];
    f32 const m03 = m->values[3];
    f32 const m10 = m->values[4];
    f32 const m11 = m->values[5];
    f32 const m12 = m->values[6];
    f32 const m13 = m->values[7];
    f32 const m20 = m->values[8];
    f32 const m21 = m->values[9];
    f32 const m22 = m->values[10];
    f32 const m23 = m->values[11];
    f32 const m30 = m->values[12];
    f32 const m31 = m->values[13];
    f32 const m32 = m->values[14];
    f32 const m33 = m->values[15];

    f32 const n00 = n->values[0];
    f32 const n01 = n->values[1];
    f32 const n02 = n->values[2];
    f32 const n03 = n->values[3];
    f32 const n10 = n->values[4];
    f32 const n11 = n->values[5];
    f32 const n12 = n->values[6];
    f32 const n13 = n->values[7];
    f32 const n20 = n->values[8];
    f32 const n21 = n->values[9];
    f32 const n22 = n->values[10];
    f32 const n23 = n->values[11];
    f32 const n30 = n->values[12];
    f32 const n31 = n->values[13];
    f32 const n32 = n->values[14];
    f32 const n33 = n->values[15];

    mat4x4_t res;

    res.values[0]  = m00*n00+m01*n10+m02*n20+m03*n30;  // res[0][0]
    res.values[1]  = m00*n01+m01*n11+m02*n21+m03*n31;  // res[0][1]
    res.values[2]  = m00*n02+m01*n12+m02*n22+m03*n32;  // res[0][2]
    res.values[3]  = m00*n03+m01*n13+m02*n23+m03*n33;  // res[0][3]
    res.values[4]  = m10*n00+m11*n10+m12*n20+m13*n30;  // res[1][0]
    res.values[5]  = m10*n01+m11*n11+m12*n21+m13*n31;  // res[1][1]
    res.values[6]  = m10*n02+m11*n12+m12*n22+m13*n32;  // res[1][2]
    res.values[7]  = m10*n03+m11*n13+m12*n23+m13*n33;  // res[1][3]
    res.values[8]  = m20*n00+m21*n10+m22*n20+m23*n30;  // res[2][0]
    res.values[9]  = m20*n01+m21*n11+m22*n21+m23*n31;  // res[2][1]
    res.values[10] = m20*n02+m21*n12+m22*n22+m23*n32;  // res[2][2]
    res.values[11] = m20*n03+m21*n13+m22*n23+m23*n33;  // res[2][3]
    res.values[12] = m30*n00+m31*n10+m32*n20+m33*n30;  // res[3][0]
    res.values[13] = m30*n01+m31*n11+m32*n21+m33*n31;  // res[3][1]
    res.values[14] = m30*n02+m31*n12+m32*n22+m33*n32;  // res[3][2]
    res.values[15] = m30*n03+m31*n13+m32*n23+m33*n33;  // res[3][3]
    
    return res;
}

mat4x4_t mat4x4_transpose(mat4x4_t const *m)
{
    f32 const m00 = m->values[0];
    f32 const m01 = m->values[1];
    f32 const m02 = m->values[2];
    f32 const m03 = m->values[3];
    f32 const m10 = m->values[4];
    f32 const m11 = m->values[5];
    f32 const m12 = m->values[6];
    f32 const m13 = m->values[7];
    f32 const m20 = m->values[8];
    f32 const m21 = m->values[9];
    f32 const m22 = m->values[10];
    f32 const m23 = m->values[11];
    f32 const m30 = m->values[12];
    f32 const m31 = m->values[13];
    f32 const m32 = m->values[14];
    f32 const m33 = m->values[15];

    mat4x4_t res;

    res.values[0]  = m00;  // res[0][0]
    res.values[1]  = m10;  // res[0][1]
    res.values[2]  = m20;  // res[0][2]
    res.values[3]  = m30;  // res[0][3]
    
    res.values[4]  = m01;  // res[1][0]
    res.values[5]  = m11;  // res[1][1]
    res.values[6]  = m21;  // res[1][2]
    res.values[7]  = m31;  // res[1][3]

    res.values[8]  = m02;  // res[2][0]
    res.values[9]  = m12;  // res[2][1]
    res.values[10] = m22;  // res[2][2]
    res.values[11] = m32;  // res[2][3]
    
    res.values[12] = m03;  // res[3][0]
    res.values[13] = m13;  // res[3][1]
    res.values[14] = m23;  // res[3][2]
    res.values[15] = m33;  // res[3][3]
    
    return res;
}

mat4x4_t mat4x4_mult_simd(mat4x4_t const *m, mat4x4_t const *n) 
{
    mat4x4_t res;
    
    __m128 n_col0 = _mm_load_ps(&n->values[0]);  // n[0,0] n[1,0] n[2,0] n[3,0]
    __m128 n_col1 = _mm_load_ps(&n->values[4]);  // n[0,1] n[1,1] n[2,1] n[3,1]
    __m128 n_col2 = _mm_load_ps(&n->values[8]);  // n[0,2] n[1,2] n[2,2] n[3,2]
    __m128 n_col3 = _mm_load_ps(&n->values[12]); // n[0,3] n[1,3] n[2,3] n[3,3]
    
    for (int i = 0; i < 4; i++) 
    {
        __m128 m_row = _mm_load_ps(&m->values[i*4]);
        
        __m128 result = _mm_mul_ps(_mm_shuffle_ps(m_row, m_row, 0x00), n_col0);
        result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(m_row, m_row, 0x55), n_col1));
        result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(m_row, m_row, 0xAA), n_col2));
        result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(m_row, m_row, 0xFF), n_col3));
        
        _mm_store_ps(&res.values[i*4], result);
    }
    
    return res;
}

mat4x4_t mat_perspective(f32 n, f32 f, f32 fovY, f32 aspect_ratio)
{
    f32 top   = n * tanf(fovY / 2.f);
    f32 right = top * aspect_ratio;

    return (mat4x4_t) {
        n / right,      0.f,       0.f,                    0.f,
        0.f,            n / top,   0.f,                    0.f,
        0.f,            0.f,       -(f + n) / (f - n),     - 2.f * f * n / (f - n),
        0.f,            0.f,       -1.f,                   0.f,
    };
}

mat4x4_t mat_orthographic(f32 l, f32 r, f32 bottom, f32 t, f32 n, f32 f)
{
    return (mat4x4_t) {
        2.0f / (r - l),             0.0f,                       0.0f,                   -(r + l) / (r - l),            
        0.0f,                       2.0f / (t - bottom),        0.0f,                   -(t + bottom) / (t - bottom),  
        0.0f,                       0.0f,                       -2.0f / (f - n),        -(f + n) / (f - n),            
        0.0f,                       0.0f,                       0.0f,                   1.0f                           
    };
}

mat4x4_t mat_identity(void)
{
    return (mat4x4_t){
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };
}

mat4x4_t mat_scale(vec3f_t s)
{
    return (mat4x4_t) {
        s.x,  0.f,  0.f,  0.f,
        0.f,  s.y,  0.f,  0.f,
        0.f,  0.f,  s.z,  0.f,
        0.f,  0.f,  0.f,  1.f,
    };
}

mat4x4_t mat_scale_const(f32 s)
{
    vec3f_t vec = {
        .x = s,
        .y = s,
        .z = s,
    };
    return mat_scale(vec);
}

mat4x4_t mat_scale_const_xyz(f32 x, f32 y, f32 z)
{
    vec3f_t vec = {
        .x = x,
        .y = y,
        .z = z,
    };
    return mat_scale(vec);
}

mat4x4_t mat_translate(vec3f_t s)
{
    return (mat4x4_t){
        1.f, 0.f, 0.f, s.x,
        0.f, 1.f, 0.f, s.y,
        0.f, 0.f, 1.f, s.z,
        0.f, 0.f, 0.f, 1.f,
    };
}

mat4x4_t mat_rotate_xy(f32 angle)
{
    f32 cos = cosf(angle);
    f32 sin = sinf(angle);

    return (mat4x4_t){
        cos, -sin, 0.f, 0.f,
        sin,  cos, 0.f, 0.f,
        0.f,  0.f, 1.f, 0.f,
        0.f,  0.f, 0.f, 1.f,
    };
}

mat4x4_t mat_rotate_yz(f32 angle)
{
    f32 cos = cosf(angle);
    f32 sin = sinf(angle);

    return (mat4x4_t){
        1.f, 0.f,  0.f, 0.f,
        0.f, cos, -sin, 0.f,
        0.f, sin,  cos, 0.f,
        0.f, 0.f,  0.f, 1.f,
    };
}

mat4x4_t mat_rotate_zx(f32 angle)
{
    f32 cos = cosf(angle);
    f32 sin = sinf(angle);

    return (mat4x4_t){
         cos, 0.f, sin, 0.f,
         0.f, 1.f, 0.f, 0.f,
        -sin, 0.f, cos, 0.f,
         0.f, 0.f, 0.f, 1.f,
    };
}

float trig_values[] = { 
    0.0000f,0.0175f,0.0349f,0.0523f,0.0698f,0.0872f,0.1045f,0.1219f,
    0.1392f,0.1564f,0.1736f,0.1908f,0.2079f,0.2250f,0.2419f,0.2588f,
    0.2756f,0.2924f,0.3090f,0.3256f,0.3420f,0.3584f,0.3746f,0.3907f,
    0.4067f,0.4226f,0.4384f,0.4540f,0.4695f,0.4848f,0.5000f,0.5150f,
    0.5299f,0.5446f,0.5592f,0.5736f,0.5878f,0.6018f,0.6157f,0.6293f,
    0.6428f,0.6561f,0.6691f,0.6820f,0.6947f,0.7071f,0.7071f,0.7193f,
    0.7314f,0.7431f,0.7547f,0.7660f,0.7771f,0.7880f,0.7986f,0.8090f,
    0.8192f,0.8290f,0.8387f,0.8480f,0.8572f,0.8660f,0.8746f,0.8829f,
    0.8910f,0.8988f,0.9063f,0.9135f,0.9205f,0.9272f,0.9336f,0.9397f,
    0.9455f,0.9511f,0.9563f,0.9613f,0.9659f,0.9703f,0.9744f,0.9781f,
    0.9816f,0.9848f,0.9877f,0.9903f,0.9925f,0.9945f,0.9962f,0.9976f,
    0.9986f,0.9994f,0.9998f,1.0000f
};

float sine(int x)
{
    x = x % 360;
    if (x < 0) {
        x += 360;
    }
    if (x == 0){
        return 0;
    }else if (x == 90){
        return 1;
    }else if (x == 180){
        return 0;
    }else if (x == 270){
        return -1;
    }

    if(x > 270){
        return - trig_values[360-x];
    }else if(x>180){
        return - trig_values[x-180];
    }else if(x>90){
        return trig_values[180-x];
    }else{
        return trig_values[x];
    }
}

float cosine(int x){
    return sine(90-x);
}

animation_t animation_items[ANIMATION_MAX_ITEMS];
i32 animation_item_count;

/* https://easings.net/ */
f64 apply_easing(f64 t, easing_type easing) 
{
    switch (easing) 
    {
        case EASE_LINEAR:
            return t;
            
        case EASE_IN_QUAD:
            return t * t;
            
        case EASE_OUT_QUAD:
            return t * (2.0 - t);
            
        case EASE_IN_OUT_QUAD:
            if (t < 0.5) return 2.0 * t * t;
            return -1.0 + (4.0 - 2.0 * t) * t;
            
        case EASE_IN_CUBIC:
            return t * t * t;
            
        case EASE_OUT_CUBIC:
            return (--t) * t * t + 1.0;
            
        case EASE_IN_OUT_CUBIC:
            if (t < 0.5) return 4.0 * t * t * t;
            return (t - 1.0) * (2.0 * t - 2.0) * (2.0 * t - 2.0) + 1.0;
            
        case EASE_IN_SINE:
            return 1.0 - (f64)cosf(t * M_PI_2);
            
        case EASE_OUT_SINE:
            return (f64)sinf(t * M_PI_2);
            
        case EASE_IN_OUT_SINE:
            return -0.5 * ((f64)cosf(M_PI * t) - 1.0);

        case EASE_OUT_BOUNCE:
            const f64 n1 = 7.5625;
            const f64 d1 = 2.75;

            if (t < 1.0 / d1) {
                return n1 * t * t;
            } else if (t < 2.0 / d1) {
                return n1 * (t -= 1.5 / d1) * t + 0.75;
            } else if (t < 2.5 / d1) {
                return n1 * (t -= 2.25 / d1) * t + 0.9375;
            } else {
                return n1 * (t -= 2.625 / d1) * t + 0.984375;
            }
        default:
            return t;
    }
}

void animation_start(u64 id, f32 start, f32 target, f32 duration, easing_type easing) 
{
    for (i32 i = 0; i < animation_item_count; i++) 
    {
        animation_t *it = &animation_items[i];
        if (it->id == id) {
            // it->elapsed = 0;      
            it->start = it->current;
            it->target = target;
            it->elapsed = 0;
            it->duration = duration;
            return;
        }
    }

    // push new item if we have room
    if (animation_item_count < ANIMATION_MAX_ITEMS) 
    {
        animation_items[animation_item_count++] = (animation_t){
            .id = id,
            .start = start,
            .target = target,
            .easing = easing,
            .duration = duration,
            .elapsed = 0,
            .done = false
        };
    }
}

void animation_update(f64 dt) 
{
    for(i32 i = animation_item_count-1; i>=0; i--)
    {
        animation_t *anim = &animation_items[i];
        
        anim->elapsed += dt;
        
        if (anim->elapsed >= anim->duration) 
        {
            anim->done = true;
            // clamp it to target
            anim->current = anim->target;
            // remove it from the list when done
            // just replace it with the last item and decrement the count
            *anim = animation_items[--animation_item_count];
            continue;
        }
        
        // progress in animation
        f64 t = anim->elapsed / anim->duration;
        f64 eased_t = apply_easing(t, anim->easing);
        
        anim->current = LERP_F32(anim->start, anim->target, eased_t);
    }
}

bool animation_get(u64 id, f32 *current)
{
    for (i32 i = 0; i < animation_item_count; i++) 
    {
        animation_t *it = &animation_items[i];

        if (it->id == id) {
            *current = it->current;
            return true;
        }
    }
    return false;
}

void animation_pingpong(u64 id, f32 start, f32 target, f32 duration, easing_type easing)
{
    static bool reached_top = false;
    static bool reached_bottom = false;

    f32 begin = MIN(start, target);
    f32 end = MAX(start, target);

    f32 current = 0;

    animation_get(id, &current);

    if(current >= end-EPSILON_F32 && !reached_top)
    {
        animation_start((u64)&current, end, begin, duration, easing);
        reached_bottom = false;
        reached_top = true;
    }
    else if(current <= begin+EPSILON_F32 && !reached_bottom)
    {
        animation_start((u64)&current ,begin, end, duration, easing);
        reached_top = false;
        reached_bottom = true;
    }
}

f64 get_time_difference(void *last_time) 
{
    f64 dt = 0.0;

#ifdef _WIN32
    LARGE_INTEGER now, f;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&now);
    LARGE_INTEGER *last_time_win = (LARGE_INTEGER *)last_time;
    dt = (f64)(now.QuadPart - last_time_win->QuadPart) / (f64)f.QuadPart;
    *last_time_win = now;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    struct timespec *last_time_posix = (struct timespec *)last_time;
    dt = (now.tv_sec - last_time_posix->tv_sec) +
         (now.tv_nsec - last_time_posix->tv_nsec) / 1e9;
    *last_time_posix = now;
#endif

    return dt;
}

void get_time(void *time)
{
    #ifdef WIN32
        QueryPerformanceCounter((LARGE_INTEGER *)time);
    #else
        struct timespec last_frame_start;
        clock_gettime(CLOCK_MONOTONIC, (struct timespec *)time);
    #endif
}

thread_handle_t create_thread(thread_func_t func, thread_func_param_t data)
{
    #ifdef _WIN32
        return CreateThread(NULL,  // security attribute -no idea- NULL means default 
                             0,    // stack size zero means default size 
                             func, // pointer to the function to be executed by the thread
                             data, // pointer to the params passed to the thread
                              0,   // run immediately
                              NULL // dont return identifier
                            );
    #else
        pthread_t thread;
        pthread_create(&thread, NULL, func, data);
        return thread;
    #endif
}

void join_thread(thread_handle_t thread) 
{
    #ifdef _WIN32
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    #else
        pthread_join(thread, NULL);
    #endif
}

int get_core_count(void) 
{
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
    #else
        return sysconf(_SC_NPROCESSORS_ONLN);
    #endif
}

const char* get_file_extension(const char *filepath)
{
    const char* dot = strrchr(filepath, '.');
    if(!dot){
        return NULL;
    }
    return dot+1;
}
