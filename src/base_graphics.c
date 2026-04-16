#include "base_graphics.h"
#include "util.h"

scissor_region_t scissor_stack[MAX_SCISSOR_STACK];
u32 scissor_stack_size;
rect_t current_scissor;
bool scissor_enabled;

/* -------------------- Color stuff -------------------- */

/*
    convert normalized color to 0->255 range
*/
color4_t to_color4(vec3f_t const c)
{
    color4_t color = {
        .r = (u8)Clamp(0.0f, c.x, 255.0f),
        .g = (u8)Clamp(0.0f, c.y, 255.0f),
        .b = (u8)Clamp(0.0f, c.z, 255.0f),
        .a = (u8)255
    };

    return color;
}

/* 
    Approximation gamma = 2.0, powf(x, 1.0f / 2.4) is more accurate
    good enough for most cases
*/
color4_t linear_to_gamma(color4_t color)
{
    color4_t col = {0.0f,0.0f,0.0f,0.0f};

    if(color.r > 0){
        col.r = sqrt_f32(color.r);
    } 
    if(color.g > 0){
        col.g = sqrt_f32(color.g);
    }
    if(color.b > 0){
        col.b = sqrt_f32(color.b);
    }
    col.a = color.a;

    return col;
}

/*
    The definition uses two different functions – a straight line and an exponential curve – glued together at a certain “cutoff point”.
    The implication is that these functions (the ones in the sRGB to Linear definition) intersect at the point: (0.04045, 0.0031308)
    - condition : 0 ≤ S ≤ 0.0404482362771082 , value L = S/12.92
    - condition : 0.0404482362771082 < S ≤ 1, L = ((S+0.055)/1.055)^2.4
 */
float srgb_to_linear_f(float c) 
{
    if (c >1.0f) c = c / 255.0f;
    return (c <= 0.04045f) ? (c / 12.92f) : powf((c + 0.055f) / 1.055f, 2.4f);
}

float linear_to_srgb_f(float c) 
{
    if (c >1.0f) c = c / 255.0f;
    return (c <= 0.0031308f) ? (12.92f * c) : (1.055f * powf(c, 1.0f / 2.4f) - 0.055f);
}

color4_t srgb_to_linear(color4_t color) 
{
    return (color4_t){
        (u8)(srgb_to_linear_f(color.r / 255.0f) * 255.0f + 0.5f),
        (u8)(srgb_to_linear_f(color.g / 255.0f) * 255.0f + 0.5f),
        (u8)(srgb_to_linear_f(color.b / 255.0f) * 255.0f + 0.5f),
        (u8)color.a
    };
}

color4_t linear_to_srgb(color4_t color) 
{
    return (color4_t){
        (u8)(linear_to_srgb_f(color.r / 255.0f) * 255.0f + 0.5f),
        (u8)(linear_to_srgb_f(color.g / 255.0f) * 255.0f + 0.5f),
        (u8)(linear_to_srgb_f(color.b / 255.0f) * 255.0f + 0.5f),
        (u8)color.a
    };
}

/* 
    h : the hue, the pure color
    s : the saturation, color's intensity or purity
    v : the value, color's brightness
 */
void hsv_to_rgb(f32 h, f32 s, f32 v, f32 *r, f32 *g, f32 *b) 
{
    i32 i = (i32)(h * 6.0f);
    f32 f = h * 6.0f - i;
    f32 p = v * (1.0f - s);
    f32 q = v * (1.0f - f * s);
    f32 t = v * (1.0f - (1.0f - f) * s);
    
    switch (i % 6) 
    {
        case 0: 
            *r = v; *g = t; *b = p; 
            break;
        case 1: 
            *r = q; *g = v; *b = p; 
            break;
        case 2: 
            *r = p; *g = v; *b = t; 
            break;
        case 3: 
            *r = p; *g = q; *b = v; 
            break;
        case 4: 
            *r = t; *g = p; *b = v; 
            break;
        case 5: 
            *r = v; *g = p; *b = q; 
            break;
    }
}

/*
    Perceptually uniform color space by Bjorn Ottosson
    produces smooth, natural transitions without muddy
    end points
    Recommended for blending colors
 */
static color4_t srgb_to_oklab(color4_t c) 
{
    // Convert u8 to linear float [0,1]
    float lR = srgb_to_linear_f(c.r / 255.0f);
    float lG = srgb_to_linear_f(c.g / 255.0f);
    float lB = srgb_to_linear_f(c.b / 255.0f);

    float l_ = 0.4122214708f*lR + 0.5363325363f*lG + 0.0514459929f*lB;
    float m_ = 0.2119034982f*lR + 0.6806995451f*lG + 0.1073969566f*lB;
    float s_ = 0.0883024619f*lR + 0.2817188376f*lG + 0.6299787005f*lB;

    float l_c = cbrtf(l_), m_c = cbrtf(m_), s_c = cbrtf(s_);

    float L = 0.2104542553f*l_c + 0.7936177850f*m_c - 0.0040720468f*s_c;
    float A = 1.9779984951f*l_c - 2.4285922050f*m_c + 0.4505937099f*s_c;
    float B = 0.0259040371f*l_c + 0.7827717662f*m_c - 0.8086757660f*s_c;

    // Pack L[0,1], A[-0.5,0.5], B[-0.5,0.5] into u8
    // L is already roughly [0,1]; A and B are roughly [-0.4,0.4] — bias to [0,1]
    return (color4_t){
        (u8)(L * 255.0f + 0.5f),
        (u8)((A + 0.5f) * 255.0f + 0.5f),
        (u8)((B + 0.5f) * 255.0f + 0.5f),
        c.a
    };
}

static color4_t oklab_to_srgb(color4_t lab) 
{
    // Unpack u8 back to floats
    float l = lab.r / 255.0f;
    float a = (lab.g / 255.0f) - 0.5f;
    float b = (lab.b / 255.0f) - 0.5f;

    float l_c = l + 0.3963377774f*a + 0.2158037573f*b;
    float m_c = l - 0.1055613458f*a - 0.0638541728f*b;
    float s_c = l - 0.0894841775f*a - 1.2914855480f*b;

    float l3 = l_c*l_c*l_c, m3 = m_c*m_c*m_c, s3 = s_c*s_c*s_c;

    float R = +4.0767416621f*l3 - 3.3077115913f*m3 + 0.2309699292f*s3;
    float G = -1.2684380046f*l3 + 2.6097574011f*m3 - 0.3413193965f*s3;
    float B = -0.0041960863f*l3 - 0.7034186147f*m3 + 1.7076147010f*s3;

    R = R < 0.0f ? 0.0f : (R > 1.0f ? 1.0f : R);
    G = G < 0.0f ? 0.0f : (G > 1.0f ? 1.0f : G);
    B = B < 0.0f ? 0.0f : (B > 1.0f ? 1.0f : B);

    return (color4_t){
        (u8)(linear_to_srgb_f(R) * 255.0f + 0.5f),
        (u8)(linear_to_srgb_f(G) * 255.0f + 0.5f),
        (u8)(linear_to_srgb_f(B) * 255.0f + 0.5f),
        lab.a
    };
}

// OKLCH: cylindrical form of OKLAB
// Packing: L -> r [0,1], C -> g [0,~0.4 max, stored /0.5], H -> b [0,1]
static color4_t oklab_to_oklch(color4_t lab) 
{
    float L = lab.r / 255.0f;
    float a = (lab.g / 255.0f) - 0.5f;
    float b = (lab.b / 255.0f) - 0.5f;

    float C = sqrtf(a*a + b*b);
    float H = atan2f(b, a) / (2.0f * 3.1415926535f);
    if (H < 0.0f) H += 1.0f;

    // C is in [0, ~0.4]; scale by /0.5 to fit [0,1] with headroom
    return (color4_t){
        (u8)(L * 255.0f + 0.5f),
        (u8)((C / 0.5f) * 255.0f + 0.5f),
        (u8)(H * 255.0f + 0.5f),
        lab.a
    };
}

static color4_t oklch_to_oklab(color4_t lch) 
{
    float L = lch.r / 255.0f;
    float C = (lch.g / 255.0f) * 0.5f;
    float H = lch.b / 255.0f;

    float h_rad = H * 2.0f * 3.1415926535f;
    float a = C * cosf(h_rad);
    float b = C * sinf(h_rad);

    return (color4_t){
        (u8)(L * 255.0f + 0.5f),
        (u8)((a + 0.5f) * 255.0f + 0.5f),
        (u8)((b + 0.5f) * 255.0f + 0.5f),
        lch.a
    };
}

static color4_t srgb_to_oklch(color4_t c) { return oklab_to_oklch(srgb_to_oklab(c)); }
static color4_t oklch_to_srgb(color4_t c) { return oklab_to_srgb(oklch_to_oklab(c)); }

color4_t color4_lerp(color4_t c1, color4_t c2, f32 t)
{
    t = Clamp(0.0f, t, 1.0f);
    
    color4_t result = {
        .r = (u8)(LERP_F32(c1.r, c2.r ,t)),
        .g = (u8)(LERP_F32(c1.g, c2.g ,t)),
        .b = (u8)(LERP_F32(c1.b, c2.b ,t)),
        .a = (u8)(LERP_F32(c1.a, c2.a ,t))
    };
    
    return result;
}

color4_t color4_lerp_oklab(color4_t c1, color4_t c2, f32 t)
{
    t = Clamp(0.0f, t, 1.0f);

    color4_t srgb1 =  linear_to_srgb(c1);
    color4_t srgb2 =  linear_to_srgb(c2);

    color4_t oklab1 =  srgb_to_oklab(srgb1);
    color4_t oklab2 =  srgb_to_oklab(srgb2);
    
    color4_t result = {
        .r = (u8)(LERP_F32(oklab1.r, oklab2.r ,t)),
        .g = (u8)(LERP_F32(oklab1.g, oklab2.g ,t)),
        .b = (u8)(LERP_F32(oklab1.b, oklab2.b ,t)),
        .a = (u8)(LERP_F32(oklab1.a, oklab2.a ,t))
    };

    return oklab_to_srgb(result);
}

color4_t average(color4_t c, color4_t d)
{
    return (color4_t){
        (u8)((c.r / 2) + (d.r / 2) + ((c.r & 1) & (d.r & 1))),
        (u8)((c.g / 2) + (d.g / 2) + ((c.g & 1) & (d.g & 1))),
        (u8)((c.b / 2) + (d.b / 2) + ((c.b & 1) & (d.b & 1))),
        (u8)((c.a / 2) + (d.a / 2) + ((c.a & 1) & (d.a & 1)))
    };
}

color4_t dark(color4_t c)
{
    return average(c, RGB_GREY(0));
}

color4_t lite(color4_t c)
{
    return average(c, COLOR_WHITE);
}

color4_t rgb_from_wavelength(f64 wave)
{
    f64 r = 0;
    f64 g = 0;
    f64 b = 0;

    // Spectral color mapping
    if (wave >= 380.0 && wave <= 440.0) {
        r = -1.0 * (wave - 440.0) / (440.0 - 380.0);
        b = 1.0;
    } else if (wave >= 440.0 && wave <= 490.0) {
        g = (wave - 440.0) / (490.0 - 440.0);
        b = 1.0;
    } else if (wave >= 490.0 && wave <= 510.0) {
        g = 1.0;
        b = -1.0 * (wave - 510.0) / (510.0 - 490.0);
    } else if (wave >= 510.0 && wave <= 580.0) {
        r = (wave - 510.0) / (580.0 - 510.0);
        g = 1.0;
    } else if (wave >= 580.0 && wave <= 645.0) {
        r = 1.0;
        g = -1.0 * (wave - 645.0) / (645.0 - 580.0);
    } else if (wave >= 645.0 && wave <= 780.0) {
        r = 1.0;
    }

    // Brightness adjustment
    f64 s = 1.0;
    if (wave > 700.0)
        s = 0.3 + 0.7 * (780.0 - wave) / (780.0 - 700.0);
    else if (wave <  420.0)
        s = 0.3 + 0.7 * (wave - 380.0) / (420.0 - 380.0);

    // Gamma correction
    r = pow(r * s, 0.8);
    g = pow(g * s, 0.8);
    b = pow(b * s, 0.8);
    
    // Create and return color_t
    color4_t color;
    color.r = (u8)(r * 255);
    color.g = (u8)(g * 255);
    color.b = (u8)(b * 255);
    color.a = 255;
    
    return color;
}

/* -------------------- Shapes stuff -------------------- */

/*
    Each pixel contains some color information
    consisting of three values: red, green and blue 
    and can also contain information about transparency with an
    additional alpha channel indicating the opacity of each pixel
 */
inline void set_pixel(image_view_t const *img, i32 x, i32 y, color4_t color) 
{
    
    if (Inside(x, 0, (i32)img->width) &&
        Inside(y, 0, (i32)img->height)) 
    {
        BUF_AT(img, x, y) = color;
    }
}

inline color4_t get_pixel(image_view_t const *img, i32 x, i32 y) 
{
    color4_t color = {0, 0, 0, 0};

    if (Inside(x, 0, (i32)img->width) &&
        Inside(y, 0, (i32)img->height)) 
    {
        color =  BUF_AT(img, x, y);
    }

    return color;
}

/* alpha blending to combine stuff depending on transparency */
inline color4_t blend_pixel(color4_t dst, color4_t src) 
{
    if (IS_OPAQUE(src))
    {
        return src;
    }

    if (IS_INVIS(src))  
    {
        return dst;
    }
    
    u8 inv_alpha = 255 - src.a;

    color4_t result = {
        .r = (src.r * src.a + dst.r * inv_alpha) * INV_255,
        .g = (src.g * src.a + dst.g * inv_alpha) * INV_255,
        .b = (src.b * src.a + dst.b * inv_alpha) * INV_255,
        .a = src.a + (dst.a * inv_alpha) * INV_255
    };

    return result;
}

/* Get existing color in the frame buffer and blend it witht the new color */
inline void set_pixel_blend(image_view_t const *img, i32 x, i32 y, color4_t color) 
{
    if (IS_OPAQUE(color)) 
    {
        set_pixel(img, x, y,color);
    } 
    else 
    {
        color4_t dst = get_pixel(img, x, y);
        set_pixel(img, x, y, blend_pixel(dst, color));
    }
}

// Used for anti-aliasing stuff
inline void set_pixel_weighted(image_view_t *img, i32 x, i32 y, color4_t color, u8 weight) 
{
    color4_t weighted = color;
    weighted.a = ClampTop(255, (color.a * weight) * INV_255);
    set_pixel_blend(img, x, y, weighted);
}

static void plot_4_points(image_view_t *img, i32 cx, i32 cy, i32 x, i32 y, f32 opacity, color4_t color)
{
    if (opacity <= 0.0f) return;
    
    u8 weight = opacity * 255.0f;
    
    if (x > 0 && y > 0) {
        set_pixel_weighted(img, cx + x, cy + y, color, weight);
        set_pixel_weighted(img, cx + x, cy - y, color, weight);
        set_pixel_weighted(img, cx - x, cy + y, color, weight);
        set_pixel_weighted(img, cx - x, cy - y, color, weight);
    }
    else if (x == 0) {
        set_pixel_weighted(img, cx + x, cy + y, color, weight);
        set_pixel_weighted(img, cx + x, cy - y, color, weight);
    }
    else if (y == 0) {
        set_pixel_weighted(img, cx + x, cy + y, color, weight);
        set_pixel_weighted(img, cx - x, cy + y, color, weight);
    }
}

void draw_pixel(image_view_t const *img, i32 x, i32 y, color4_t color) 
{
    set_pixel_blend(img, x, y, color);
}

void clear_screen(image_view_t const *color_buf, color4_t const color)
{
    for (u32 y = 0; y < color_buf->height; ++y)
    {
        for(u32 x = 0; x < color_buf->width; ++x)
        {    
            BUF_AT(color_buf, x, y) = color;
        }
    }
}

void clear_screen_radial_gradient(image_view_t const *color_buf,
                                       color4_t const color0,
                                       color4_t const color1)
{
    f32 center_x = color_buf->width * 0.5f;
    f32 center_y = color_buf->height * 0.5f;
    f32 max_dist_sq = center_x * center_x + center_y * center_y;
    
    for (u32 y = 0; y < color_buf->height; ++y) 
    {
        f32 dy = (f32)y - center_y;
        f32 dy_2 =  dy * dy;
        for (u32 x = 0; x < color_buf->width; ++x) 
        {
            f32 dx = (f32)x - center_x;
            f32 dist_sq = dx * dx + dy_2;
            
            f32 t = Clamp(0.0f, dist_sq / max_dist_sq, 1.0f);
            
            BUF_AT(color_buf, x, y) = color4_lerp(color0, color1, t);
        }
    }
}

void draw_hline(image_view_t const *color_buf, i32 y, i32 x0, i32 x1, color4_t const color)
{
    if (y < 0 || y >= (i32)color_buf->height){
        return;
    }

    if (x0 > x1) {
        SWAP(x0, x1, i32);
    }

    x0 = MAX(0, x0);
    x1 = MIN((i32)color_buf->width - 1, x1);
    
    for (i32 x = x0; x <= x1; x++) {
        draw_pixel(color_buf, x, y, color);
    }
}

void draw_vline(image_view_t const *color_buf, i32 x, i32 y0, i32 y1, color4_t const color)
{
    if(x < 0 || x >= (i32)color_buf->width){
        return;
    }

    if (y0 > y1) {
        SWAP(y0, y1, i32);
    }

    y0 = MAX(0, y0);
    y1 = MIN((i32)color_buf->height - 1, y1);
    
    for (i32 y = y0; y <= y1; y++) {
        draw_pixel(color_buf, x, y, color);
    }
}

/*
    Bresenham's line drawing algorithm
 */
void draw_line(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color)
{
    bool steep = false;
    
    if (abs(x0 - x1) < abs(y0 - y1)) {
        SWAP(x0, y0, i32);
        SWAP(x1, y1, i32);
        steep = true;
    }
    
    if (x0 > x1) {
        SWAP(x0, x1, i32);
        SWAP(y0, y1, i32);
    }
    
    i32 dx = x1 - x0;
    i32 dy = y1 - y0;
    i32 derror2 = abs(dy) * 2;
    i32 error2 = 0;
    i32 y = y0;
    
    for (i32 x = x0; x <= x1; x++) 
    {
        if (steep) 
        {
            draw_pixel(color_buf, y, x, color);
        } 
        else 
        {
            draw_pixel(color_buf, x, y, color);
        }
        
        error2 += derror2;
        if (error2 > dx) {
            y += (y1 > y0 ? 1 : -1);
            error2 -= dx * 2;
        }
    }
}

/*
    Xiaolin Wu's AA line implementation
 */
void draw_aaline(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t color)
{
    // Make sure the line runs top to bottom
    if (y0 > y1) {
        SWAP(x0, x1, i32);
        SWAP(y0, y1, i32);
    }
    
    // initial pixel
    draw_pixel(color_buf, x0, y0, color);
    
    i32 xdir, dx = x1 - x0;
    if (dx >= 0) {
        xdir = 1;
    } else {
        xdir = -1;
        dx = -dx; // make dx positive
    }
    
    i32 dy = y1 - y0;
    
    // special cases
    if (dy == 0) {
        draw_hline(color_buf, y0, x0, x1, color);
        return;
    }
    
    if (dx == 0) {
        draw_vline(color_buf, x0, y0, y1, color);
        return;
    }
    
    if (dx == dy) {
        draw_line(color_buf, x0, y0, x1, y1, color);
        return;
    }
    
    u32 error_adj;
    u32 error_acc = 0;
    
    if (dy > dx) 
    {
        error_adj = ((u32)dx << 16) / (u32)dy;
        
        for (i32 i = 0; i < dy - 1; i++) {
            u32 error_acc_temp = error_acc;
            error_acc += error_adj;
            if (error_acc <= error_acc_temp) {
                x0 += xdir;
            }
            y0++;
            
            u32 weighting = error_acc >> 8;
            set_pixel_weighted(color_buf, x0, y0, color, 255 - weighting);
            set_pixel_weighted(color_buf, x0 + xdir, y0, color, weighting);
        }
    } 
    else 
    {
        error_adj = ((u32)dy << 16) / (u32)dx;
        
        for (i32 i = 0; i < dx - 1; i++) 
        {
            u32 error_acc_temp = error_acc;
            error_acc += error_adj;
            if (error_acc <= error_acc_temp) {
                y0++;
            }
            x0 += xdir;
            
            u32 weighting = error_acc >> 8;
            set_pixel_weighted(color_buf, x0, y0, color, 255 - weighting);
            set_pixel_weighted(color_buf, x0, y0 + 1, color, weighting);
        }
    }
    
    draw_pixel(color_buf, x1, y1, color);
}

void draw_rect_outline_wh(image_view_t const *color_buf, i32 x0, i32 y0, i32 w , i32 h , color4_t const color)
{
   if (h == 1) 
   {
        draw_hline(color_buf, y0, x0, x0+w-1, color);
   }
   else if (w == 1)
   {
        draw_vline(color_buf, x0, y0, y0+h-1, color);
   }
   else if (h > 1 && w > 1) 
   {
      i32 x1 = x0+w-1, y1 = y0+h-1;
      draw_hline(color_buf, y0,x0,x1-1, color);
      draw_vline(color_buf, x1,y0,y1-1, color);
      draw_hline(color_buf, y1,x0+1,x1, color);
      draw_vline(color_buf, x0,y0+1,y1, color);
   }
}

void draw_rect_outline(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color)
{
    if (x1 < x0) 
    { 
        SWAP(x1, x0, i32);
    }
    if (y1 < y0) 
    { 
        SWAP(y1, y0, i32);
    }
    draw_rect_outline_wh(color_buf, x0,y0, x1-x0+1, y1-y0+1, color);
}

void draw_rect_solid_wh(image_view_t const *color_buf, i32 x0, i32 y0, i32 w , i32 h , color4_t const color)
{
    if (w > 0) 
    {   
        i32 j, x1 = x0 + w - 1;
        for (j=0; j < h; ++j)
        {
            draw_hline(color_buf, y0+j, x0, x1, color);
        }
    }
}
  
void draw_rect_solid(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color)
{
    if (x1 < x0) 
    { 
        SWAP(x1, x0, i32);
    }
    if (y1 < y0) 
    { 
        SWAP(y1, y0, i32);
    }
    draw_rect_solid_wh(color_buf, x0,y0, x1-x0+1, y1-y0+1, color);
}

void draw_triangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 x3, i32 y3, color4_t color) 
{
    draw_line(img, x1, y1, x2, y2, color);
    draw_line(img, x2, y2, x3, y3, color);
    draw_line(img, x3, y3, x1, y1, color);
}

void draw_triangle_filled_scanline(image_view_t *img, i32 y, i32 x1, i32 x2, color4_t color) 
{
    if (x1 > x2) { 
        SWAP(x1, x2, i32);
    }

    draw_hline(img, x1, x2, y, color);
}

void draw_triangle_filled(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 x3, i32 y3, color4_t color) 
{
    // sort by y-coordinate
    if (y1 > y2) { 
        SWAP(x1, x2, i32); 
        SWAP(y1, y2, i32); 
    }
    if (y2 > y3) { 
        SWAP(x2, x3, i32); 
        SWAP(y2, y3, i32); 
    }
    if (y1 > y2) { 
        SWAP(x1, x2, i32); 
        SWAP(y1, y2, i32); 
    }

    if (y1 == y3){
        return; 
    }
    
    for (i32 y = y1; y <= y3; y++) 
    {
        i32 xa, xb;
        
        if (y < y2) 
        {
            if (y2 != y1) 
            {
                xa = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            }
            else
            {
                xa = x1;
            } 
            if (y3 != y1)
            {
                xb = x1 + (x3 - x1) * (y - y1) / (y3 - y1);
            } 
            else 
            {
                xb = x1;
            }
        } 
        else 
        {
            if (y3 != y2)
            {
                xa = x2 + (x3 - x2) * (y - y2) / (y3 - y2);
            } 
            else
            {
                xa = x2;
            } 

            if (y3 != y1)
            {
                xb = x1 + (x3 - x1) * (y - y1) / (y3 - y1);
            }
            else
            {
                xb = x1;
            } 
        }
        
        draw_triangle_filled_scanline(img, y, xa, xb, color);
    }
}

/*
    Midpoint Circle Algorithm
    - Things to consider :
    1- circles are symmetrical, if we can draw and octant we can simpy just mirror them
    2- y increases downwards (0,0 top left)
 */
void draw_circle(image_view_t *img, i32 cx, i32 cy, i32 radius, color4_t color) 
{
    i32 x = 0;
    i32 y = radius;          // start at the bottom of the circle

    // decision parameter = ((x+1)^2 + (y-0.5)^2 - r^2) checks whether next step is inside or outside the circle
    // First iteration: x=0, y = radius -> d = 1 + (r-0.5)^2 - r^2 = 1 + r^2 - r + 0.25 - r^2 = (1.25 - r)
    // double and round  = 2.5 - 2r = 3 - 2r
    i32 d = 3 - 2 * radius;
    
    while (y >= x) 
    {
        draw_pixel(img, cx + x, cy + y, color);   // bottom right arc
        draw_pixel(img, cx - x, cy + y, color);   // bottom left arc
        draw_pixel(img, cx + x, cy - y, color);   // top right arc
        draw_pixel(img, cx - x, cy - y, color);   // top left arc
        draw_pixel(img, cx + y, cy + x, color);   // low mid right arc
        draw_pixel(img, cx - y, cy + x, color);   // low mid left arc
        draw_pixel(img, cx + y, cy - x, color);   // high mid right arc
        draw_pixel(img, cx - y, cy - x, color);   // high mid left arc
        
        x++;

        // decide whether we should decrement y or not (go up)
        // depending on whether the midpoint is inside the circle
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

/*
    Just connect the arcs with lines
 */
void draw_circle_filled(image_view_t *img, i32 cx, i32 cy, i32 radius, color4_t color) 
{
    i32 x = 0;
    i32 y = radius;
    i32 d = 3 - 2 * radius;
    
    while (y >= x) 
    {
        draw_hline(img, cy + y, cx - x, cx + x,  color);
        draw_hline(img, cy - y, cx - x, cx + x,  color);
        draw_hline(img, cy + x, cx - y, cx + y,  color);
        draw_hline(img, cy - x, cx - y, cx + y,  color);
        
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

/*
    Xiaolin Wu's AA circle algorithm implementation
 */
void draw_circle_aa(image_view_t *img, i32 cx, i32 cy, f32 radius, color4_t color) 
{
    if (radius <= 0) return;
    
    f32 rsq = radius * radius;
    i32 ffd = (i32)roundf(radius / d_sqrt(2.0f));
    
    for (i32 xi = 0; xi <= ffd; xi++) 
    {
        f32 yj = d_sqrt(rsq - xi * xi);
        f32 frc = yj - floorf(yj);  
        i32 flr = (i32)floorf(yj);
        
        plot_4_points(img, cx, cy, xi, flr, 1.0f - frc, color);
        plot_4_points(img, cx, cy, xi, flr + 1, frc, color);
    }
    
    for (i32 yi = 0; yi <= ffd; yi++) 
    {
        f32 xj = d_sqrt(rsq - yi * yi);
        f32 frc = xj - floorf(xj);  
        i32 flr = (i32)floorf(xj);
        
        plot_4_points(img, cx, cy, flr, yi, 1.0f - frc, color);
        plot_4_points(img, cx, cy, flr + 1, yi, frc, color);
    }
}

void draw_circle_filled_aa(image_view_t *img,
                           i32 cx, i32 cy,
                           f32 radius,
                           color4_t color)
{
    if (radius <= 0.0f) return;

    f32 rsq = radius * radius;

    for (i32 yi = (i32)ceilf(-radius);
         yi <= (i32)floorf(radius);
         yi++)
    {
        f32 fy = (f32)yi;
        f32 x_real = d_sqrt(rsq - fy * fy);

        i32 x_int = (i32)floorf(x_real);
        f32 frac = x_real - (f32)x_int;

        i32 y_screen = cy + yi;

        draw_hline(img,
                   y_screen,
                   cx - x_int,
                   cx + x_int,
                   color);

        u8 alpha = (u8)(frac * 255.0f);

        if (alpha > 0)
        {
            set_pixel_weighted(img,
                               cx + x_int + 1,
                               y_screen,
                               color,
                               alpha);

            set_pixel_weighted(img,
                               cx - x_int - 1,
                               y_screen,
                               color,
                               alpha);
        }
    }
}


void draw_ellipse(image_view_t *img, i32 cx, i32 cy, i32 rx, i32 ry, color4_t color) 
{
    if (rx <= 0 || ry <= 0) return;
    
    i32 rx2 = rx * rx;
    i32 ry2 = ry * ry;
    i32 tworx2 = 2 * rx2;
    i32 twory2 = 2 * ry2;
    i32 x = 0;
    i32 y = ry;
    i32 px = 0;
    i32 py = tworx2 * y;
    
    i32 p = ry2 - (rx2 * ry) + (rx2 / 4);
    while (px < py) 
    {
        draw_pixel(img, cx + x, cy + y, color);
        draw_pixel(img, cx - x, cy + y, color);
        draw_pixel(img, cx + x, cy - y, color);
        draw_pixel(img, cx - x, cy - y, color);
        
        x++;
        px += twory2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= tworx2;
            p += ry2 + px - py;
        }
    }
    
    p = (i32)(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) 
    {
        draw_pixel(img, cx + x, cy + y, color);
        draw_pixel(img, cx - x, cy + y, color);
        draw_pixel(img, cx + x, cy - y, color);
        draw_pixel(img, cx - x, cy - y, color);
        
        y--;
        py -= tworx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += twory2;
            p += rx2 - py + px;
        }
    }
}

void draw_ellipse_filled(image_view_t *img, i32 cx, i32 cy, i32 rx, i32 ry, color4_t color) 
{
    if (rx <= 0 || ry <= 0) return;
    
    i32 rx2 = rx * rx;
    i32 ry2 = ry * ry;
    i32 tworx2 = 2 * rx2;
    i32 twory2 = 2 * ry2;
    i32 x = 0;
    i32 y = ry;
    i32 px = 0;
    i32 py = tworx2 * y;
    
    i32 p = ry2 - (rx2 * ry) + (rx2 / 4);
    while (px < py) 
    {
        draw_hline(img, cy + y, cx - x, cx + x, color);
        draw_hline(img, cy - y, cx - x, cx + x, color);
        
        x++;
        px += twory2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= tworx2;
            p += ry2 + px - py;
        }
    }
    
    p = (i32)(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) 
    {
        draw_hline(img, cy + y, cx - x, cx + x, color);
        draw_hline(img, cy - y, cx - x, cx + x, color);
        
        y--;
        py -= tworx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += twory2;
            p += rx2 - py + px;
        }
    }
}

/*
    Extract the arc logic from the midpoint algorithm
 */
void draw_arc_quadrant(image_view_t *img, i32 cx, i32 cy, i32 radius, i32 quadrant, color4_t color) 
{
    i32 x = 0;
    i32 y = radius;
    i32 d = 3 - 2 * radius;
    
    while (y >= x) {
        switch (quadrant) {
            case 0: 
                draw_pixel(img, cx + x, cy - y, color);
                draw_pixel(img, cx + y, cy - x, color);
                break;
            case 1: 
                draw_pixel(img, cx - x, cy - y, color);
                draw_pixel(img, cx - y, cy - x, color);
                break;
            case 2: 
                draw_pixel(img, cx - x, cy + y, color);
                draw_pixel(img, cx - y, cy + x, color);
                break;
            case 3: 
                draw_pixel(img, cx + x, cy + y, color);
                draw_pixel(img, cx + y, cy + x, color);
                break;
        }
        
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

void draw_rounded_rectangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 radius, color4_t color) 
{
    if (x1 > x2) { SWAP(x1, x2, i32); }
    if (y1 > y2) { SWAP(y1, y2, i32); }
    
    i32 w = x2 - x1 + 1;
    i32 h = y2 - y1 + 1;
    
    radius = ClampTop(radius, MIN(w,h) / 2);
    
    if (radius <= 1) {
        draw_rect_outline(img, x1, y1, x2, y2, color);
        return;
    }
    
    draw_arc_quadrant(img, x1 + radius, y1 + radius, radius, 1, color);
    draw_arc_quadrant(img, x2 - radius, y1 + radius, radius, 0, color);
    draw_arc_quadrant(img, x1 + radius, y2 - radius, radius, 2, color);
    draw_arc_quadrant(img, x2 - radius, y2 - radius, radius, 3, color);
    
    draw_hline(img, y1, x1 + radius + 1, x2 - radius - 1, color); 
    draw_hline(img, y2, x1 + radius + 1, x2 - radius - 1, color); 
    draw_vline(img, x1, y1 + radius + 1, y2 - radius - 1, color); 
    draw_vline(img, x2, y1 + radius + 1, y2 - radius - 1, color); 
}

void draw_rounded_rectangle_filled_wh(image_view_t *img, i32 x, i32 y, i32 width, i32 height, i32 radius, color4_t color) 
{
    i32 x2 = x + width;
    i32 y2 = y + height;
    
    draw_rounded_rectangle_filled(img, x, y, x2, y2, radius, color);
}

void draw_rounded_rectangle_filled_simple(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 radius, color4_t color) 
{
    if (x1 > x2) { SWAP(x1, x2, i32); }
    if (y1 > y2) { SWAP(y1, y2, i32); }
    
    i32 w = x2 - x1 + 1;
    i32 h = y2 - y1 + 1;
    
    // Clamp radius to valid range
    radius = Clamp(0, radius, MIN(w,h) / 2);

    // No rounding draw regular rect
    if (radius <= 1) {
        draw_rect_solid(img, x1, y1, x2, y2, color);
        return;
    }

    // Test for special cases of straight lines or single point 
    if (x1 == x2) 
    {
        if (y1 == y2) {
            draw_pixel(img, x1, y1, color);
        } else {
            draw_vline(img, x1, y1, y2, color);
        }
        return;
    } 
    else 
    {
        if (y1 == y2) {
            draw_hline(img, y1, x1, x2, color);
            return;
        }
    }
    
    i32 px = radius;
    i32 py = 0;
    i32 d = 1 - radius;
    
    while (px >= py) 
    {
        draw_pixel(img, x1 + radius - px, y1 + radius - py, color);
        draw_pixel(img, x1 + radius - py, y1 + radius - px, color);
        
        draw_pixel(img, x2 - radius + px, y1 + radius - py, color);
        draw_pixel(img, x2 - radius + py, y1 + radius - px, color);
        
        draw_pixel(img, x1 + radius - px, y2 - radius + py, color);
        draw_pixel(img, x1 + radius - py, y2 - radius + px, color);
        
        draw_pixel(img, x2 - radius + px, y2 - radius + py, color);
        draw_pixel(img, x2 - radius + py, y2 - radius + px, color);
        
        // connect top-left to top-right
        draw_hline(img, y1 + radius - py, x1 + radius - px, x2 - radius + px, color);
        draw_hline(img, y1 + radius - px, x1 + radius - py, x2 - radius + py, color);
        
        // connect bottom-left to bottom-right
        draw_hline(img, y2 - radius + py, x1 + radius - px, x2 - radius + px, color);
        draw_hline(img, y2 - radius + px, x1 + radius - py, x2 - radius + py, color);
        
        py++;
        if (d < 0) {
            d += 2 * py + 1;
        } else {
            px--;
            d += 2 * (py - px) + 1;
        }
    }
    
    // middle rect
    draw_rect_solid_wh(img, x1, y1 + radius, w, h - 2 * radius, color);
}

void draw_rounded_rectangle_filled(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 radius, color4_t color) 
{
    if (x1 > x2) { SWAP(x1, x2, i32); }
    if (y1 > y2) { SWAP(y1, y2, i32); }
    
    i32 w = x2 - x1 + 1;
    i32 h = y2 - y1 + 1;
    
    // Clamp radius to valid range
    radius = Clamp(0, radius, MIN(w,h) / 2);

    // No rounding draw regular rect
    if (radius <= 1) {
        draw_rect_solid(img, x1, y1, x2, y2, color);
        return;
    }

    /*
    * Test for special cases of straight lines or single point 
    */
    if (x1 == x2) 
    {
        if (y1 == y2) {
            draw_pixel(img, x1, y1, color);
        } else {
            draw_vline(img, x1, y1, y2, color);
        }
        return;
    } 
    else 
    {
        if (y1 == y2) {
            draw_hline(img, y1, x1, x2, color);
            return;
        }
    }
    
    i32 x = x1 + radius;
    i32 y = y1 + radius;

    i32 dx = x2 - x1 - radius - radius;
    i32 dy = y2 - y1 - radius - radius;
    
    i32 cx = 0;
    i32 cy = radius;
    i32 ocx = 0xffff;
    i32 ocy = 0xffff;
    i32 df = 1 - radius;
    i32 d_e = 3;
    i32 d_se = -2 * radius + 5;
    
    do {
        i32 xpcx = x + cx;
        i32 xmcx = x - cx;
        i32 xpcy = x + cy;
        i32 xmcy = x - cy;
        
        if (ocy != cy) {
            if (cy > 0) {
                i32 ypcy = y + cy;
                i32 ymcy = y - cy;
                // Top and bottom horizontal lines for corners
                draw_hline(img,ypcy + dy, xmcx, xpcx + dx,  color);  // bottom
                draw_hline(img,ymcy, xmcx, xpcx + dx,  color);       // top
            } else {
                draw_hline(img,  y,xmcx, xpcx + dx, color);
            }
            ocy = cy;
        }
        
        if (ocx != cx) {
            if (cx != cy) {
                if (cx > 0) {
                    i32 ypcx = y + cx;
                    i32 ymcx = y - cx;
                    draw_hline(img, ymcx,xmcy, xpcy + dx,  color);
                    draw_hline(img, ypcx + dy,xmcy, xpcy + dx,  color);
                } else {
                    draw_hline(img, y,xmcy, xpcy + dx,  color);
                }
            }
            ocx = cx;
        }
        
        if (df < 0) {
            df += d_e;
            d_e += 2;
            d_se += 2;
        } else {
            df += d_se;
            d_e += 2;
            d_se += 4;
            cy--;
        }
        cx++;
    } while (cx <= cy);
    
    draw_rect_solid(img, x1, y1 + radius + 1, x2, y2 - radius, color);
}

void draw_rounded_rectangle_filled_aa_wh(image_view_t *img, i32 x, i32 y, i32 width, i32 height, f32 radius, color4_t color) 
{
    i32 x2 = x + width;
    i32 y2 = y + height;
    
    draw_rounded_rectangle_filled_aa(img, x, y, x2, y2, radius, color);
}

void draw_rounded_rectangle_filled_aa(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, f32 radius, color4_t color) 
{
    if (x1 > x2) { SWAP(x1, x2, i32); }
    if (y1 > y2) { SWAP(y1, y2, i32); }
    
    i32 w = x2 - x1 + 1;
    i32 h = y2 - y1 + 1;
    
    // Clamp radius to valid range
    radius = Clamp(0.0f, radius, (f32)MIN(w, h) / 2.0f);

    // No rounding - draw regular rect
    if (radius <= 1.0f) {
        draw_rect_solid(img, x1, y1, x2, y2, color);
        return;
    }

    // Special cases
    if (x1 == x2) {
        if (y1 == y2) {
            draw_pixel(img, x1, y1, color);
        } else {
            draw_vline(img, x1, y1, y2, color);
        }
        return;
    } else if (y1 == y2) {
        draw_hline(img, y1, x1, x2, color);
        return;
    }
    
    i32 cx_left = x1 + (i32)radius;
    i32 cx_right = x2 - (i32)radius;
    i32 cy_top = y1 + (i32)radius;
    i32 cy_bottom = y2 - (i32)radius;
    
    f32 radiusX2 = radius * radius;
    f32 radiusY2 = radius * radius;
    
    i32 quarter = (i32)roundf(radiusX2 / d_sqrt(radiusX2 + radiusY2));
    
    for (i32 x = 0; x <= quarter; x++)
    {
        f32 y = radius * d_sqrt(1.0f - (f32)(x * x) / radiusX2);
        f32 error = y - floorf(y);
        
        u8 transparency = (u8)(error * 255.0f);
        i32 y_floor = (i32)floorf(y);
        
        draw_hline(img, cy_top - y_floor, cx_left - x, cx_left + x, color);

        set_pixel_weighted(img, cx_left + x, cy_top - y_floor - 1, color, transparency);
        set_pixel_weighted(img, cx_left - x, cy_top - y_floor - 1, color, transparency);
        
        draw_hline(img, cy_top - y_floor, cx_right - x, cx_right + x, color);

        set_pixel_weighted(img, cx_right + x, cy_top - y_floor - 1, color, transparency);
        set_pixel_weighted(img, cx_right - x, cy_top - y_floor - 1, color, transparency);
        
        draw_hline(img, cy_bottom + y_floor, cx_left - x, cx_left + x, color);

        set_pixel_weighted(img, cx_left + x, cy_bottom + y_floor + 1, color, transparency);
        set_pixel_weighted(img, cx_left - x, cy_bottom + y_floor + 1, color, transparency);
        
        draw_hline(img, cy_bottom + y_floor, cx_right - x, cx_right + x, color);

        set_pixel_weighted(img, cx_right + x, cy_bottom + y_floor + 1, color, transparency);
        set_pixel_weighted(img, cx_right - x, cy_bottom + y_floor + 1, color, transparency);
    }
    
    quarter = (i32)roundf(radiusY2 / d_sqrt(radiusX2 + radiusY2));
    
    for (i32 y = 0; y <= quarter; y++)
    {
        f32 x = radius * d_sqrt(1.0f - (f32)(y * y) / radiusY2);
        f32 error = x - floorf(x);
        
        u8 transparency = (u8)(error * 255.0f);
        i32 x_floor = (i32)floorf(x);

        draw_hline(img, cy_top - y, cx_left - x_floor, cx_left + x_floor, color);

        set_pixel_weighted(img, cx_left - x_floor - 1, cy_top - y, color, transparency);
        
        draw_hline(img, cy_top - y, cx_right - x_floor, cx_right + x_floor, color);

        set_pixel_weighted(img, cx_right + x_floor + 1, cy_top - y, color, transparency);
        
        draw_hline(img, cy_bottom + y, cx_left - x_floor, cx_left + x_floor, color);

        set_pixel_weighted(img, cx_left - x_floor - 1, cy_bottom + y, color, transparency);
        
        draw_hline(img, cy_bottom + y, cx_right - x_floor, cx_right + x_floor, color);

        set_pixel_weighted(img, cx_right + x_floor + 1, cy_bottom + y, color, transparency);
    }
    
    draw_rect_solid(img, x1, cy_top, x2, cy_bottom, color);
    
    draw_rect_solid(img, cx_left, y1, cx_right, cy_top - 1, color);
    draw_rect_solid(img, cx_left, cy_bottom + 1, cx_right, y2, color);
}

/*
    to check whether a point lies inside a polygon 
        - compare each side of the polygon to the Y (vertical) coordinate of the test point
        - compile list of intersections and sort them in x-axis
        - if there are an odd number of nodes on each side of the test point, then it is inside the polygon
    Notes :
        - if the polygon crosses itself it exhibits an exclusive or behaviour
        - Points which are exactly on the Y threshold must be considered to belong to one side of the threshold.
 */
void draw_filled_polygon(image_view_t *img, const i32 *vx, const i32 *vy, i32 n, color4_t color)
{
    if (vx == NULL ||
        vy == NULL ||
        n < 3) 
    {
        return;
    }

    i32 miny = vy[0];
    i32 maxy = vy[0];
    for (i32 i = 1; i < n; i++) {
        if (vy[i] < miny) miny = vy[i];
        if (vy[i] > maxy) maxy = vy[i];
    }

    i32 *intersections = (i32 *)malloc(sizeof(i32) * n);
    if (intersections == NULL) {
        return;
    }

    for (i32 y = miny; y <= maxy; y++) 
    {
        i32 num_intersections = 0;

        // Find all intersections with this scanline
        for (i32 i = 0; i < n; i++) 
        {
            i32 i1 = i;
            i32 i2 = (i + 1) % n;
            
            i32 y1 = vy[i1];
            i32 y2 = vy[i2];
            
            // Skip horizontal edges
            if (y1 == y2) {
                continue;
            }
            
            // Ensure y1 < y2 for consistent processing
            i32 x1, x2;
            if (y1 < y2) 
            {
                x1 = vx[i1];
                x2 = vx[i2];
            } 
            else 
            {
                SWAP(y1, y2, i32);
                x1 = vx[i2];
                x2 = vx[i1];
            }
            
            // Check if scanline intersects this edge
            // vertex is only counted by the edge below it, not the edge above it.
            // this amazingly solves alot of edge cases 
            if (y >= y1 &&
                y < y2) 
            {
                i32 intersection = EDGE_INTERSECT_FP(y, y1, y2, x1, x2);
                intersections[num_intersections++] = intersection;
            }
        }

        for (i32 i = 0; i < num_intersections - 1; i++) 
        {
            for (i32 j = i + 1; j < num_intersections; j++) 
            {
                if (intersections[i] > intersections[j]) 
                {
                    SWAP(intersections[i], intersections[j], i32);
                }
            }
        }

        /* 
            Skip through intersections in pairs to identify when inside the polygon
         */
        for (i32 i = 0; i < num_intersections; i += 2) 
        {
            if (i + 1 < num_intersections) 
            {
                i32 x_start = (intersections[i] + 255) >> 8;  // Round up
                i32 x_end = (intersections[i + 1] - 1) >> 8;  // Round down
                
                if (x_start <= x_end) {
                    draw_hline(img, y, x_start, x_end, color);
                }
            }
        }
    }

    free(intersections);
}

void draw_thick_line(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 width, color4_t color)
{
    if (width < 1) {
        return;
    }

    if ((x1 == x2) && (y1 == y2)) 
    {
        i32 radius = width / 2;
        i32 size = 2 * radius + 1; 
        draw_rect_solid_wh(img, x1 - radius, y1 - radius, size, size, color);
        return;
    }

    if (width == 1) 
    {
        draw_line(img, x1, y1, x2, y2, color);
        return;
    }

    // Calculate offsets for sides
    f64 dx = (f64)(x2 - x1);
    f64 dy = (f64)(y2 - y1);
    f64 l = sqrt(dx * dx + dy * dy);
    f64 ang = atan2(dx, dy);
    f64 adj = 0.1 + 0.9 * fabs(cos(2.0 * ang));
    f64 wl2 = ((f64)width - adj) / (2.0 * l);
    f64 nx = dx * wl2;
    f64 ny = dy * wl2;

    // Build polygon for the thick line
    i32 px[4], py[4];
    px[0] = (i32)(x1 + ny);
    px[1] = (i32)(x1 - ny);
    px[2] = (i32)(x2 - ny);
    px[3] = (i32)(x2 + ny);
    py[0] = (i32)(y1 - nx);
    py[1] = (i32)(y1 + nx);
    py[2] = (i32)(y2 + nx);
    py[3] = (i32)(y2 - nx);

    // Draw the thick line as a filled polygon
    draw_filled_polygon(img, px, py, 4, color);
}

void draw_thick_aaline(image_view_t *img , i32 x1, i32 y1, i32 x2, i32 y2, i32 width, color4_t color)
{
    if (width < 1) return;
    
    if (width == 1) {
        draw_aaline(img, x1, y1, x2, y2, color);
        return;
    }

    if ((x1 == x2) && (y1 == y2)) 
    {
        i32 radius = width / 2;
        i32 size = 2 * radius + 1;
        draw_rect_solid_wh(img, x1 - radius, y1 - radius, size, size, color);
        return;
    }

    f64 dx = x2 - x1;
    f64 dy = y2 - y1;
    f64 length = sqrt(dx * dx + dy * dy);
    if (length == 0) return;
    
    f64 nx = -dy / length;
    f64 ny = dx / length;
    f64 hw = width / 2.0;
    f64 offset_x = nx * hw;
    f64 offset_y = ny * hw;

    i32 px[4], py[4];
    px[0] = (i32)(x1 + offset_x); py[0] = (i32)(y1 + offset_y);  // top start
    px[1] = (i32)(x1 - offset_x); py[1] = (i32)(y1 - offset_y);  // bottom start  
    px[2] = (i32)(x2 - offset_x); py[2] = (i32)(y2 - offset_y);  // bottom end
    px[3] = (i32)(x2 + offset_x); py[3] = (i32)(y2 + offset_y);  // top end

    draw_filled_polygon(img, px, py, 4, color);

    draw_aaline(img, px[0], py[0], px[3], py[3], color);  // top edge
    draw_aaline(img, px[1], py[1], px[2], py[2], color);  // bottom edge
}

#define MAX_POLY_CORNERS 256
#define SPLINE    2.
#define CUBIC     4.
#define NEW_LOOP  3.
#define END      -2.

/*
    F = (1.0 - t)^2 * p0.y + 2(1.0 - t) * t * p1.y + t^2 * p2.y
    F = (1.0 - 2t + t^2) * p0.y + (2t - 2t^2) * p1.y + t^2 * p2.y 

    t^2 [ p0.y - 2p1.y + p2.y ] +
    t   [ 2(p1.y - p0.y) ] +
    p0.y 

    a = [ p0.y - 2p1.y + p2.y ]
    b = [ 2(p1.y - p0.y) ]
    c = p0.y

    F = -b +- sqrt({b^2 - 4ac} \ 2a)
 */

void draw_spline_polygon_filled(image_view_t *img, double *poly, int IMAGE_LEFT, int IMAGE_RIGHT, int IMAGE_TOP, int IMAGE_BOT , color4_t color) 
{
    int nodes, nodeX[MAX_POLY_CORNERS], i, j, k, start, swap;
    double Sx, Sy, Ex, Ey, p1x, p1y, sRoot, F, plusOrMinus, topPart, bottomPart, xPart;

    for (int pixelY=IMAGE_TOP; pixelY<IMAGE_BOT; pixelY++) 
    {
        double Y = (double)pixelY + 0.000001;
        nodes = 0;
        i = 0;
        start = 0;

        while (poly[i] != END) 
        {
            j=i+2;
            if (poly[i]==SPLINE) j++;
            if (poly[i]==CUBIC)  j+=2;
            if (poly[j]==END || poly[j]==NEW_LOOP) j=start;

            if (poly[i]!=SPLINE && poly[i]!=CUBIC &&
                poly[j]!=SPLINE && poly[j]!=CUBIC) 
            {
                //  STRAIGHT LINE
                if ((poly[i+1]<Y && poly[j+1]>=Y) ||
                    (poly[j+1]<Y && poly[i+1]>=Y)) 
                {
                    nodeX[nodes++]=(int)(poly[i]+(Y-poly[i+1])/(poly[j+1]-poly[i+1])*(poly[j]-poly[i]));
                }
            }
            else if (poly[j]==SPLINE) 
            {
                //  QUADRATIC SPLINE
                p1x=poly[j+1];
                p1y=poly[j+2];
                k=j+3;

                if (poly[k]==END ||
                    poly[k]==NEW_LOOP)
                {
                    k=start;
                } 

                if (poly[i]!=SPLINE) 
                { 
                    Sx=poly[i];
                    Sy=poly[i+1]; 
                }
                else                 
                { 
                    Sx=(poly[i+1]+poly[j+1])/2.;
                    Sy=(poly[i+2]+poly[j+2])/2.; 
                }

                if (poly[k]!=SPLINE) 
                { 
                    Ex=poly[k];
                    Ey=poly[k+1]; 
                }
                else                 
                {
                    Ex=(poly[j+1]+poly[k+1])/2.0;
                    Ey=(poly[j+2]+poly[k+2])/2.0; 
                }

                // 2a = 2 * [ p0.y - 2p1.y + p2.y ]
                bottomPart=2.0*(Sy+Ey-p1y-p1y);

                // zero denomenator in sqrt
                if (bottomPart==0.0)
                { 
                    p1y+=.0001;
                    bottomPart=-.0004; 
                }

                sRoot=2.0*(p1y-Sy);     // b = [ 2(p1.y - p0.y) ]
                sRoot*=sRoot;           // b^2 
                sRoot-=2.0*bottomPart*(Sy-Y); // -4ac

                if (sRoot>=0.0) 
                {
                    sRoot=sqrt(sRoot);
                    topPart=2.0*(Sy-p1y);
                    // two solutions for plus and ,minus
                    for (plusOrMinus=-1.0; plusOrMinus<1.1; plusOrMinus+=2.) 
                    {
                        F = (topPart+plusOrMinus*sRoot)/bottomPart;
                        if (F>=0.0 && F<=1.0) 
                        {
                            xPart=Sx+F*(p1x-Sx);
                            nodeX[nodes++]=(int)(xPart+F*(p1x+F*(Ex-p1x)-xPart));
                        }
                    }
                }
            }
            else if (poly[j]==CUBIC) 
            {
                double p1x, p1y, p2x, p2y;

                //  CUBIC SPLINE
                p1x=poly[j+1]; p1y=poly[j+2];
                p2x=poly[j+3]; p2y=poly[j+4];
                k=j+5;
                if (poly[k]==END || poly[k]==NEW_LOOP) k=start;

                if (poly[i]!=SPLINE && poly[i]!=CUBIC) { Sx=poly[i];   Sy=poly[i+1]; }
                else                                    { Sx=(poly[i+1]+p1x)/2.; Sy=(poly[i+2]+p1y)/2.; }
                if (poly[k]!=SPLINE && poly[k]!=CUBIC) { Ex=poly[k];   Ey=poly[k+1]; }
                else                                    { Ex=(p2x+poly[k+1])/2.; Ey=(p2y+poly[k+2])/2.; }

                double ca = -Sy + 3*p1y - 3*p2y + Ey;
                double cb =  3*Sy - 6*p1y + 3*p2y;
                double cc = -3*Sy + 3*p1y;
                double cd =  Sy - Y;

                double guesses[3] = {0.1, 0.5, 0.9};
                double roots[3];
                int    num_roots=0;

                for (int g=0; g<3; g++) 
                {
                    double t=guesses[g];
                    for (int iter=0; iter<16; iter++) 
                    {
                        double ft  = ((ca*t + cb)*t + cc)*t + cd;
                        double dft =  (3*ca*t + 2*cb)*t + cc;
                        if (fabs(dft)<1e-10) break;
                        t = t - ft/dft;
                    }
                    if (t<0. || t>1.) continue;
                    if (fabs(((ca*t+cb)*t+cc)*t+cd)>1e-6) continue;
                    int dup=0;
                    for (int r=0; r<num_roots; r++) if (fabs(roots[r]-t)<1e-6) { dup=1; break; }
                    if (!dup) roots[num_roots++]=t;
                }

                for (int r=0; r<num_roots; r++) {
                    double t=roots[r], mt=1.-t;
                    nodeX[nodes++]=(int)(mt*mt*mt*Sx + 3*mt*mt*t*p1x + 3*mt*t*t*p2x + t*t*t*Ex);
                }
            }

            if (poly[i]==SPLINE) i++;
            if (poly[i]==CUBIC)  i+=3;
            i+=2;
            if (poly[i]==NEW_LOOP) { i++; start=i; }
        }

        //  Sort nodes via bubble sort
        i=0;
        while (i<nodes-1) 
        {
            if (nodeX[i]>nodeX[i+1]) 
            {
                swap=nodeX[i];
                nodeX[i]=nodeX[i+1];
                nodeX[i+1]=swap;
                if (i) i--;
            } 
            else 
            {
                i++;
            }
        }

        //  Fill between node pairs
        for (i=0; i<nodes; i+=2) 
        {
            if (nodeX[i]  >= IMAGE_RIGHT) 
            {
                break;
            }
            if (nodeX[i+1] > IMAGE_LEFT) 
            {
                if (nodeX[i] < IMAGE_LEFT)
                {
                    nodeX[i] = IMAGE_LEFT;
                }
                if (nodeX[i+1] > IMAGE_RIGHT)
                {
                    nodeX[i+1] = IMAGE_RIGHT;
                }
                for (int pixelX=nodeX[i]; pixelX<nodeX[i+1]; pixelX++) 
                {
                    draw_pixel(img, pixelX, pixelY, color);
                }
            }
        }
    }
}

void draw_path_filled(image_view_t *img, path_data_t *path, color4_t color)
{
    if (!path || arrlen(path->segments) == 0) return;

    // Max size estimate: each cubic needs 5 doubles (CUBIC + 4 coords),
    // each quad needs 3 (SPLINE + 2 coords), each line needs 2, plus NEW_LOOP and END
    int max_poly_size = arrlen(path->segments) * 8 + 32;
    double *poly = malloc(max_poly_size * sizeof(double));
    if (!poly) return;

    int pi = 0;
    bool first_in_loop = true;
    int loop_start = 0;

    for (int i = 0; i < arrlen(path->segments); i++)
    {
        bezier_segment_t *seg = &path->segments[i];

        // Detect start of new contour: degenerate moveto segment (p0==p2)
        bool is_moveto = seg->is_linear &&
                         seg->p0.x == seg->p2.x &&
                         seg->p0.y == seg->p2.y;

        if (is_moveto)
        {
            if (!first_in_loop && pi > 0)
            {
                poly[pi++] = NEW_LOOP;
            }
            loop_start = pi;
            first_in_loop = false;

            poly[pi++] = seg->p0.x;
            poly[pi++] = seg->p0.y;
            continue;
        }

        if (seg->is_linear)
        {
            // Straight line: just emit the endpoint
            poly[pi++] = seg->p2.x;
            poly[pi++] = seg->p2.y;
        }
        else if (!seg->is_cubic)
        {
            // Quadratic bezier
            poly[pi++] = SPLINE;
            poly[pi++] = seg->p1.x;
            poly[pi++] = seg->p1.y;
            poly[pi++] = seg->p2.x;
            poly[pi++] = seg->p2.y;
        }
        else
        {
            // Cubic bezier
            poly[pi++] = CUBIC;
            poly[pi++] = seg->p1.x;
            poly[pi++] = seg->p1.y;
            poly[pi++] = seg->p2.x;
            poly[pi++] = seg->p2.y;
            poly[pi++] = seg->p3.x;
            poly[pi++] = seg->p3.y;
        }
    }

    poly[pi++] = END;

    draw_spline_polygon_filled(img, poly,
                   0, img->width,
                   0, img->height, color);

    free(poly);
}

void export_image(image_view_t const *color_buf, const char *filename) 
{
    FILE *file = fopen(filename, "wb");

    if (!file) {
        fprintf(stderr, "Error : Failed to open file to export image\n");
        return;
    }

    uint8_t header[18] = {0};
    TGA_HEADER(header, color_buf->width, color_buf->height, 32);

    fwrite(header, sizeof(uint8_t), 18, file);

    for (size_t y = 0; y < color_buf->height; ++y) 
    {
        for (size_t x = 0; x < color_buf->width; ++x) 
        {
            color4_t pixel = BUF_AT(color_buf, x, y);
            uint8_t bgra[4] = {pixel.b, pixel.g, pixel.r, pixel.a };
            fwrite(bgra, sizeof(uint8_t), 4, file);
        }
    }
    fclose(file);
}

/* -------------------- Scissor stuff -------------------- */

// check whether a point is inside a rectangle
bool inside_rect(i32 x, i32 y, rect_t *s)
{
    bool check_x = Inside(x, s->x, (s->x + s->w));
    bool check_y = Inside(y, s->y, (s->y + s->h));
    return (check_x && check_y);
}

// Get the intersection rect of two rectangles
rect_t intersect_rects(const rect_t *a, const rect_t *b)
{
    i32 left = MAX(a->x, b->x);
    i32 top  = MAX(a->y, b->y);
    i32 right =  MIN(a->x+a->w, b->x+b->w);
    i32 bottom = MIN(a->y+a->h, b->y+b->h);
    
    rect_t result = {0};
    if (right > left &&
        bottom > top) 
    {
        result.x = left;
        result.y = top;
        result.w = (right - left);
        result.h = (bottom - top);
    }
    
    return result;
}

// Update the current effective scissor rectangle
// Combine nested scissor regions to create a final clipping area
void update_current_scissor(void)
{
    if (scissor_stack_size == 0) {
        scissor_enabled = false;
        return;
    }
    
    bool found_first = false;
    rect_t effective = {0};
    
    for (u32 i = 0; i < scissor_stack_size; i++) 
    {
        if (scissor_stack[i].enabled) 
        {
            rect_t current = (rect_t){scissor_stack[i].x,scissor_stack[i].y,
                                      scissor_stack[i].w, scissor_stack[i].h};
            if (!found_first) 
            {
                effective = current;
                found_first = true;
            } 
            else 
            {
                effective = intersect_rects(&effective, &current);
                // If intersection is empty, no pixels can be drawn
                if (effective.w == 0 || effective.h == 0) {
                    break;
                }
            }
        }
    }
    
    current_scissor = effective;
    scissor_enabled = found_first;
}

bool push_scissor(i32 x, i32 y, i32 width, i32 height)
{
    if (scissor_stack_size >= MAX_SCISSOR_STACK) {
        return false; 
    }
    
    scissor_region_t region = {0};
    region.x = x;
    region.y = y;
    region.w = width;
    region.h = height;
    region.enabled = true;
    
    scissor_stack[scissor_stack_size++] = region;
    update_current_scissor();
    
    return true;
}

bool pop_scissor(void)
{
    if (scissor_stack_size == 0) {
        return false; 
    }
    
    scissor_stack_size--;
    update_current_scissor();
    
    return true;
}

bool pixel_in_current_scissor(i32 x, i32 y)
{
    if (!scissor_enabled)
        return true;
    
    return inside_rect(x, y, &current_scissor);
}

bool rect_intersects_current_scissor(i32 x, i32 y, i32 width, i32 height)
{
    if (!scissor_enabled)
        return true;
    
    rect_t *s = &current_scissor;
    
    if (x >= (s->x + s->w)  ||
        y >= (s->y + s->h)  ||
        s->x >= (x + width) ||
        s->y >= (y + height))
    {
        return false;
    }
    
    return true;
}

void clip_rect_to_current_scissor(i32 *x, i32 *y, i32 *width, i32 *height)
{
    if (!scissor_enabled)
        return;
    
    rect_t *s = &current_scissor;
    rect_t d = {*x,*y,*width,*height};

    rect_t intersection = intersect_rects(&d,s);

    *x = intersection.x;
    *y = intersection.y;
    *width = intersection.w ? intersection.w : 0;
    *height = intersection.h ? intersection.h : 0;
}

void set_pixel_scissored(image_view_t const*img, i32 x, i32 y, color4_t color)
{
    if (!pixel_in_current_scissor(x, y)){
        return;
    }
    
    draw_pixel(img, x, y, color);
}

void draw_rect_scissored(image_view_t const *img, i32 x, i32 y, i32 width, i32 height, color4_t color)
{
    if (!rect_intersects_current_scissor(x, y, width, height))
        return;
    
    i32 clipped_x = x;
    i32 clipped_y = y;
    i32 clipped_width = width;
    i32 clipped_height = height;
    
    clip_rect_to_current_scissor(&clipped_x, &clipped_y, &clipped_width, &clipped_height);
    
    draw_rect_solid_wh(img, clipped_x, clipped_y, clipped_width, clipped_height, color);
}

void draw_rounded_rect_scissored(image_view_t *img, i32 x, i32 y, i32 width, i32 height, i32 rad, color4_t color)
{
    if (!rect_intersects_current_scissor(x, y, width, height))
        return;
    
    i32 clipped_x = x;
    i32 clipped_y = y;
    i32 clipped_width = width;
    i32 clipped_height = height;
    
    clip_rect_to_current_scissor(&clipped_x, &clipped_y, &clipped_width, &clipped_height);
    
    draw_rounded_rectangle_filled_aa_wh(img, clipped_x, clipped_y, clipped_width, clipped_height, rad, color);
}

void clear_scissor_stack(void)
{
    scissor_stack_size = 0;
    scissor_enabled = false;
}

/* -------------------- Radial stuff -------------------- */

void polar_to_cartesian(polar_t polar, f32 center_x, f32 center_y, 
                        f32 scale_x, f32 scale_y, f32 *out_x, f32 *out_y) 
{
    *out_x = center_x + (polar.radius * cosf(polar.angle)) * scale_x;
    *out_y = center_y + (polar.radius * sinf(polar.angle)) * scale_y;
}

void cartesian_to_polar(f32 x, f32 y,
                        f32 center_x, f32 center_y,
                        f32 scale_x, f32 scale_y,
                        polar_t *out_polar)
{
    f32 dx = (x - center_x) / scale_x;
    f32 dy = (y - center_y) / scale_y;

    out_polar->radius = sqrtf(dx*dx + dy*dy);
    out_polar->angle  = atan2f(dy, dx);
}

f32 normalize_angle(f32 a)
{
    a = fmodf(a, 2.0f * M_PI);
    if (a < 0.0f)
        a += 2.0f * M_PI;
    return a;
}

bool point_in_radial_segment(f32 px, f32 py, radial_layout_t *layout, radial_segment_t *seg) 
{
    polar_t pt = {0};
    cartesian_to_polar(px,py,layout->center_x, layout->center_y,
                        layout->scale_x, layout->scale_y, &pt);

    pt.angle = normalize_angle(pt.angle);
    
    f32 seg_start = normalize_angle(seg->angle_start);
    f32 seg_end = normalize_angle(seg->angle_end);
    
    if (pt.radius < seg->inner_radius ||
        pt.radius > seg->outer_radius) 
    {
        return false;
    }
    
    bool in_angle = IN_RANGE_WRAP(pt.angle, seg_start, seg_end);
    
    return in_angle;
}

f32 radial_angle_step(i32 num_segments, f32 gap_factor) 
{
    f32 full_step = (2.0f * M_PI) / num_segments;
    return full_step * (1.0f - gap_factor * 0.5f);
}

radial_segment_t radial_get_segment(radial_layout_t *layout, i32 i, f32 height) 
{
    f32 full_angle = 2.0f * M_PI;
    f32 total_gap = full_angle * layout->gap_factor;
    f32 gap_per_segment = total_gap / layout->num_segments;
    
    f32 segment_angle = (full_angle - total_gap) / layout->num_segments;
    
    radial_segment_t seg;
    // Each segment: [segment][gap][segment][gap]...
    seg.angle_start = (i * (segment_angle + gap_per_segment)) + layout->rotation;
    seg.angle_end = seg.angle_start + segment_angle;
    seg.inner_radius = layout->inner_radius;
    seg.outer_radius = layout->inner_radius + height * (layout->outer_radius - layout->inner_radius);
    
    return seg;
}

void draw_radial_segment_filled(image_view_t *img,
                                radial_layout_t *layout, 
                                 radial_segment_t *seg,
                                 color4_t color) 
{
    #define ARC_STEPS 32
    #define MAX_VERTICES (ARC_STEPS * 2)
    
    i32 vx[MAX_VERTICES];
    i32 vy[MAX_VERTICES];
    
    f32 angle_range = seg->angle_end - seg->angle_start;
    f32 angle_step = angle_range / (ARC_STEPS - 1);
    
    i32 vertex_count = 0;
    
    for (i32 i = 0; i < ARC_STEPS; i++) 
    {
        f32 angle = seg->angle_start + i * angle_step;
        f32 x, y;
        polar_to_cartesian((polar_t){seg->outer_radius, angle}, 
                          layout->center_x, layout->center_y,
                          layout->scale_x, layout->scale_y, &x, &y);
        vx[vertex_count] = (i32)x;
        vy[vertex_count] = (i32)y;
        vertex_count++;
    }
    
    for (i32 i = ARC_STEPS - 1; i >= 0; i--) 
    {
        f32 angle = seg->angle_start + i * angle_step;
        f32 x, y;
        polar_to_cartesian((polar_t){seg->inner_radius, angle},
                          layout->center_x, layout->center_y,
                          layout->scale_x, layout->scale_y, &x, &y);
        vx[vertex_count] = (i32)x;
        vy[vertex_count] = (i32)y;
        vertex_count++;
    }
    
    draw_filled_polygon(img, vx, vy, vertex_count, color);
    
    #undef ARC_STEPS
    #undef MAX_VERTICES
}

/* -------------------- Spline stuff -------------------- */

/*
   ​Given four points p0, p1, p2, p3 and t ∈ [0,1]

   | −0.5     1.5    −1.5    0.5 |     | t^3 |
   |  1.0    −2.5     2.0   −0.5 |     | t^2 |
   | −0.5     0.0     0.5    0.0 |  .  | t   |
   |  0.0     1.0     0.0    0.0 |     | 1   |

    P(t)=0.5[(2p1​)
            +(−p0​+p2​)t
            +(2p0​−5p1​+4p2​−p3​)t^2
            +(−p0​+3p1​−3p2​+p3​)t^3]

    Produces C1 (no discontinuities in the tangent direction and magnitude) 
    continuous cubic curve that Passes through all of the control points

    Segments are define by two end control points p1 at t=0, p2 at t=1
    plus an additional control point on either side of the endpoints.
    Thus, to define S segments, S+3 control points are required.
 */
void catmull_rom_vertex(f32 t, vertex_t p[4], vertex_t *out)
{
    f32 t2 = t * t;
    f32 t3 = t2 * t;
    
    // Catmull-Rom basis matrix coefficients
    f32 c0 = -0.5f * t3 + t2 - 0.5f * t;
    f32 c1 = 1.5f * t3 - 2.5f * t2 + 1.0f;
    f32 c2 = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
    f32 c3 = 0.5f * t3 - 0.5f * t2;
    
    out->x = c0 * p[0].x + c1 * p[1].x + c2 * p[2].x + c3 * p[3].x;
    out->y = c0 * p[0].y + c1 * p[1].y + c2 * p[2].y + c3 * p[3].y;
    out->r = c0 * p[0].r + c1 * p[1].r + c2 * p[2].r + c3 * p[3].r;
    out->g = c0 * p[0].g + c1 * p[1].g + c2 * p[2].g + c3 * p[3].g;
    out->b = c0 * p[0].b + c1 * p[1].b + c2 * p[2].b + c3 * p[3].b;
}

/*
    Another way to calculate it 
 */
f32 catmull_rom_1d_eval(f32 f, f32 p0, f32 p1, f32 p2, f32 p3)
{
    f32 inv = 1.0f - f;

    f32 out;
    out = ((p3 * f + p2 * inv) * f +(p2 * f + p1 * inv) * inv) * f +
          ((p2 * f + p1 * inv) * f +(p1 * f + p0 * inv) * inv) * inv;
    return out;
}

path_data_t * path_new(void)
{
    path_data_t *path = calloc(1, sizeof(path_data_t));
    path->segments = NULL;
    path->selected_segment = -1;
    path->selected_point = -1;
    path->is_dragging = false;
    path->min_x = path->min_y =  1e9f;
    path->max_x = path->max_y = -1e9f;
    return path;
}

void add_bezier_segment(path_data_t *path, vec2f_t p0, vec2f_t p1, vec2f_t p2, vec2f_t p3, bool is_cubic, bool is_linear) 
{
    bezier_segment_t seg;
    seg.p0 = p0;
    seg.p1 = p1;
    seg.p2 = p2;
    seg.p3 = p3;
    seg.is_cubic = is_cubic;
    seg.is_linear = is_linear;

    arrput(path->segments, seg);
}

/*
    Reads every <path d="..."> element from a plain-text SVG.
*/
path_data_t *load_path_from_svg(const char *filename)
{
    unsigned char *buf = read_file(filename);   
    if (!buf) return NULL;

    path_data_t *path = path_new();         

    char *file = (char*)buf;
    char *search = file;

    while ((search = strstr(search, "d=\"")) != NULL)
    {
        search += 3;                    
        const char *end = strchr(search, '"');
        if (!end) break;

        /* copy path data into a null-terminated scratch buffer */
        size_t len = (size_t)(end - search);
        char  *file_path = malloc(len + 1);
        if (!file_path) break;
        memcpy(file_path, search, len);
        file_path[len] = '\0';

        /* --- interpret path commands ------------------------------ */
        const char *p = file_path;
        float cx = 0, cy = 0;           /* current pen position       */
        float sx = 0, sy = 0;           /* start of current subpath   */
        char  last_cmd = 0;

        while (*p)
        {
            skip_whitespace_and_commas(&p);
            if (!*p) break;

            char cmd = *p;
            int  is_alpha = isalpha((unsigned char)cmd);

            if (is_alpha) { last_cmd = cmd; p++; }
            else            cmd = last_cmd;     /* implicit repeat */

            float x, y, x1, y1, x2, y2;

            switch (cmd)
            {
                /* ---- moveto --------------------------------------- */
                case 'M':
                    if (!parse_two_floats(&p, &x, &y)) goto next_path;
                    cx = x; cy = y; sx = cx; sy = cy;
                    UPDATE_BB(path, cx, cy);
                
                    /* emit a degenerate moveto segment (p0 == p2) */
                    add_bezier_segment(path,
                        (vec2f_t){cx,cy}, (vec2f_t){cx,cy},
                        (vec2f_t){cx,cy}, (vec2f_t){0,0},
                        false, true);
                    /* subsequent coords are implicit lineto */
                    last_cmd = 'L';
                    break;

                case 'm':
                    if (!parse_two_floats(&p, &x, &y)) goto next_path;
                    cx += x; cy += y; sx = cx; sy = cy;
                    UPDATE_BB(path, cx, cy);
                    add_bezier_segment(path,
                        (vec2f_t){cx,cy}, (vec2f_t){cx,cy},
                        (vec2f_t){cx,cy}, (vec2f_t){0,0},
                        false, true);
                    last_cmd = 'l';
                    break;

                /* ---- lineto --------------------------------------- */
                case 'L':
                    if (!parse_two_floats(&p, &x, &y)) goto next_path;
                    {
                        vec2f_t p0 = {cx, cy};
                        cx = x; cy = y;
                        UPDATE_BB(path, cx, cy);
                        vec2f_t p1 = {(p0.x+cx)/2.f, (p0.y+cy)/2.f};
                        add_bezier_segment(path, p0, p1,
                            (vec2f_t){cx,cy}, (vec2f_t){0,0}, false, true);
                    }
                    break;

                case 'l':
                    if (!parse_two_floats(&p, &x, &y)) goto next_path;
                    {
                        vec2f_t p0 = {cx, cy};
                        cx += x; cy += y;
                        UPDATE_BB(path, cx, cy);
                        vec2f_t p1 = {(p0.x+cx)/2.f, (p0.y+cy)/2.f};
                        add_bezier_segment(path, p0, p1,
                            (vec2f_t){cx,cy}, (vec2f_t){0,0}, false, true);
                    }
                    break;

                /* ---- horizontal / vertical lineto ----------------- */
                case 'H':
                    if (!parse_float(&p, &x)) goto next_path;
                    {
                        vec2f_t p0 = {cx, cy};
                        cx = x;
                        UPDATE_BB(path, cx, cy);
                        vec2f_t p1 = {(p0.x+cx)/2.f, cy};
                        add_bezier_segment(path, p0, p1,
                            (vec2f_t){cx,cy}, (vec2f_t){0,0}, false, true);
                    }
                    break;

                case 'h':
                    if (!parse_float(&p, &x)) goto next_path;
                    {
                        vec2f_t p0 = {cx, cy};
                        cx += x;
                        UPDATE_BB(path, cx, cy);
                        vec2f_t p1 = {(p0.x+cx)/2.f, cy};
                        add_bezier_segment(path, p0, p1,
                            (vec2f_t){cx,cy}, (vec2f_t){0,0}, false, true);
                    }
                    break;

                case 'V':
                    if (!parse_float(&p, &y)) goto next_path;
                    {
                        vec2f_t p0 = {cx, cy};
                        cy = y;
                        UPDATE_BB(path, cx, cy);
                        vec2f_t p1 = {cx, (p0.y+cy)/2.f};
                        add_bezier_segment(path, p0, p1,
                            (vec2f_t){cx,cy}, (vec2f_t){0,0}, false, true);
                    }
                    break;

                case 'v':
                    if (!parse_float(&p, &y)) goto next_path;
                    {
                        vec2f_t p0 = {cx, cy};
                        cy += y;
                        UPDATE_BB(path, cx, cy);
                        vec2f_t p1 = {cx, (p0.y+cy)/2.f};
                        add_bezier_segment(path, p0, p1,
                            (vec2f_t){cx,cy}, (vec2f_t){0,0}, false, true);
                    }
                    break;

                /* ---- cubic bezier --------------------------------- */
                case 'C':
                    if (!parse_two_floats(&p, &x1, &y1)) goto next_path;
                    if (!parse_two_floats(&p, &x2, &y2)) goto next_path;
                    if (!parse_two_floats(&p, &x,  &y )) goto next_path;
                    {
                        vec2f_t p0 = {cx, cy};
                        UPDATE_BB(path, x1,y1); UPDATE_BB(path, x2,y2); UPDATE_BB(path, x,y);
                        cx = x; cy = y;
                        add_bezier_segment(path, p0,
                            (vec2f_t){x1,y1}, (vec2f_t){x2,y2},
                            (vec2f_t){cx,cy}, true, false);
                    }
                    break;

                case 'c':
                    if (!parse_two_floats(&p, &x1, &y1)) goto next_path;
                    if (!parse_two_floats(&p, &x2, &y2)) goto next_path;
                    if (!parse_two_floats(&p, &x,  &y )) goto next_path;
                    {
                        vec2f_t p0 = {cx, cy};
                        x1+=cx; y1+=cy; x2+=cx; y2+=cy; x+=cx; y+=cy;
                        UPDATE_BB(path, x1,y1); UPDATE_BB(path, x2,y2); UPDATE_BB(path, x,y);
                        cx = x; cy = y;
                        add_bezier_segment(path, p0,
                            (vec2f_t){x1,y1}, (vec2f_t){x2,y2},
                            (vec2f_t){cx,cy}, true, false);
                    }
                    break;

                /* ---- quadratic bezier ----------------------------- */
                case 'Q':
                    if (!parse_two_floats(&p, &x1, &y1)) goto next_path;
                    if (!parse_two_floats(&p, &x,  &y )) goto next_path;
                    {
                        vec2f_t p0 = {cx, cy};
                        UPDATE_BB(path, x1,y1); UPDATE_BB(path, x,y);
                        cx = x; cy = y;
                        add_bezier_segment(path, p0,
                            (vec2f_t){x1,y1}, (vec2f_t){cx,cy},
                            (vec2f_t){0,0}, false, false);
                    }
                    break;

                case 'q':
                    if (!parse_two_floats(&p, &x1, &y1)) goto next_path;
                    if (!parse_two_floats(&p, &x,  &y )) goto next_path;
                    {
                        vec2f_t p0 = {cx, cy};
                        x1+=cx; y1+=cy; x+=cx; y+=cy;
                        UPDATE_BB(path, x1,y1); UPDATE_BB(path, x,y);
                        cx = x; cy = y;
                        add_bezier_segment(path, p0,
                            (vec2f_t){x1,y1}, (vec2f_t){cx,cy},
                            (vec2f_t){0,0}, false, false);
                    }
                    break;

                /* ---- closepath ------------------------------------ */
                case 'Z':
                case 'z':
                    if (cx != sx || cy != sy)
                    {
                        vec2f_t p0 = {cx, cy};
                        cx = sx; cy = sy;
                        vec2f_t p1 = {(p0.x+cx)/2.f, (p0.y+cy)/2.f};
                        add_bezier_segment(path, p0, p1,
                            (vec2f_t){cx,cy}, (vec2f_t){0,0}, false, true);
                    }
                    last_cmd = 0;
                    break;

                default:
                    p++; /* skip unknown character */
                    break;
            }
        } /* while path commands */

        next_path:
        free(file_path);
        search = end + 1;
    }

    free(buf);

    if (arrlen(path->segments) == 0) {
        printf("[SVG] No segments parsed from '%s'\n", filename);
        arrfree(path->segments);
        free(path);
        return NULL;
    }

    /* SVG Y-axis is flipped vs. the path coordinate system used here.
       Flip all Y coordinates so it renders upright with path_to_screen. */
    float y_center = (path->min_y + path->max_y) / 2.0f;
    for (int i = 0; i < arrlen(path->segments); i++)
    {
        bezier_segment_t *s = &path->segments[i];
        s->p0.y = 2.f*y_center - s->p0.y;
        s->p1.y = 2.f*y_center - s->p1.y;
        s->p2.y = 2.f*y_center - s->p2.y;
        s->p3.y = 2.f*y_center - s->p3.y;
    }
    /* bounding box stays the same after flip */

    printf("[SVG] Loaded '%s': %d segments  BB:(%.1f,%.1f)-(%.1f,%.1f)\n",
           filename,
           arrlen(path->segments),
           path->min_x, path->min_y,
           path->max_x, path->max_y);

    return path;
}

/* -------------------- Noise stuff -------------------- */

/* 
    ---Interleaved Gradient Noise---

    f32 noise = gradient_noise((f32)x, (f32)y);
    vec3f_t noise_offset = {
        (1.0f / 255.0f) * noise - (0.5f / 255.0f),
        (1.0f / 255.0f) * noise - (0.5f / 255.0f),
        (1.0f / 255.0f) * noise - (0.5f / 255.0f)
    };
    color = vec3f_add(color, noise_offset);
 */
f32 gradient_noise(f32 x, f32 y) 
{
    const f32 magic_x = 0.06711056f;
    const f32 magic_y = 0.00583715f;
    const f32 magic_z = 52.9829189f;
    
    f32 dot_product = x * magic_x + y * magic_y;
    f32 fract_dot = dot_product - floorf(dot_product);
    f32 result = magic_z * fract_dot;
    return result - floorf(result);
}

/*
    Simple Perlin Noise function
 */
f32 f_noise2f(f32 x, f32 y)
{
	f32 a, b, c, d;
	int xi, yi;
	x += 256.0;
	xi = (int)x;
	x -= (f32)xi;
	y += 4096.0;
	yi = (int)y;
	y -= (f32)yi;
	yi *= 11;
	x = (3.0 * x * x - 2.0 * x * x * x);
	y = (3.0 * y * y - 2.0 * y * y * y);

	a = f_randnf((u32)xi + yi);
	b = f_randnf((u32)xi + yi + 1);
	c = a + (b - a) * x;

	a = f_randnf((u32)xi + yi + 11);
	b = f_randnf((u32)xi + yi + 12);
	d = a + (b - a) * x;

	return c + (d - c) * y;
}

/*
    Tiled version for stuff that needs to be connected
 */
f32 f_noiset2f(f32 x, f32 y, i32 period)
{
	f32 a, b, c, d;
	i32 xi, yi, xin, yin;
	x += 256.0;
	xi = (i32)x;
	x -= (f32)xi;
	y += 4096.0;
	yi = (i32)y;
	y -= (f32)yi;
	x = (3.0 * x * x - 2.0 * x * x * x);
	y = (3.0 * y * y - 2.0 * y * y * y);
	xi = xi % period;
	xin = (xi + 1) % period;
	yin = ((yi + 1) % period) * 11;
	yi = (yi % period) * 11;
	a = f_randnf(xi + yi);
	b = f_randnf(xin + yi);
	c = a + (b - a) * x;
	a = f_randnf(xi + yin);
	b = f_randnf(xin + yin);
	d = a + (b - a) * x;

	return c + (d - c) * y;
}

static int const noise_perm[512] = 
{
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
    190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,
    125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,
    105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,200,196,
    135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,
    82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,
    153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,
    106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,
    78,66,215,61,156,180,
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
    190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,
    125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,
    105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,200,196,
    135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,
    82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,
    153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,
    106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,
    78,66,215,61,156,180
};

// Ken Perlin's improved fade curve
// smoothstep quintic curve 6t⁵ - 15t⁴ + 10t³, gives C2-continuous output
// if the second derivative of the smoothstep function has discontinuities 
// which would lead to nasty artificats this supposedly fixes the problem
static inline f32 noise_fade(f32 t) 
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static inline f32 noise_grad2d(int hash, f32 x, f32 y) 
{
    int h = hash & 7;
    f32 u = h < 4 ? x : y;
    f32 v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

static inline f32 noise_grad2d_simplex(int hash, f32 x, f32 y) 
{
    static f32 const grad2[][2] = {
        {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
        { 0.7071067811865476f,  0.7071067811865476f},
        {-0.7071067811865476f,  0.7071067811865476f},
        { 0.7071067811865476f, -0.7071067811865476f},
        {-0.7071067811865476f, -0.7071067811865476f}
    };
    int h = hash & 7;
    return grad2[h][0] * x + grad2[h][1] * y;
}

static inline f32 noise_grad3d(int hash, f32 x, f32 y, f32 z) 
{
    int h = hash & 15;
    f32 u = h < 8 ? x : y;
    f32 v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

f32 perlin_2d(f32 x, f32 y, int seed) 
{
    x += seed * 12.9898f;
    y += seed * 78.233f;

    int X = (int)floorf(x) & 255;
    int Y = (int)floorf(y) & 255;

    x -= floorf(x);
    y -= floorf(y);

    f32 u = noise_fade(x);
    f32 v = noise_fade(y);

    int A = noise_perm[X]     + Y;
    int B = noise_perm[X + 1] + Y;

    f32 g00 = noise_grad2d(noise_perm[A],     x,        y);
    f32 g10 = noise_grad2d(noise_perm[B],     x - 1.0f, y);
    f32 g01 = noise_grad2d(noise_perm[A + 1], x,        y - 1.0f);
    f32 g11 = noise_grad2d(noise_perm[B + 1], x - 1.0f, y - 1.0f);

    return BILERP(g00, g10, g01, g11, u, v) * 0.5f;
}


f32 perlin_3d(f32 x, f32 y, f32 z, int seed) 
{
    x += seed * 12.9898f;
    y += seed * 78.233f;
    z += seed * 37.719f;

    int X = (int)floorf(x) & 255;
    int Y = (int)floorf(y) & 255;
    int Z = (int)floorf(z) & 255;

    x -= floorf(x);
    y -= floorf(y);
    z -= floorf(z);

    f32 u = noise_fade(x);
    f32 v = noise_fade(y);
    f32 w = noise_fade(z);

    int A  = noise_perm[X]     + Y;
    int AA = noise_perm[A]     + Z;
    int AB = noise_perm[A + 1] + Z;
    int B  = noise_perm[X + 1] + Y;
    int BA = noise_perm[B]     + Z;
    int BB = noise_perm[B + 1] + Z;

    f32 g000 = noise_grad3d(noise_perm[AA],     x,        y,        z);
    f32 g100 = noise_grad3d(noise_perm[BA],     x - 1.0f, y,        z);
    f32 g010 = noise_grad3d(noise_perm[AB],     x,        y - 1.0f, z);
    f32 g110 = noise_grad3d(noise_perm[BB],     x - 1.0f, y - 1.0f, z);
    f32 g001 = noise_grad3d(noise_perm[AA + 1], x,        y,        z - 1.0f);
    f32 g101 = noise_grad3d(noise_perm[BA + 1], x - 1.0f, y,        z - 1.0f);
    f32 g011 = noise_grad3d(noise_perm[AB + 1], x,        y - 1.0f, z - 1.0f);
    f32 g111 = noise_grad3d(noise_perm[BB + 1], x - 1.0f, y - 1.0f, z - 1.0f);

    return TRILERP(g000, g100, g010, g110, g001, g101, g011, g111, u, v, w) * 0.5f;
}


f32 simplex_2d(f32 x, f32 y, int seed) 
{
    static f32 const F2 = 0.366025403f;
    static f32 const G2 = 0.211324865f;

    x += seed * 12.9898f;
    y += seed * 78.233f;

    f32 s = (x + y) * F2;
    int i = (int)floorf(x + s);
    int j = (int)floorf(y + s);

    f32 t  = (i + j) * G2;
    f32 x0 = x - (i - t);
    f32 y0 = y - (j - t);

    int i1 = (x0 > y0) ? 1 : 0;
    int j1 = (x0 > y0) ? 0 : 1;

    f32 x1 = x0 - i1 + G2;
    f32 y1 = y0 - j1 + G2;
    f32 x2 = x0 - 1.0f + 2.0f * G2;
    f32 y2 = y0 - 1.0f + 2.0f * G2;

    int ii = i & 255;
    int jj = j & 255;

    f32 n0, n1, n2;

    f32 t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 < 0.0f) { n0 = 0.0f; }
    else { t0 *= t0; n0 = t0 * t0 * noise_grad2d_simplex(noise_perm[ii + noise_perm[jj]], x0, y0); }

    f32 t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 < 0.0f) { n1 = 0.0f; }
    else { t1 *= t1; n1 = t1 * t1 * noise_grad2d_simplex(noise_perm[ii + i1 + noise_perm[jj + j1]], x1, y1); }

    f32 t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 < 0.0f) { n2 = 0.0f; }
    else { t2 *= t2; n2 = t2 * t2 * noise_grad2d_simplex(noise_perm[ii + 1 + noise_perm[jj + 1]], x2, y2); }

    return 45.23065f * (n0 + n1 + n2);
}


f32 worley_2d(f32 x, f32 y, int seed) 
{
    x += seed * 12.9898f;
    y += seed * 78.233f;

    int xi = (int)floorf(x);
    int yi = (int)floorf(y);

    f32 min_dist = 1e10f;

    for (int dy = -1; dy <= 1; dy++) 
    {
        for (int dx = -1; dx <= 1; dx++) 
        {
            int cx = xi + dx;
            int cy = yi + dy;

            int h  = noise_perm[(cx & 255) + noise_perm[(cy & 255)]];
            f32 px = (f32)cx + (noise_perm[h] / 255.0f);
            f32 py = (f32)cy + (noise_perm[(h + 1) & 255] / 255.0f);

            f32 ddx  = x - px;
            f32 ddy  = y - py;
            f32 dist = sqrtf(ddx * ddx + ddy * ddy);

            if (dist < min_dist) min_dist = dist;
        }
    }

    return min_dist * 1.4f - 1.0f;
}

f32 vhash(int px, int py, int seed)
{
    int _perm1 = (px + seed) & 255; 
    int _perm2 = (noise_perm[_perm1] + py) & 255; 
    int _perm3 = noise_perm[_perm2];

    return RANGE_CONVERT((f32)_perm3, \
                  0.0f, 255.0f, \
                  -1.0f, 1.0f);
}

f32 value_noise_2d(f32 x, f32 y, int seed) 
{
    int xi = (int)floorf(x);
    int yi = (int)floorf(y);

    f32 xf = x - (f32)xi;
    f32 yf = y - (f32)yi;

    xi = xi & 255;
    yi = yi & 255;

    f32 u = noise_fade(xf);
    f32 v = noise_fade(yf);

    // hash the corners and use lerp to get values between
    f32 n00 = vhash(xi,     yi    , seed);
    f32 n10 = vhash(xi + 1, yi    , seed);
    f32 n01 = vhash(xi,     yi + 1, seed);
    f32 n11 = vhash(xi + 1, yi + 1, seed);

    // billinear interpolation
    return BILERP(n00, n10, n01, n11, u, v);
}

/* -------------------- Motion stuff -------------------- */

f32 f_wigglef(f32 f, f32 size)
{
	float v0, v1, v2, v3;
	u32 seed;
	seed = (float)f;
	f -= (float)seed;
	seed *= 2;
	size *= 2.0;
	v0 = (f_randf(seed++) - 0.5) * size;
	v1 = (f_randf(seed++) - 0.5) * size;
	v3 = (f_randf(seed++) - 0.5) * size;
	v2 = v3 * 2.0 - (f_randf(seed++) - 0.5) * size;
	return catmull_rom_1d_eval(f, v0, v1, v2, v3);
}

// Simulates a damped spring system settling from 0 to 1.
// Parameters:
//   u    - time input [0, inf)
//   mass - mass of the spring (clamped to 1 if <= 0)
//   k    - stiffness (higher = faster oscillation)
//   c    - damping coefficient
//   v0   - initial velocity
// Returns a value that starts at 0 and settles toward 1.
// Behavior depends on the damping ratio (zeta):
//   zeta < 1 : underdamped  — overshoots and oscillates before settling
//   zeta = 1 : critically damped — fastest settle with no overshoot
//   zeta > 1 : overdamped   — slow creep to 1, no oscillation
static inline f32 spring_unit(f32 u, f32 mass, f32 k, f32 c, f32 v0) 
{
    f32 m    = (mass <= 0.0f ? 1.0f : mass);
    f32 wn   = sqrtf(k / m);
    f32 zeta = c / (2.0f * sqrtf(k * m));
    f32 t    = u;

    if (zeta < 1.0f) {
        // Underdamped: oscillates around 1 with decaying amplitude
        f32 wdn = wn * sqrtf(1.0f - zeta * zeta);
        f32 A   = 1.0f;
        f32 B   = (zeta * wn * A + v0) / wdn;
        f32 e   = expf(-zeta * wn * t);
        return 1.0f - e * (A * cosf(wdn * t) + B * sinf(wdn * t));
    } else if (zeta == 1.0f) {
        // Critically damped: settles fastest without overshooting
        f32 e = expf(-wn * t);
        return 1.0f - e * (1.0f + wn * t);
    } else {
        // Overdamped: two decaying exponentials, sluggish approach to 1
        f32 wd = wn * sqrtf(zeta * zeta - 1.0f);
        f32 e1 = expf(-(zeta * wn - wd) * t);
        f32 e2 = expf(-(zeta * wn + wd) * t);
        return 1.0f - 0.5f * (e1 + e2);
    }
}

typedef enum {
    WAVE_SINE     = 0,
    WAVE_TRIANGLE = 1,
    WAVE_SAWTOOTH = 2,
    WAVE_SQUARE   = 3,
} wave_type_t;

// Evaluates a single period of a waveform at normalized time t.
// t is wrapped to [0, 1) so any value is valid input.
// All wave types output in [-1, 1].
//   SINE      — smooth sinusoidal
//   TRIANGLE  — linear ramp up then down, pointy peaks
//   SAWTOOTH  — linear ramp from -1 to 1, hard reset
//   SQUARE    — instant flip between +1 and -1 at midpoint
static inline f32 eval_wave(wave_type_t wave_type, f32 t) 
{
    t = t - floorf(t); // wrap to [0, 1)
    switch (wave_type) {
        case WAVE_SINE:
            return sinf(t * 2.0f * M_PI);
        case WAVE_TRIANGLE:
            return (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
        case WAVE_SAWTOOTH:
            return 2.0f * t - 1.0f;
        case WAVE_SQUARE:
            return (t < 0.5f) ? 1.0f : -1.0f;
        default:
            return sinf(t * 2.0f * M_PI);
    }
}

typedef struct {
    f32 time;
    u32 last_frame;
} osc_state_t;

//   state     - persistent oscillator state owned by the caller
//   amplitude - peak output value, output range is [-amplitude, amplitude]
//   frequency - cycles per second
//   wave_type - shape of the waveform (see wave_type_t)
//   phase     - time offset in [0, 1], shifts the wave without touching state
//   dt        - delta time in seconds for this frame
f32 oscillate(osc_state_t* state, f32 amplitude, f32 frequency,
              wave_type_t wave_type, f32 phase, f32 dt, u32 current_frame) 
{
    if (state->last_frame != current_frame) {
        state->time      += dt;
        state->last_frame = current_frame;
    }
    f32 t = state->time * frequency + phase;
    return amplitude * eval_wave(wave_type, t);
}

/* -------------------- Texture stuff -------------------- */

image_view_t* load_texture(const char *filepath) 
{
    image_view_t *texture = malloc(sizeof(*texture));
    assert(texture);
    
    int width, height, channels;
    unsigned char *img_data = stbi_load(filepath, &width, &height, &channels, 4); // Force RGBA
    
    if (!img_data) {
        fprintf(stderr, "Failed to load texture: %s\n", filepath);
        free(texture);
        return NULL;
    }
    
    texture->width = (u32)width;
    texture->height = (u32)height;
    
    texture->pixels = malloc(width * height * sizeof(color4_t));
    assert(texture->pixels);
    
    for (int i = 0; i < width * height; i++) {
        texture->pixels[i].r = img_data[i * 4 + 0];
        texture->pixels[i].g = img_data[i * 4 + 1];
        texture->pixels[i].b = img_data[i * 4 + 2];
        texture->pixels[i].a = img_data[i * 4 + 3];
    }
    
    stbi_image_free(img_data);
    return texture;
}

void free_texture(image_view_t *texture) 
{
    if (texture) {
        if (texture->pixels) {
            free(texture->pixels);
        }
        free(texture);
    }
}

void render_texture_to_buffer(image_view_t const *color_buf, image_view_t const *texture,  
                               u32 dst_x, u32 dst_y, f32 scale, rect_t *out_rect) 
{
    u32 dst_w = texture->width * scale;
    u32 dst_h = texture->height * scale;
    
    if (dst_x >= color_buf->width ||
        dst_y >= color_buf->height) 
    {
        return;
    }
    
    if ((i32)dst_x + (i32)dst_w <= 0 || 
        (i32)dst_y + (i32)dst_h <= 0) 
    {
        return;
    }
    
    u32 x_start = MAX(0, dst_x);
    u32 y_start = MAX(0, dst_y);
    u32 x_end = MIN(color_buf->width, dst_x + dst_w);
    u32 y_end = MIN(color_buf->height, dst_y + dst_h);
    
    f32 x_scale = (f32)texture->width / dst_w;
    f32 y_scale = (f32)texture->height / dst_h;

    if(out_rect)
    {
        out_rect->x = x_start;
        out_rect->y = y_start;
        out_rect->w = x_end - x_start;
        out_rect->h = y_end - y_start;
    }

    for (u32 y = y_start; y < y_end; y++) 
    {
        for (u32 x = x_start; x < x_end; x++) 
        {
            u32 rel_x = x - dst_x;
            u32 rel_y = y - dst_y;
            
            u32 src_x = (u32)((f32)rel_x * x_scale);
            u32 src_y = (u32)((f32)rel_y * y_scale);
            
            if (src_x < texture->width && src_y < texture->height) 
            {
                color4_t src_pixel = texture->pixels[src_y * texture->width + src_x];
                
                if (src_pixel.a > 0) { 
                    BUF_AT(color_buf, x, y) = src_pixel;
                }
            }
        }
    }
}
