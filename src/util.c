#include "../include/util.h"
#include "../external/include/stb_ds.h"

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

static inline f32 smoothstep(f32 edge0, f32 edge1, f32 x) 
{
    f32 t = Clamp(0.0f, (x - edge0) / (edge1 - edge0), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/*
    TODO: Check if its really faster
 */
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

static f32 trig_values[] = 
{ 
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

f32 sine_deg(i32 x)
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

f32 cosine_deg(i32 x)
{
    return sine_deg(90-x);
}

/* -------------------- Math stuff -------------------- */

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

bool mat4x4_invert(mat4x4_t const *m, mat4x4_t *out)
{
    f32 wtmp[4][8];
    f32 m0, m1, m2, m3, s;
    f32 *r0, *r1, *r2, *r3;
    
    r0 = wtmp[0], r1 = wtmp[1], r2 = wtmp[2], r3 = wtmp[3];
    
    // Initialize augmented matrix [M | I]
    r0[0] = MAT(m->values,0,0), r0[1] = MAT(m->values,0,1), 
    r0[2] = MAT(m->values,0,2), r0[3] = MAT(m->values,0,3),
    r0[4] = 1.0f, r0[5] = r0[6] = r0[7] = 0.0f;
    
    r1[0] = MAT(m->values,1,0), r1[1] = MAT(m->values,1,1),
    r1[2] = MAT(m->values,1,2), r1[3] = MAT(m->values,1,3),
    r1[5] = 1.0f, r1[4] = r1[6] = r1[7] = 0.0f;
    
    r2[0] = MAT(m->values,2,0), r2[1] = MAT(m->values,2,1),
    r2[2] = MAT(m->values,2,2), r2[3] = MAT(m->values,2,3),
    r2[6] = 1.0f, r2[4] = r2[5] = r2[7] = 0.0f;
    
    r3[0] = MAT(m->values,3,0), r3[1] = MAT(m->values,3,1),
    r3[2] = MAT(m->values,3,2), r3[3] = MAT(m->values,3,3),
    r3[7] = 1.0f, r3[4] = r3[5] = r3[6] = 0.0f;
    
    /* choose pivot - or die */
    if (fabsf(r3[0])>fabsf(r2[0])) SWAP_ROWS(r3, r2);
    if (fabsf(r2[0])>fabsf(r1[0])) SWAP_ROWS(r2, r1);
    if (fabsf(r1[0])>fabsf(r0[0])) SWAP_ROWS(r1, r0);
    if (0.0f == r0[0]) return false;
    
    /* eliminate first variable */
    m1 = r1[0]/r0[0]; m2 = r2[0]/r0[0]; m3 = r3[0]/r0[0];
    s = r0[1]; r1[1] -= m1*s; r2[1] -= m2*s; r3[1] -= m3*s;
    s = r0[2]; r1[2] -= m1*s; r2[2] -= m2*s; r3[2] -= m3*s;
    s = r0[3]; r1[3] -= m1*s; r2[3] -= m2*s; r3[3] -= m3*s;
    s = r0[4];
    if (s != 0.0f) { r1[4] -= m1*s; r2[4] -= m2*s; r3[4] -= m3*s; }
    s = r0[5];
    if (s != 0.0f) { r1[5] -= m1*s; r2[5] -= m2*s; r3[5] -= m3*s; }
    s = r0[6];
    if (s != 0.0f) { r1[6] -= m1*s; r2[6] -= m2*s; r3[6] -= m3*s; }
    s = r0[7];
    if (s != 0.0f) { r1[7] -= m1*s; r2[7] -= m2*s; r3[7] -= m3*s; }
    
    /* choose pivot - or die */
    if (fabsf(r3[1])>fabsf(r2[1])) SWAP_ROWS(r3, r2);
    if (fabsf(r2[1])>fabsf(r1[1])) SWAP_ROWS(r2, r1);
    if (0.0f == r1[1]) return false;
    
    /* eliminate second variable */
    m2 = r2[1]/r1[1]; m3 = r3[1]/r1[1];
    r2[2] -= m2*r1[2]; r3[2] -= m3*r1[2];
    r2[3] -= m2*r1[3]; r3[3] -= m3*r1[3];
    s = r1[4];
    if (0.0f != s) { r2[4] -= m2*s; r3[4] -= m3*s; }
    s = r1[5];
    if (0.0f != s) { r2[5] -= m2*s; r3[5] -= m3*s; }
    s = r1[6];
    if (0.0f != s) { r2[6] -= m2*s; r3[6] -= m3*s; }
    s = r1[7];
    if (0.0f != s) { r2[7] -= m2*s; r3[7] -= m3*s; }
    
    /* choose pivot - or die */
    if (fabsf(r3[2])>fabsf(r2[2])) SWAP_ROWS(r3, r2);
    if (0.0f == r2[2]) return false;
    
    /* eliminate third variable */
    m3 = r3[2]/r2[2];
    r3[3] -= m3*r2[3], r3[4] -= m3*r2[4],
    r3[5] -= m3*r2[5], r3[6] -= m3*r2[6], r3[7] -= m3*r2[7];
    
    /* last check */
    if (0.0f == r3[3]) return false;
    
    s = 1.0f/r3[3]; /* now back substitute row 3 */
    r3[4] *= s; r3[5] *= s; r3[6] *= s; r3[7] *= s;
    
    m2 = r2[3]; /* now back substitute row 2 */
    s = 1.0f/r2[2];
    r2[4] = s*(r2[4] - r3[4]*m2), r2[5] = s*(r2[5] - r3[5]*m2),
    r2[6] = s*(r2[6] - r3[6]*m2), r2[7] = s*(r2[7] - r3[7]*m2);
    m1 = r1[3];
    r1[4] -= r3[4]*m1, r1[5] -= r3[5]*m1,
    r1[6] -= r3[6]*m1, r1[7] -= r3[7]*m1;
    m0 = r0[3];
    r0[4] -= r3[4]*m0, r0[5] -= r3[5]*m0,
    r0[6] -= r3[6]*m0, r0[7] -= r3[7]*m0;
    
    m1 = r1[2]; /* now back substitute row 1 */
    s = 1.0f/r1[1];
    r1[4] = s*(r1[4] - r2[4]*m1), r1[5] = s*(r1[5] - r2[5]*m1),
    r1[6] = s*(r1[6] - r2[6]*m1), r1[7] = s*(r1[7] - r2[7]*m1);
    m0 = r0[2];
    r0[4] -= r2[4]*m0, r0[5] -= r2[5]*m0,
    r0[6] -= r2[6]*m0, r0[7] -= r2[7]*m0;
    
    m0 = r0[1]; /* now back substitute row 0 */
    s = 1.0f/r0[0];
    r0[4] = s*(r0[4] - r1[4]*m0), r0[5] = s*(r0[5] - r1[5]*m0),
    r0[6] = s*(r0[6] - r1[6]*m0), r0[7] = s*(r0[7] - r1[7]*m0);
    
    MAT(out->values,0,0) = r0[4]; MAT(out->values,0,1) = r0[5],
    MAT(out->values,0,2) = r0[6]; MAT(out->values,0,3) = r0[7],
    MAT(out->values,1,0) = r1[4]; MAT(out->values,1,1) = r1[5],
    MAT(out->values,1,2) = r1[6]; MAT(out->values,1,3) = r1[7],
    MAT(out->values,2,0) = r2[4]; MAT(out->values,2,1) = r2[5],
    MAT(out->values,2,2) = r2[6]; MAT(out->values,2,3) = r2[7],
    MAT(out->values,3,0) = r3[4]; MAT(out->values,3,1) = r3[5],
    MAT(out->values,3,2) = r3[6]; MAT(out->values,3,3) = r3[7];
    
    return true;
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

/*
    [cos  -sin  0]
    [sin   cos  0]
    [0     0    1]
 */
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

/*
    [1.f, 0.f,  0.f]
    [0.f, cos, -sin]
    [0.f, sin,  cos]
 */
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

/*
    [cos,  0.f, sin]
    [0.f,  1.f, 0.f]
    [-sin, 0.f, cos]
 */
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

vec3f_t mat_direction_from_angles(f32 yaw, f32 pitch)
{
    vec3f_t direction;
    direction.x = cosf(yaw) * cosf(pitch);
    direction.y = sinf(pitch);
    direction.z = sinf(yaw) * cosf(pitch);
    
    return vec3f_normalize(direction);
}

mat4x4_t mat_rotation_from_direction(vec3f_t front, vec3f_t world_up)
{
    vec3f_t right = vec3f_normalize(vec3f_cross(front, world_up));
    vec3f_t up = vec3f_cross(right, front);
    
    return (mat4x4_t){
        right.x,  right.y,  right.z,  0.f,
        up.x,     up.y,     up.z,     0.f,
        -front.x, -front.y, -front.z, 0.f,
        0.f,      0.f,      0.f,      1.f
    };
}

mat4x4_t mat_rotation_from_angles(f32 yaw, f32 pitch)
{
    vec3f_t front = mat_direction_from_angles(yaw, pitch);
    vec3f_t world_up = {0.f, 1.f, 0.f};
    return mat_rotation_from_direction(front, world_up);
}

mat4x4_t mat_look_at(vec3f_t eye, vec3f_t center, vec3f_t up)
{
    vec3f_t f = vec3f_normalize(vec3f_sub(center, eye));
    vec3f_t r = vec3f_normalize(vec3f_cross(f, up));
    vec3f_t u = vec3f_cross(r, f);
    
    return (mat4x4_t){
        r.x,  r.y,  r.z,  -vec3f_dot(r, eye),
        u.x,  u.y,  u.z,  -vec3f_dot(u, eye),
       -f.x, -f.y, -f.z,   vec3f_dot(f, eye),
        0.f,  0.f,  0.f,   1.f,
    };
}

// Convert rotation matrix to quaternion
quat_t quat_from_mat4x4(mat4x4_t const *mat)
{
    quat_t quat;
    f32 tr, s, q[4];
    i32 i, j, k;
    i32 nxt[3] = {1, 2, 0};
    
    // Calculate trace
    tr = MAT(mat->values, 0, 0) + MAT(mat->values, 1, 1) + MAT(mat->values, 2, 2);
    
    // Check the diagonal
    if (tr > 0.0f) 
    {
        s = sqrtf(tr + 1.0f);
        quat.w = s * 0.5f;
        s = 0.5f / s;
        
        quat.x = (MAT(mat->values, 1, 2) - MAT(mat->values, 2, 1)) * s;
        quat.y = (MAT(mat->values, 2, 0) - MAT(mat->values, 0, 2)) * s;
        quat.z = (MAT(mat->values, 0, 1) - MAT(mat->values, 1, 0)) * s;
    }
    else 
    {
        // Diagonal is negative - find largest diagonal element
        i = 0;
        if (MAT(mat->values, 1, 1) > MAT(mat->values, 0, 0)) i = 1;
        if (MAT(mat->values, 2, 2) > MAT(mat->values, i, i)) i = 2;
        
        j = nxt[i];
        k = nxt[j];
        
        s = sqrtf((MAT(mat->values, i, i) - (MAT(mat->values, j, j) + MAT(mat->values, k, k))) + 1.0f);
        
        q[i] = s * 0.5f;
        
        if (s != 0.0f) s = 0.5f / s;
        
        q[3] = (MAT(mat->values, j, k) - MAT(mat->values, k, j)) * s;
        q[j] = (MAT(mat->values, i, j) + MAT(mat->values, j, i)) * s;
        q[k] = (MAT(mat->values, i, k) + MAT(mat->values, k, i)) * s;
        
        quat.x = q[0];
        quat.y = q[1];
        quat.z = q[2];
        quat.w = q[3];
    }
    
    return quat;
}

// Convert quaternion to rotation matrix
mat4x4_t quat_to_mat4x4(quat_t const *quat)
{
    mat4x4_t m;
    
    // Calculate coefficients
    f32 x2 = quat->x + quat->x;
    f32 y2 = quat->y + quat->y;
    f32 z2 = quat->z + quat->z;
    
    f32 xx = quat->x * x2;
    f32 xy = quat->x * y2;
    f32 xz = quat->x * z2;
    f32 yy = quat->y * y2;
    f32 yz = quat->y * z2;
    f32 zz = quat->z * z2;
    f32 wx = quat->w * x2;
    f32 wy = quat->w * y2;
    f32 wz = quat->w * z2;
    
    MAT(m.values, 0, 0) = 1.0f - (yy + zz);
    MAT(m.values, 0, 1) = xy - wz;
    MAT(m.values, 0, 2) = xz + wy;
    MAT(m.values, 0, 3) = 0.0f;
    
    MAT(m.values, 1, 0) = xy + wz;
    MAT(m.values, 1, 1) = 1.0f - (xx + zz);
    MAT(m.values, 1, 2) = yz - wx;
    MAT(m.values, 1, 3) = 0.0f;
    
    MAT(m.values, 2, 0) = xz - wy;
    MAT(m.values, 2, 1) = yz + wx;
    MAT(m.values, 2, 2) = 1.0f - (xx + yy);
    MAT(m.values, 2, 3) = 0.0f;
    
    MAT(m.values, 3, 0) = 0.0f;
    MAT(m.values, 3, 1) = 0.0f;
    MAT(m.values, 3, 2) = 0.0f;
    MAT(m.values, 3, 3) = 1.0f;
    
    return m;
}

/* 
    Computes the product of two quaternions
    - Quaternion multiplication is not commutative
 */
 quat_t quat_mult(quat_t const *q1, quat_t const *q2)
{
    vec3f_t v1 = {q1->x, q1->y, q1->z};
    vec3f_t v2 = {q2->x, q2->y, q2->z};
    
    vec3f_t t1 = vec3f_scale(v1, q2->w);
    vec3f_t t2 = vec3f_scale(v2, q1->w);
    vec3f_t t3 = vec3f_cross(v1, v2);
    
    vec3f_t result_vec = vec3f_add(vec3f_add(t1, t2), t3);
    
    return (quat_t){
        .x = result_vec.x,
        .y = result_vec.y,
        .z = result_vec.z,
        .w = q1->w * q2->w - vec3f_dot(v1, v2)
    };
}

/* 
    Purpose: Computes the product of two quaternions
*/
quat_t quat_mult2(quat_t *quat1, quat_t *quat2)
{
    quat_t tmp;
    tmp.x =		quat2->w * quat1->x + quat2->x * quat1->w +
				quat2->y * quat1->z - quat2->z * quat1->y;
    tmp.y  =	quat2->w * quat1->y + quat2->y * quat1->w +
				quat2->z * quat1->x - quat2->x * quat1->z;
    tmp.z  =	quat2->w * quat1->z + quat2->z * quat1->w +
				quat2->x * quat1->y - quat2->y * quat1->x;
    tmp.w  =	quat2->w * quat1->w - quat2->x * quat1->x -
				quat2->y * quat1->y - quat2->z * quat1->z;

    return tmp;
}

/* 
    Purpose: Normalize a Quaternion
    Quaternions must follow the rules of x^2 + y^2 + z^2 + w^2 = 1
    				This function insures this 
*/
void quat_normalize(quat_t *quat)
{
	float magnitude;

	magnitude = (quat->x * quat->x) + 
				(quat->y * quat->y) + 
				(quat->z * quat->z) + 
				(quat->w * quat->w);

	quat->x = quat->x / magnitude;
	quat->y = quat->y / magnitude;
	quat->z = quat->z / magnitude;
	quat->w = quat->w / magnitude;
}

/*
    q = q_yaw * q_pitch * q_roll where:
        qroll = [cos (ψ/2), (sin(ψ/2), 0, 0)]
        qpitch = [cos (θ/2), (0, sin(θ/2), 0)]
        qyaw = [cos(φ /2), (0, 0, sin(φ /2)]

        Purpose:    Convert a set of Euler angles to a Quaternion
        Arguments:	A rotation set of 3 angles, a quaternion to set
        Discussion: As the order of rotations is important I am
        				using the Quantum Mechanics convention of (X,Y,Z)
        				a Yaw-Pitch-Roll (Y,X,Z) system would have to be
        				adjusted.  It is more efficient this way though.
    
        Euler (x = ψ, y = θ, z = φ)
        Qx = [(sin(ψ/2),0,0), cos(ψ/2)]
        Qy = [(0,sin(θ/2),0), cos(θ/2)]
        Qz = [(0,0, sin(φ/2)), cos(φ/2)]
*/
void quat_from_euler(vec3f_t *rot, quat_t *quat)
{
	float rx,ry,rz,tx,ty,tz,cx,cy,cz,sx,sy,sz,cc,cs,sc,ss;

	// FIRST STEP, CONVERT ANGLES TO RADIANS
	rx =  (rot->x * (float)M_PI) / (360 / 2);
	ry =  (rot->y * (float)M_PI) / (360 / 2);
	rz =  (rot->z * (float)M_PI) / (360 / 2);

	// GET THE HALF ANGLES
	tx = rx * (float)0.5;
	ty = ry * (float)0.5;
	tz = rz * (float)0.5;
	cx = (float)cos(tx);
	cy = (float)cos(ty);
	cz = (float)cos(tz);
	sx = (float)sin(tx);
	sy = (float)sin(ty);
	sz = (float)sin(tz);

	cc = cx * cz;
	cs = cx * sz;
	sc = sx * cz;
	ss = sx * sz;

	quat->x = (cy * sc) - (sy * cs);
	quat->y = (cy * ss) + (sy * cc);
	quat->z = (cy * cs) - (sy * sc);
	quat->w = (cy * cc) + (sy * ss);

	// PROBABLY NOT NECESSARY IN MOST CASES
	quat_normalize(quat);
}

/* 
    Purpose:    Convert a set of Euler angles to a Quaternion
    Discussion: This is a second variation.  It creates a
			Series of quaternions and multiplies them together
			It would be easier to extend this for other rotation orders 
*/

void quat_from_euler2(vec3f_t *rot, quat_t *quat)
{
	float rx,ry,rz,ti,tj,tk;
	quat_t qx,qy,qz,qf;

	// FIRST STEP, CONVERT ANGLES TO RADIANS
	rx =  (rot->x * (float)M_PI) / (360 / 2);
	ry =  (rot->y * (float)M_PI) / (360 / 2);
	rz =  (rot->z * (float)M_PI) / (360 / 2);
	// GET THE HALF ANGLES
	ti = rx * (float)0.5;
	tj = ry * (float)0.5;
	tk = rz * (float)0.5;

	qx.x = (float)sin(ti); qx.y = 0.0; qx.z = 0.0; qx.w = (float)cos(ti);
	qy.x = 0.0; qy.y = (float)sin(tj); qy.z = 0.0; qy.w = (float)cos(tj);
	qz.x = 0.0; qz.y = 0.0; qz.z = (float)sin(tk); qz.w = (float)cos(tk);

	qf = quat_mult(&qx,&qy);
	qf = quat_mult(&qf,&qz);

// ANOTHER TEST VARIATION
//	MultQuaternions2(&qx,&qy,&qf);
//	MultQuaternions2(&qf,&qz,&qf);

	// INSURE THE QUATERNION IS NORMALIZED
	// PROBABLY NOT NECESSARY IN MOST CASES
	quat_normalize(&qf);

	quat->x = qf.x;
	quat->y = qf.y;
	quat->z = qf.z;
	quat->w = qf.w;

}

/* 
    Purpose: Convert a Quaternion to Axis Angle representation
    q = cos(φ/2) + x sin(φ/2)i + y sin(φ/2)j + z sin(φ/2)k
    φ = acos(Qw)*2
    x = Qx/sin(φ/2)
    y = Qy/sin(φ/2)
    z = Qz/sin(φ/2)
 */
void quat_to_axis_angle(quat_t *quat, quat_t *axisAngle)
{
	float scale,tw;

	tw = (float)acos(quat->w) * 2;
	scale = (float)sin(tw / 2.0);

	axisAngle->x = quat->x / scale;
	axisAngle->y = quat->y / scale;
	axisAngle->z = quat->z / scale;

	axisAngle->w = (tw * (360 / 2)) / (float)M_PI;
}

#define DELTA	0.0001		// DIFFERENCE AT WHICH TO LERP INSTEAD OF SLERP

/*
    Purpose:		Spherical Linear Interpolation Between two Quaternions

    SLERP(p,q,t) = {psin((1-t)θ) + qsin(tθ)} / {sin(θ)}
    where pq = cos(θ) and parameter t goes from 0 to 1
*/
void quat_slerp(quat_t *quat1, quat_t *quat2, float slerp, quat_t *result)
{
	double omega,cosom,sinom,scale0,scale1;
	// USE THE DOT PRODUCT TO GET THE COSINE OF THE ANGLE BETWEEN THE
	// QUATERNIONS
	cosom = quat1->x * quat2->x + 
			quat1->y * quat2->y + 
			quat1->z * quat2->z + 
			quat1->w * quat2->w; 

	// CHECK A COUPLE OF SPECIAL CASES. 
	// MAKE SURE THE TWO QUATERNIONS ARE NOT EXACTLY OPPOSITE? (WITHIN A LITTLE SLOP)
	if ((1.0 + cosom) > DELTA)
	{
		// ARE THEY MORE THAN A LITTLE BIT DIFFERENT? AVOID A DIVIDED BY ZERO AND LERP IF NOT
		if ((1.0 - cosom) > DELTA) {
			// YES, DO A SLERP
			omega = acos(cosom);
			sinom = sin(omega);
			scale0 = sin((1.0 - slerp) * omega) / sinom;
			scale1 = sin(slerp * omega) / sinom;
		} else {
			// NOT A VERY BIG DIFFERENCE, DO A LERP
			scale0 = 1.0 - slerp;
			scale1 = slerp;
		}
		result->x = scale0 * quat1->x + scale1 * quat2->x;
		result->y = scale0 * quat1->y + scale1 * quat2->y;
		result->z = scale0 * quat1->z + scale1 * quat2->z;
		result->w = scale0 * quat1->w + scale1 * quat2->w;
	} else {
		// THE QUATERNIONS ARE NEARLY OPPOSITE SO TO AVOID A DIVIDED BY ZERO ERROR
		// CALCULATE A PERPENDICULAR QUATERNION AND SLERP THAT DIRECTION
		result->x = -quat2->y;
		result->y = quat2->x;
		result->z = -quat2->w;
		result->w = quat2->z;
		scale0 = sin((1.0 - slerp) * (float)M_PI_2);
		scale1 = sin(slerp * (float)M_PI_2);
		result->x = scale0 * quat1->x + scale1 * result->x;
		result->y = scale0 * quat1->y + scale1 * result->y;
		result->z = scale0 * quat1->z + scale1 * result->z;
		result->w = scale0 * quat1->w + scale1 * result->w;
	}

}

/* -------------------- Animation stuff -------------------- */

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

/* -------------------- Timing stuff -------------------- */

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

const char* get_file_extension(const char *filepath)
{
    const char* dot = strrchr(filepath, '.');
    if(!dot){
        return NULL;
    }
    return dot+1;
}

unsigned long get_page_size(void) 
{
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwPageSize;
    #else
        return sysconf(_SC_PAGESIZE);
    #endif
}

unsigned long get_allocation_granularity(void) 
{
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwAllocationGranularity;
    #else
        return sysconf(_SC_PAGESIZE);
    #endif
}

#ifdef _WIN32
static PSYSTEM_LOGICAL_PROCESSOR_INFORMATION get_processor_info_buffer(DWORD* pCount) 
{
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = NULL;
    DWORD dwSize = 0;
    
    GetLogicalProcessorInformation(pBuffer, &dwSize);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return NULL;
    }
    
    pBuffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(dwSize);
    if (!pBuffer) {
        return NULL;
    }
    
    if (!GetLogicalProcessorInformation(pBuffer, &dwSize)) {
        free(pBuffer);
        return NULL;
    }
    
    if (pCount) {
        *pCount = dwSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    }
    
    return pBuffer;
}

int get_physical_core_count(void) 
{
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = get_processor_info_buffer(NULL);
    if (!pBuffer) return 0;
    
    int coreCount = 0;
    DWORD byteOffset = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = pBuffer;
    
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= 
           sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) * 64) { // reasonable limit
        if (ptr->Relationship == RelationProcessorCore) {
            coreCount++;
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    
    free(pBuffer);
    return coreCount;
}

int get_logical_processor_count(void) 
{
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = get_processor_info_buffer(NULL);
    if (!pBuffer) return 0;
    
    int logicalCount = 0;
    DWORD byteOffset = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = pBuffer;
    
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= 
           sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) * 64) {
        if (ptr->Relationship == RelationProcessorCore) {
            // Count bits in ProcessorMask to get number of logical processors for this core
            ULONG_PTR mask = ptr->ProcessorMask;
            while (mask) {
                logicalCount += (mask & 1);
                mask >>= 1;
            }
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    
    free(pBuffer);
    return logicalCount;
}

BOOL has_hyperthreading(void) 
{
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = get_processor_info_buffer(NULL);
    if (!pBuffer) return FALSE;
    
    BOOL hasHT = FALSE;
    DWORD byteOffset = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = pBuffer;
    
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= 
           sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) * 64) {
        if (ptr->Relationship == RelationProcessorCore) {
            // If Flags == 1, it's hyper-threaded
            if (ptr->ProcessorCore.Flags == 1) {
                hasHT = TRUE;
                break;
            }
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    
    free(pBuffer);
    return hasHT;
}


int get_numa_node_count(void) 
{
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = get_processor_info_buffer(NULL);
    if (!pBuffer) return 0;
    
    int numaCount = 0;
    DWORD byteOffset = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = pBuffer;
    
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= 
           sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) * 64) {
        if (ptr->Relationship == RelationNumaNode) {
            numaCount++;
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    
    free(pBuffer);
    return numaCount;
}

void get_cache_info(int cacheLevel, int* lineSize, DWORD64* size) 
{
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = get_processor_info_buffer(NULL);
    if (!pBuffer) {
        if (lineSize) *lineSize = 0;
        if (size) *size = 0;
        return;
    }
    
    DWORD byteOffset = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = pBuffer;
    
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= 
           sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) * 64) {
        if (ptr->Relationship == RelationCache) {
            if (ptr->Cache.Level == cacheLevel) {
                if (lineSize) *lineSize = ptr->Cache.LineSize;
                if (size) *size = ptr->Cache.Size;
                break;
            }
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    
    free(pBuffer);
}

int get_socket_count(void) 
{
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = get_processor_info_buffer(NULL);
    if (!pBuffer) return 0;
    
    int socketCount = 0;
    DWORD byteOffset = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = pBuffer;
    
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= 
           sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) * 64) {
        if (ptr->Relationship == RelationProcessorPackage) {
            socketCount++;
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    
    free(pBuffer);
    return socketCount;
}

void print_detailed_processor_info(void) 
{
    DWORD count = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = get_processor_info_buffer(&count);
    if (!pBuffer) {
        printf("Cannot get processor information\n");
        return;
    }
    
    int physicalCores = 0;
    int logicalProcessors = 0;
    
    for (DWORD i = 0; i < count; i++) {
        switch (pBuffer[i].Relationship) {
            case RelationProcessorCore:
                physicalCores++;
                printf("Core %d: ", physicalCores);
                if (pBuffer[i].ProcessorCore.Flags == 1) {
                    printf("Hyper-Threaded ");
                }
                // Count logical processors in this core
                int logicalInCore = 0;
                ULONG_PTR mask = pBuffer[i].ProcessorMask;
                while (mask) {
                    logicalInCore += (mask & 1);
                    mask >>= 1;
                }
                printf("(%d logical processors)\n", logicalInCore);
                logicalProcessors += logicalInCore;
                break;
                
            case RelationNumaNode:
                printf("NUMA Node %d\n", pBuffer[i].NumaNode.NodeNumber);
                break;
                
            case RelationCache:
                printf("Cache: L%d, Size: %llu KB, Line: %d bytes\n",
                       pBuffer[i].Cache.Level,
                       pBuffer[i].Cache.Size / 1024,
                       pBuffer[i].Cache.LineSize);
                break;
                
            case RelationProcessorPackage:
                printf("Physical Package/Socket\n");
                break;
        }
    }
    
    printf("\nSummary:\n");
    printf("  Physical Cores: %d\n", physicalCores);
    printf("  Logical Processors: %d\n", logicalProcessors);
    printf("  Sockets: %d\n", get_socket_count());
    printf("  NUMA Nodes: %d\n", get_numa_node_count());
    printf("  Hyper-Threading: %s\n", has_hyperthreading() ? "Enabled" : "Disabled");
    
    free(pBuffer);
}
#endif
/* -------------------- Multithreading stuff -------------------- */

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

void atomic_inc(atomic_int_t* var)
{
    #ifdef _WIN32
        InterlockedIncrement(var);
    #else
        atomic_fetch_add(var, 1);
    #endif
}

void atomic_dec(atomic_int_t* var)
{
    #ifdef _WIN32
        InterlockedDecrement(var);
    #else
        atomic_fetch_sub(var, 1);
    #endif
}

int atomic_load_int(atomic_int_t* var)
{
    #ifdef _WIN32
        return InterlockedCompareExchange(var, 0, 0);  // Atomic read trick
    #else
        return atomic_load(var);
    #endif
}

thread_handle_t create_thread(thread_func_t func, thread_func_param_t data)
{
    #ifdef _WIN32
        return CreateThread(NULL,  // security attribute -no idea- NULL means default 
                            0,     // stack size zero means default size 
                            func,  // pointer to the function to be executed by the thread
                            data,  // pointer to the params passed to the thread
                            0,     // run immediately
                            NULL); // dont return identifier
    #else
        pthread_t thread;
        pthread_create(&thread, NULL, func, data);
        return thread;
    #endif
}

void join_thread(thread_handle_t thread) 
{
    #ifdef _WIN32
        WaitForSingleObject(thread, INFINITE);  // return only when thread is signaled
        CloseHandle(thread);
    #else
        pthread_join(thread, NULL);
    #endif
}

void mutex_init(mutex_handle_t* mutex)
{
    #ifdef _WIN32
        InitializeCriticalSection(mutex);
    #else
        pthread_mutex_init(mutex, NULL);
    #endif
}

void mutex_destroy(mutex_handle_t* mutex)
{
    #ifdef _WIN32
        DeleteCriticalSection(mutex);
    #else
        pthread_mutex_destroy(mutex);
    #endif
}

void mutex_lock(mutex_handle_t* mutex)
{
    #ifdef _WIN32
        EnterCriticalSection(mutex);
    #else
        pthread_mutex_lock(mutex);
    #endif
}

void mutex_unlock(mutex_handle_t* mutex)
{
    #ifdef _WIN32
        LeaveCriticalSection(mutex);
    #else
        pthread_mutex_unlock(mutex);
    #endif
}

void cond_init(cond_handle_t* cond)
{
    #ifdef _WIN32
        InitializeConditionVariable(cond);
    #else
        pthread_cond_init(cond, NULL);
    #endif
}

void cond_destroy(cond_handle_t* cond)
{
    #ifdef _WIN32
        (void) cond; // no cleanup ?
    #else
        pthread_cond_destroy(cond);
    #endif
}

void cond_wait(cond_handle_t* cond, mutex_handle_t* mutex)
{
    #ifdef _WIN32
        SleepConditionVariableCS(cond, mutex, INFINITE);
    #else
        pthread_cond_wait(cond, mutex);
    #endif
}

void cond_signal(cond_handle_t* cond)
{
    #ifdef _WIN32
        WakeConditionVariable(cond);
    #else
        pthread_cond_signal(cond);
    #endif
}

void cond_broadcast(cond_handle_t* cond)
{
    #ifdef _WIN32
        /* 
            The WakeAllConditionVariable wakes all waiting threads
            while the WakeConditionVariable wakes only a single thread. 
        */
        WakeAllConditionVariable(cond);
    #else
        pthread_cond_broadcast(cond);
    #endif
}

void event_create(event_handle *event)
{
#ifdef _WIN32
    event = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
    pthread_mutex_init(event->mutex, NULL);
    pthread_cond_init(&event->cond, NULL);
    event->signaled = false;
#endif
}

void event_destroy(event_handle *event)
{
#ifdef _WIN32    
    if (event != NULL) {
        CloseHandle((HANDLE)event);
    }
#else
    pthread_cond_destroy(&event->cond);
    pthread_mutex_destroy(&event->mutex);
#endif
}

bool event_wait(event_handle *event)
{
#ifdef _WIN32
    if (event == NULL) {
        return FALSE;
    }
    DWORD result = WaitForSingleObject((HANDLE)event, INFINITE);
    return result == WAIT_OBJECT_0;
#else
    if (event == NULL) {
        return false;
    }
    
    event_t *e = (event_t *)event;
    pthread_mutex_lock(&e->mutex);
    
    while (!e->signaled) {
        pthread_cond_wait(&e->cond, &e->mutex);
    }
    
    e->signaled = false;  // Auto-reset behavior
    pthread_mutex_unlock(&e->mutex);
    
    return true;
#endif
}

bool event_activate(void *event)
{
#ifdef _WIN32
    if (event == NULL) {
        return FALSE;
    }
    return SetEvent((HANDLE)event) != 0;
#else
    if (event == NULL) {
        return false;
    }
    
    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    pthread_cond_signal(&event->cond);
    pthread_mutex_unlock(&event->mutex);
    
    return true;
#endif
}

thread_func_ret_t thread_loop(thread_func_param_t param)
{
    thread_pool_t* pool = (thread_pool_t*)param;

    job_t job = {0};

    while (true) 
    {
        mutex_lock(&pool->queue_mutex);
        {
            /*
                sleep as long as there are no pending jobs
             */
            while (arrlen(pool->jobs) == 0 &&
                   !pool->should_terminate) 
            {
                cond_wait(&pool->mutex_condition, &pool->queue_mutex);
            }
            
            if (pool->should_terminate) 
            {
                mutex_unlock(&pool->queue_mutex);
                return 0;
            }
            /*
                pull the job from the queue
             */
            job = pool->jobs[0];
            arrdel(pool->jobs, 0);
        }
        mutex_unlock(&pool->queue_mutex);
        
        atomic_inc(&pool->active_jobs);
        job.func(job.data);
        atomic_dec(&pool->active_jobs);
    }
    
    return 0;
}

thread_pool_t* threadpool_create(void)
{
    thread_pool_t* pool = (thread_pool_t*)malloc(sizeof(thread_pool_t));
    if (!pool) return NULL;
    
    pool->threads = NULL;  
    pool->jobs = NULL;     
    pool->should_terminate = false;
    pool->active_jobs = 0; 

    mutex_init(&pool->queue_mutex);
    cond_init(&pool->mutex_condition);
    
    uint32_t num_threads = get_core_count();
    
    for (uint32_t i = 0; i < num_threads; ++i) {
        thread_handle_t thread = create_thread(thread_loop, (thread_func_param_t)pool);
        arrput(pool->threads, thread);
    }
    
    return pool;
}

void threadpool_destroy(thread_pool_t* pool)
{
    if (!pool) return;
    
    mutex_lock(&pool->queue_mutex);
    {
        pool->should_terminate = true;
    }
    mutex_unlock(&pool->queue_mutex);
    
    cond_broadcast(&pool->mutex_condition);
    
    for (int i = 0; i < arrlen(pool->threads); ++i) {
        join_thread(pool->threads[i]);
    }
    
    arrfree(pool->threads);
    arrfree(pool->jobs);
    
    mutex_destroy(&pool->queue_mutex);
    cond_destroy(&pool->mutex_condition);
    
    free(pool);
}

void threadpool_queue_job(thread_pool_t* pool, job_func_t func, void* data)
{
    job_t job = { .func = func, .data = data };
    
    mutex_lock(&pool->queue_mutex);
    {
        arrput(pool->jobs, job);
    }
    mutex_unlock(&pool->queue_mutex);
    
    // notify a thread to pick up the job
    cond_signal(&pool->mutex_condition);
}

/*
    there is neither pending nor in progress jobs
 */
bool threadpool_busy(thread_pool_t* pool)
{
    bool busy = false;
    
    mutex_lock(&pool->queue_mutex);
    {
        busy = (arrlen(pool->jobs) > 0 ||
                pool->active_jobs > 0); 
    }
    mutex_unlock(&pool->queue_mutex);
    
    return busy;
}


/* -------------------- Processes stuff -------------------- */

int set_non_blocking(pipe_handle fd)
{
    #ifdef _WIN32
        return 0;
    #else
        int flags = fcntl(fd, F_GETFL, 0);
        if(flags == -1) return -1;
        return fcntl(fd, F_SETFL, flags|O_NONBLOCK);
    #endif
}

int spawn_process(process_t *proc, const char *command)
{
#ifdef _WIN32
    pipe_handle hReadPipe, hWritePipe;

    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    char cmdline[512];
    
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    /* Anonymous Pipe */
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) 
    {
        fprintf(stderr, "CreatePipe failed: %lu\n", GetLastError());
        return -1;
    }
    
    /*
        In Win32, child processes do not automatically inherit 
        all handles from the parent process like linux 
     */
    if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0)) 
    {
        fprintf(stderr, "SetHandleInformation failed: %lu\n", GetLastError());
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return -1;
    }
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    
    ZeroMemory(&pi, sizeof(pi));
    
    snprintf(cmdline, sizeof(cmdline), "cmd.exe /c %s", command);
    
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) 
    {
        fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return -1;
    }
    
    CloseHandle(hWritePipe);
    CloseHandle(pi.hThread);
    
    proc->pid = pi.dwProcessId;
    proc->pipe_fd = hReadPipe;
    proc->hProcess = pi.hProcess;
    strncpy(proc->cmd, command, sizeof(proc->cmd) - 1);
    proc->cmd[sizeof(proc->cmd) - 1] = '\0';
    proc->active = 1;
    
    return 0;
#else
    pipe_handle fildes[2]; // refer to the open file descriptions for the read and write ends of the pipe
    int status;

    status = pipe(fildes);
    if(status == -1)
    {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();

    switch (pid) 
    {
        case -1: /* Handle error */
            perror("fork");
            close(fildes[0]);                     
            close(fildes[1]);       
            return -1;              
            break;

        case 0:  /* Child - writes to the pipe */
            close(fildes[0]);    // read end is unused we want to read the child output 

            // redirect stdout and stderr
            if (dup2(fildes[1], STDOUT_FILENO) == -1) {
                perror("dup2");
            }

            if (dup2(fildes[1], STDERR_FILENO) == -1) {
                perror("dup2");
            }

            close(fildes[1]);                     /* Finished with pipe */

            // replaces the current process image with a new process image
            execl("/bin/sh", "sh", "-c", command, (char *)NULL);

            perror("execl");

            exit(1);

        default:  /* Parent - reads from the pipe */
            close(fildes[1]);                       /* Write end unused we read from child processes */

            if (set_non_blocking(fildes[0]) == -1)
            {
                perror("set_nonblocking");
                close(fildes[0]);
                return -1;
            }
            proc->pid = pid;
            proc->pipe_fd = fildes[0];

            strncpy(proc->cmd, command, sizeof(proc->cmd) - 1);
            proc->cmd[sizeof(proc->cmd) - 1] = '\0';
            proc->active = 1;
            break;
    }
    return 0;
#endif
}

#define BUFFER_SIZE 1024

void check_process_output(process_t *proc) 
{
    char buffer[BUFFER_SIZE];

#ifdef _WIN32
    DWORD bytes_read;
    DWORD bytes_avail;
    
    while (PeekNamedPipe(proc->pipe_fd, NULL, 0, NULL, &bytes_avail, NULL) && bytes_avail > 0) 
    {
        DWORD to_read = (bytes_avail < sizeof(buffer) - 1) 
                        ? bytes_avail 
                        : sizeof(buffer) - 1;
        
        if (ReadFile(proc->pipe_fd, buffer, to_read, &bytes_read, NULL)) 
        {
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                printf("[PID %lu - %s]: %s", proc->pid, proc->cmd, buffer);
                fflush(stdout);
            }
        } 
        else 
        {
            DWORD error = GetLastError();
            if (error != ERROR_BROKEN_PIPE) {
                fprintf(stderr, "ReadFile failed: %lu\n", error);
            }
            break;
        }
    }    
#else
    ssize_t bytes_read;
    
    // as long as there are stuff to read read, maybe limit it so we dont get stuck ?
    while ((bytes_read = read(proc->pipe_fd, buffer, sizeof(buffer) - 1)) > 0) 
    {
        buffer[bytes_read] = '\0';
        printf("[PID %d - %s]: %s", proc->pid, proc->cmd, buffer);
        fflush(stdout);
    }
    
    if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) 
    {
        perror("read");
    }
#endif
}

int check_process_status(process_t *proc) 
{
#ifdef _WIN32
    DWORD exit_code;
    
    if (!GetExitCodeProcess(proc->hProcess, &exit_code)) {
        fprintf(stderr, "GetExitCodeProcess failed: %lu\n", GetLastError());
        return 0;
    }
    
    if (exit_code == STILL_ACTIVE) {
        return 1;
    }
    
    check_process_output(proc);
    
    CloseHandle(proc->pipe_fd);
    CloseHandle(proc->hProcess);
    
    proc->active = 0;
    
    printf("[PID %lu - %s]: Exited with status %lu\n", 
           proc->pid, proc->cmd, exit_code);
    
    return 0;
#else
    int status;
    pid_t result = waitpid(proc->pid, &status, WNOHANG);
    
    if (result == 0) 
    {
        // Process still running
        return 1;
    } 
    else if (result == proc->pid) 
    {
        // Read any remaining output
        check_process_output(proc);
        
        close(proc->pipe_fd);
        proc->active = 0;
        
        if (WIFEXITED(status)) 
        {
            printf("[PID %d - %s]: Exited with status %d\n", 
                   proc->pid, proc->cmd, WEXITSTATUS(status));
        } 
        else if (WIFSIGNALED(status)) 
        {
            printf("[PID %d - %s]: Killed by signal %d\n", 
                   proc->pid, proc->cmd, WTERMSIG(status));
        }
        
        return 0;
    } 
    else 
    {
        perror("waitpid");
        return 0;
    }
#endif
}
