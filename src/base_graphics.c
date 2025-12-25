#include "base_graphics.h"
#include "util.h"

scissor_region_t scissor_stack[MAX_SCISSOR_STACK];
u32 scissor_stack_size;
rect_t current_scissor;
bool scissor_enabled;

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

vec3f_t linear_to_gamma(vec3f_t color)
{
    vec3f_t col = {0.0f,0.0f,0.0f};

    if(color.x > 0){
        col.x = sqrt_f32(color.x);
    } 
    if(color.y > 0){
        col.y = sqrt_f32(color.y);
    }
    if(color.z > 0){
        col.z = sqrt_f32(color.z);
    }

    return col;
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
    Each pixel contains some color information
    consisting of three values: red, green and blue 
    and can also contain information about transparency with an
    additional alpha channel indicating the opacity of each pixel
 */
inline void set_pixel(image_view_t const *img, i32 x, i32 y, color4_t color) 
{
    if (x >= 0 && x < (i32)img->width &&
        y >= 0 && y < (i32)img->height) 
    {
        BUF_AT(img, x, y) = color;
    }
}

inline color4_t get_pixel(image_view_t const *img, i32 x, i32 y) 
{
    color4_t color = {0, 0, 0, 0};

    if (x >= 0 && x < (i32)img->width &&
        y >= 0 && y < (i32)img->height) 
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
        for (u32 x = 0; x < color_buf->width; ++x) 
        {
            f32 dx = (f32)x - center_x;
            f32 dy = (f32)y - center_y;
            f32 dist_sq = dx * dx + dy * dy;
            
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
    if(x < 0 || x >= (i32)color_buf->width)
    {
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
        i32 j, x1 = x0 + w -1;
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

void draw_circle_filled_aa(image_view_t *img, i32 cx, i32 cy, f32 radius, color4_t color)
{
    if (radius <= 0) return;

    f32 radiusX = radius;
    f32 radiusY = radius;
    f32 radiusX2 = radiusX * radiusX;
    f32 radiusY2 = radiusY * radiusY;
    
    i32 quarter = (i32)roundf(radiusX2 / d_sqrt(radiusX2 + radiusY2));
    for (i32 x = 0; x <= quarter; x++)
    {
        f32 y = radiusY * d_sqrt(1.0f - (f32)(x * x) / radiusX2);
        f32 error = y - floorf(y);
        
        u8 transparency = (u8)(error * 255.0f);
        u8 transparency2 = (u8)((1.0f - error) * 255.0f);
        
        i32 y_floor = (i32)floorf(y);
        
        draw_hline(img, cy + y_floor, cx - x, cx + x, color);
        draw_hline(img, cy - y_floor, cx - x, cx + x, color);
        
        set_pixel_weighted(img, cx + x, cy + y_floor + 1, color, transparency);
        set_pixel_weighted(img, cx - x, cy + y_floor + 1, color, transparency);
        set_pixel_weighted(img, cx + x, cy - y_floor - 1, color, transparency);
        set_pixel_weighted(img, cx - x, cy - y_floor - 1, color, transparency);
    }
    
    quarter = (i32)roundf(radiusY2 / d_sqrt(radiusX2 + radiusY2));

    for (i32 y = 0; y <= quarter; y++)
    {
        f32 x = radiusX * d_sqrt(1.0f - (f32)(y * y) / radiusY2);
        f32 error = x - floorf(x);
        
        u8 transparency = (u8)(error * 255.0f);
        u8 transparency2 = (u8)((1.0f - error) * 255.0f);
        
        i32 x_floor = (i32)floorf(x);
        
        draw_hline(img, cy + y, cx - x_floor, cx + x_floor, color);
        draw_hline(img, cy - y, cx - x_floor, cx + x_floor, color);
        
        set_pixel_weighted(img, cx + x_floor + 1, cy + y, color, transparency);
        set_pixel_weighted(img, cx - x_floor - 1, cy + y, color, transparency);
        set_pixel_weighted(img, cx + x_floor + 1, cy - y, color, transparency);
        set_pixel_weighted(img, cx - x_floor - 1, cy - y, color, transparency);
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

void draw_filled_polygon(image_view_t *img, const i32 *vx, const i32 *vy, i32 n, color4_t color)
{
    if (vx == NULL || vy == NULL || n < 3) {
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
        for (i32 i = 0; i < n; i++) {
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
            if (y1 < y2) {
                x1 = vx[i1];
                x2 = vx[i2];
            } else {
                // Swap so y1 < y2
                i32 temp_y = y1;
                y1 = y2;
                y2 = temp_y;
                x1 = vx[i2];
                x2 = vx[i1];
            }
            
            // Check if scanline intersects this edge
            if (y >= y1 && y < y2) {
                // Calculate intersection point using fixed-point math
                i32 intersection = ((y - y1) * (x2 - x1) * 256) / (y2 - y1) + x1 * 256;
                intersections[num_intersections++] = intersection;
            }
        }

        // Sort intersections left to right
        for (i32 i = 0; i < num_intersections - 1; i++) {
            for (i32 j = i + 1; j < num_intersections; j++) {
                if (intersections[i] > intersections[j]) {
                    i32 temp = intersections[i];
                    intersections[i] = intersections[j];
                    intersections[j] = temp;
                }
            }
        }

        // Draw horizontal spans between pairs of intersections
        for (i32 i = 0; i < num_intersections; i += 2) {
            if (i + 1 < num_intersections) {
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

void export_image(image_view_t const *color_buf, const char *filename) 
{
    FILE *file = fopen(filename, "wb");

    if (!file) {
        fprintf(stderr, "Error : Failed to open file to export image");
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

// check whether a point is inside a rectangle
bool inside_rect(i32 x, i32 y, rect_t *s)
{
    bool check_x = (x >= s->x) && (x < (s->x + s->w));
    bool check_y = (y >= s->y) && (y < (s->y + s->h));
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
    if (right > left && bottom > top) 
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
            rect_t current = (rect_t){scissor_stack[i].x,scissor_stack[i].y, scissor_stack[i].w, scissor_stack[i].h};
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

void catmull_rom_point(f32 t, vertex_t p[4], vertex_t *out)
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
