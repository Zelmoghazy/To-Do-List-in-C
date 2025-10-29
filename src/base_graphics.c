#include "base_graphics.h"
#include "GLFW/glfw3.h"
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

/* 
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

color4_t color4_lerp(color4_t c1, color4_t c2, f32 t)
{
    t = Clamp(0.0f, t, 1.0f);
    
    color4_t result = {
        .r = (u8)(c1.r + t * (c2.r - c1.r)),
        .g = (u8)(c1.g + t * (c2.g - c1.g)),
        .b = (u8)(c1.b + t * (c2.b - c1.b)),
        .a = (u8)(c1.a + t * (c2.a - c1.a))
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
        .r = (src.r * src.a + dst.r * inv_alpha) / 255,
        .g = (src.g * src.a + dst.g * inv_alpha) / 255,
        .b = (src.b * src.a + dst.b * inv_alpha) / 255,
        .a = src.a + (dst.a * inv_alpha) / 255
    };

    return result;
}

/* Get existing color in the frame buffer and blend it witht the new color */
inline void set_pixel_blend(image_view_t const *img, i32 x, i32 y, color4_t color) 
{
    if (x >= 0 && x < (i32)img->width &&
        y >= 0 && y < (i32)img->height) 
    {
        if (IS_OPAQUE(color)) 
        {
            BUF_AT(img, x, y) = color;
        } 
        else 
        {
            color4_t dst = get_pixel(img, x, y);
            set_pixel(img,x,y,blend_pixel(dst, color));
        }
    }
}

// Used for anti-aliasing stuff
inline void set_pixel_weighted(image_view_t *img, i32 x, i32 y, color4_t color, i8 weight) 
{
    color4_t weighted = color;
    weighted.a = (color.a * weight) / 255;
    set_pixel_blend(img, x, y, weighted);
}

void draw_pixel(image_view_t *img, i32 x, i32 y, color4_t color) 
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
    
    for (u32 y = 0; y < color_buf->height; ++y) {
        for (u32 x = 0; x < color_buf->width; ++x) 
        {
            f32 dx = (f32)x - center_x;
            f32 dy = (f32)y - center_y;
            f32 dist_sq = dx * dx + dy * dy;
            
            f32 t = Clamp(0.0f, dist_sq / max_dist_sq, 1.0f);
            
            u8 r = (u8)LERP_F32(color0.r, color1.r, t);
            u8 g = (u8)LERP_F32(color0.g, color1.g, t);
            u8 b = (u8)LERP_F32(color0.b, color1.b, t);
            
            color4_t pixel = {r, g, b, 255};
            BUF_AT(color_buf, x, y) = pixel;
        }
    }
}

void draw_hline(image_view_t const *color_buf, i32 y, i32 x0, i32 x1, color4_t const color)
{
    if (y < 0 || y >= (i32)color_buf->height){
        return;
    }

    // Ensure x0 <= x1
    if (x0 > x1) {
        SWAP(x0, x1, i32);
    }

    x0 = MAX(0, x0);
    x1 = MIN((i32)color_buf->width - 1, x1);
    
    for (i32 x = x0; x <= x1; x++) {
        set_pixel_blend(color_buf, x, y, color);
    }
}

void draw_vline(image_view_t const *color_buf, i32 x, i32 y0, i32 y1, color4_t const color)
{
    if(x < 0 || x >= (i32)color_buf->width)
    {
        return;
    }

    // Ensure y0 <= y1
    if (y0 > y1) {
        SWAP(y0, y1, i32);
    }

    y0 = MAX(0, y0);
    y1 = MIN((i32)color_buf->height - 1, y1);
    
    for (i32 y = y0; y <= y1; y++) {
        set_pixel_blend(color_buf, x, y, color);
    }
}

void draw_line(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color)
{
    bool steep = false;
    
    // If the line is steep, we transpose the image
    if (abs(x0 - x1) < abs(y0 - y1)) {
        SWAP(x0, y0, i32);
        SWAP(x1, y1, i32);
        steep = true;
    }
    
    // Make it left-to-right
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
            set_pixel_blend(color_buf, y, x, color);
        } 
        else 
        {
            set_pixel_blend(color_buf, x, y, color);
        }
        
        error2 += derror2;
        if (error2 > dx) {
            y += (y1 > y0 ? 1 : -1);
            error2 -= dx * 2;
        }
    }
}

void draw_aaline(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, color4_t color) 
{
    i32 xx0, yy0, xx1, yy1;
    u32 erracc, erradj;
    u32 erracctmp, wgt;
    i32 dx, dy, tmp, xdir;
    
    // Handle special cases
    dx = x2 - x1;
    dy = y2 - y1;
    
    if (dx == 0) {
        draw_vline(img, x1, y1, y2, color);
        return;
    }
    if (dy == 0) {
        draw_hline(img, y1, x1, x2, color);
        return;
    }
    
    xx0 = x1;
    yy0 = y1;
    xx1 = x2;
    yy1 = y2;
    
    // Reorder points so yy0 < yy1
    if (yy0 > yy1) {
        tmp = yy0;
        yy0 = yy1;
        yy1 = tmp;
        tmp = xx0;
        xx0 = xx1;
        xx1 = tmp;
    }
    
    // Recalculate distance
    dx = xx1 - xx0;
    dy = yy1 - yy0;
    
    // Adjust for negative dx and set xdir
    if (dx >= 0) {
        xdir = 1;
    } else {
        xdir = -1;
        dx = -dx;
    }
    
    // Zero accumulator
    erracc = 0;
    
    // Draw the initial pixel
    set_pixel_blend(img, x1, y1, color);
    
    // x-major or y-major?
    if (dy > dx) {
        // y-major: Calculate fixed point fractional part
        // erradj represents how much X advances for each Y step
        erradj = ((dx << 16) / dy) << 16;
        
        i32 x0pxdir = xx0 + xdir;
        
        // Draw all pixels other than the first and last
        while (--dy) {
            erracctmp = erracc;
            erracc += erradj;
            
            if (erracc <= erracctmp) {
                // Rollover in error accumulator, x coord advances
                xx0 = x0pxdir;
                x0pxdir += xdir;
            }
            yy0++; // y-major so always advance Y
            
            // Extract intensity weighting from upper bits
            wgt = (erracc >> 24) & 255;
            set_pixel_weighted(img, xx0, yy0, color, 255 - wgt);
            set_pixel_weighted(img, x0pxdir, yy0, color, wgt);
        }
    } else {
        // x-major: Calculate fixed point fractional part
        // erradj represents how much Y advances for each X step
        erradj = ((dy << 16) / dx) << 16;
        
        i32 y0p1 = yy0 + 1;
        
        // Draw all pixels other than the first and last
        while (--dx) {
            erracctmp = erracc;
            erracc += erradj;
            
            if (erracc <= erracctmp) {
                // Accumulator turned over, advance y
                yy0 = y0p1;
                y0p1++;
            }
            xx0 += xdir; // x-major so always advance X
            
            // Extract intensity weighting from upper bits
            wgt = (erracc >> 24) & 255;
            set_pixel_weighted(img, xx0, yy0, color, 255 - wgt);
            set_pixel_weighted(img, xx0, y0p1, color, wgt);
        }
    }
    
    set_pixel_blend(img, x2, y2, color);
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

void draw_circle(image_view_t *img, i32 cx, i32 cy, i32 radius, color4_t color) 
{
    i32 x = 0;
    i32 y = radius;
    i32 d = 3 - 2 * radius;
    
    while (y >= x) 
    {
        set_pixel_blend(img, cx + x, cy + y, color);
        set_pixel_blend(img, cx - x, cy + y, color);
        set_pixel_blend(img, cx + x, cy - y, color);
        set_pixel_blend(img, cx - x, cy - y, color);
        set_pixel_blend(img, cx + y, cy + x, color);
        set_pixel_blend(img, cx - y, cy + x, color);
        set_pixel_blend(img, cx + y, cy - x, color);
        set_pixel_blend(img, cx - y, cy - x, color);
        
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

void fill_circle(image_view_t *img, i32 cx, i32 cy, i32 radius, color4_t color) 
{
    for (i32 y = -radius; y <= radius; y++) {
        for (i32 x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                set_pixel_blend(img, cx + x, cy + y, color);
            }
        }
    }
}

void draw_circle_aa(image_view_t *img, i32 cx, i32 cy, f32 radius, color4_t color) 
{
    if (radius <= 0) return;
    
    f32 rsq = radius * radius;
    i32 ffd = (i32)roundf(radius / sqrtf(2.0f)); // forty-five degree coord
    
    for (i32 xi = 0; xi <= ffd; xi++) 
    {
        f32 yj = sqrtf(rsq - xi * xi);
        f32 frc = yj - floorf(yj);
        i32 flr = (i32)floorf(yj);
        i8 weight1 = (i8)((1.0f - frc) * 255.0f);
        i8 weight2 = (i8)(frc * 255.0f);
        
        set_pixel_weighted(img, cx + xi, cy + flr, color, weight1);
        set_pixel_weighted(img, cx - xi, cy + flr, color, weight1);
        set_pixel_weighted(img, cx + xi, cy - flr, color, weight1);
        set_pixel_weighted(img, cx - xi, cy - flr, color, weight1);
        
        set_pixel_weighted(img, cx + xi, cy + flr + 1, color, weight2);
        set_pixel_weighted(img, cx - xi, cy + flr + 1, color, weight2);
        set_pixel_weighted(img, cx + xi, cy - flr - 1, color, weight2);
        set_pixel_weighted(img, cx - xi, cy - flr - 1, color, weight2);
    }
    
    for (i32 yi = 0; yi <= ffd; yi++) 
    {
        f32 xj = sqrtf(rsq - yi * yi);
        f32 frc = xj - floorf(xj);
        i32 flr = (i32)floorf(xj);
        i8 weight1 = (i8)((1.0f - frc) * 255.0f);
        i8 weight2 = (i8)(frc * 255.0f);
        
        set_pixel_weighted(img, cx + flr, cy + yi, color, weight1);
        set_pixel_weighted(img, cx - flr, cy + yi, color, weight1);
        set_pixel_weighted(img, cx + flr, cy - yi, color, weight1);
        set_pixel_weighted(img, cx - flr, cy - yi, color, weight1);
        
        set_pixel_weighted(img, cx + flr + 1, cy + yi, color, weight2);
        set_pixel_weighted(img, cx - flr - 1, cy + yi, color, weight2);
        set_pixel_weighted(img, cx + flr + 1, cy - yi, color, weight2);
        set_pixel_weighted(img, cx - flr - 1, cy - yi, color, weight2);
    }
}

void draw_circle_filled_aa(image_view_t *img, i32 cx, i32 cy, f32 radius, color4_t color) 
{
    if (radius <= 0) return;
    
    i32 r_ceil = (i32)ceilf(radius);
    f32 r_inner = radius - 1.0f;
    f32 r_inner_sq = r_inner * r_inner;
    f32 r_outer_sq = (radius + 1.0f) * (radius + 1.0f);
    
    for (i32 y = -r_ceil - 1; y <= r_ceil + 1; y++) 
    {
        f32 y_sq = y * y;
        i32 x_max = (i32)sqrtf(r_outer_sq - y_sq) + 1;
        
        for (i32 x = -x_max; x <= x_max; x++) 
        {
            f32 dist_sq = x * x + y_sq;
            
            if (dist_sq <= r_inner_sq) 
            {
                set_pixel_blend(img, cx + x, cy + y, color);
            }
            else if (dist_sq < r_outer_sq) 
            {
                f32 dist = sqrtf(dist_sq);
                f32 coverage = radius - dist + 0.5f;
                if (coverage > 0.0f && coverage < 1.0f) 
                {
                    i8 weight = (i8)(coverage * 255.0f);
                    set_pixel_weighted(img, cx + x, cy + y, color, weight);
                }
                else if (coverage >= 1.0f) 
                {
                    set_pixel_blend(img, cx + x, cy + y, color);
                }
            }
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
    
    // Region 1
    i32 p = ry2 - (rx2 * ry) + (rx2 / 4);
    while (px < py) {
        set_pixel_blend(img, cx + x, cy + y, color);
        set_pixel_blend(img, cx - x, cy + y, color);
        set_pixel_blend(img, cx + x, cy - y, color);
        set_pixel_blend(img, cx - x, cy - y, color);
        
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
    
    // Region 2
    p = (i32)(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
        set_pixel_blend(img, cx + x, cy + y, color);
        set_pixel_blend(img, cx - x, cy + y, color);
        set_pixel_blend(img, cx + x, cy - y, color);
        set_pixel_blend(img, cx - x, cy - y, color);
        
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

void fill_ellipse(image_view_t *img, i32 cx, i32 cy, i32 rx, i32 ry, color4_t color) 
{
    if (rx <= 0 || ry <= 0) return;
    
    for (i32 y = -ry; y <= ry; y++) {
        i32 dx = (i32)(rx * sqrt(1.0 - (double)(y * y) / (ry * ry)));
        draw_hline(img,  cy + y, cx - dx, cx + dx, color);
    }
}

void draw_triangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 x3, i32 y3, color4_t color) 
{
    draw_line(img, x1, y1, x2, y2, color);
    draw_line(img, x2, y2, x3, y3, color);
    draw_line(img, x3, y3, x1, y1, color);
}

void fill_triangle_scanline(image_view_t *img, i32 y, i32 x1, i32 x2, color4_t color) 
{
    if (x1 > x2) { 
        SWAP(x1, x2, i32);
    }

    draw_hline(img, x1, x2, y, color);
}

void fill_triangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 x3, i32 y3, color4_t color) 
{
    // Sort vertices by Y coordinate
    if (y1 > y2) { i32 tmp; tmp = x1; x1 = x2; x2 = tmp; tmp = y1; y1 = y2; y2 = tmp; }
    if (y2 > y3) { i32 tmp; tmp = x2; x2 = x3; x3 = tmp; tmp = y2; y2 = y3; y3 = tmp; }
    if (y1 > y2) { i32 tmp; tmp = x1; x1 = x2; x2 = tmp; tmp = y1; y1 = y2; y2 = tmp; }
    
    if (y1 == y3) return; // Degenerate case
    
    for (i32 y = y1; y <= y3; y++) {
        i32 xa, xb;
        
        if (y < y2) {
            // Upper half
            if (y2 != y1) xa = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            else xa = x1;
            if (y3 != y1) xb = x1 + (x3 - x1) * (y - y1) / (y3 - y1);
            else xb = x1;
        } else {
            // Lower half
            if (y3 != y2) xa = x2 + (x3 - x2) * (y - y2) / (y3 - y2);
            else xa = x2;
            if (y3 != y1) xb = x1 + (x3 - x1) * (y - y1) / (y3 - y1);
            else xb = x1;
        }
        
        fill_triangle_scanline(img, y, xa, xb, color);
    }
}

void draw_arc_quadrant(image_view_t *img, i32 cx, i32 cy, i32 radius, i32 quadrant, color4_t color) 
{
    i32 x = 0;
    i32 y = radius;
    i32 d = 3 - 2 * radius;
    
    while (y >= x) {
        switch (quadrant) {
            case 0: 
                set_pixel_blend(img, cx + x, cy - y, color);
                set_pixel_blend(img, cx + y, cy - x, color);
                break;
            case 1: 
                set_pixel_blend(img, cx - x, cy - y, color);
                set_pixel_blend(img, cx - y, cy - x, color);
                break;
            case 2: 
                set_pixel_blend(img, cx - x, cy + y, color);
                set_pixel_blend(img, cx - y, cy + x, color);
                break;
            case 3: 
                set_pixel_blend(img, cx + x, cy + y, color);
                set_pixel_blend(img, cx + y, cy + x, color);
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

void fill_arc_quadrant(image_view_t *img, i32 cx, i32 cy, i32 radius, i32 quadrant, color4_t color) 
{
    i32 r_squared = radius * radius;
    
    for (i32 y = 0; y <= radius; y++) 
    {
        i32 y_squared = y * y;
        
        for (i32 x = 0; x <= radius; x++) 
        {
            if (x * x + y_squared > r_squared) 
            {
                break; 
            }
            
            i32 px, py;
            switch (quadrant) {
                case 0: px = cx + x; py = cy - y; break;
                case 1: px = cx - x; py = cy - y; break;
                case 2: px = cx - x; py = cy + y; break;
                case 3: px = cx + x; py = cy + y; break;
                default: continue;
            }
            
            set_pixel_blend(img, px, py, color);
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

void fill_rounded_rectangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 radius, color4_t color) 
{
    if (x1 > x2) { SWAP(x1, x2, i32); }
    if (y1 > y2) { SWAP(y1, y2, i32); }
    
    i32 w = x2 - x1 + 1;
    i32 h = y2 - y1 + 1;
    
    radius = ClampTop(radius, MIN(w,h) / 2);

    if (radius <= 1) {
        draw_rect_solid(img, x1, y1, x2, y2, color);
        return;
    }
    
    fill_arc_quadrant(img, x1 + radius, y1 + radius, radius, 1, color); 
    fill_arc_quadrant(img, x2 - radius, y1 + radius, radius, 0, color); 
    fill_arc_quadrant(img, x1 + radius, y2 - radius, radius, 2, color); 
    fill_arc_quadrant(img, x2 - radius, y2 - radius, radius, 3, color); 
    
    draw_rect_solid(img, x1 + radius, y1, x2 - radius, y1 + radius - 1, color); 
    draw_rect_solid(img, x1 + radius, y2 - radius + 1, x2 - radius, y2, color); 
    draw_rect_solid(img, x1, y1 + radius, x2, y2 - radius, color); 
}

void fill_rounded_rectangle_wh(image_view_t *img, i32 x, i32 y, i32 width, i32 height, i32 radius, color4_t color) 
{
    i32 x2 = x + width;
    i32 y2 = y + height;
    
    fill_rounded_rectangle(img, x, y, x2, y2, radius, color);
}

static void fill_arc_quadrant_aa(image_view_t *img, i32 cx, i32 cy, f32 radius, i32 quadrant, color4_t color)
{
    if (radius <= 0) return;
    
    f32 rsq = radius * radius;
    i32 r_ceil = (i32)ceilf(radius);
    
    for (i32 yi = 0; yi <= r_ceil; yi++)
    {
        if (yi * yi > rsq) continue;
        
        f32 xj = sqrtf(rsq - yi * yi);
        f32 frc = xj - floorf(xj);
        i32 flr = (i32)floorf(xj);
        i8 weight_outer = (i8)(frc * 255.0f);
        
        switch (quadrant) {
            case 0: // Top-right
                for (i32 x = 0; x <= flr; x++)
                    set_pixel_blend(img, cx + x, cy - yi, color);
                set_pixel_weighted(img, cx + flr + 1, cy - yi, color, weight_outer);
                break;
            case 1: // Top-left
                for (i32 x = -flr; x <= 0; x++)
                    set_pixel_blend(img, cx + x, cy - yi, color);
                set_pixel_weighted(img, cx - flr - 1, cy - yi, color, weight_outer);
                break;
            case 2: // Bottom-left
                for (i32 x = -flr; x <= 0; x++)
                    set_pixel_blend(img, cx + x, cy + yi, color);
                set_pixel_weighted(img, cx - flr - 1, cy + yi, color, weight_outer);
                break;
            case 3: // Bottom-right
                for (i32 x = 0; x <= flr; x++)
                    set_pixel_blend(img, cx + x, cy + yi, color);
                set_pixel_weighted(img, cx + flr + 1, cy + yi, color, weight_outer);
                break;
        }
    }
}

void fill_rounded_rectangle_aa(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, f32 radius, color4_t color) 
{
    if (x1 == x2 && y1 == y2) {
        set_pixel_blend(img, x1, y1, color);
        return;
    }
    
    if (x1 > x2) { SWAP(x1, x2, i32); }
    if (y1 > y2) { SWAP(y1, y2, i32); }
    
    i32 w = x2 - x1 + 1;
    i32 h = y2 - y1 + 1;
    
    radius = ClampTop(radius, MIN(w,h) / 2);
    
    if (radius <= 1) {
        draw_rect_solid(img, x1, y1, x2, y2, color);
        return;
    }
    
    i32 r = (i32)ceilf(radius);
    
    fill_arc_quadrant_aa(img, x1 + r, y1 + r, radius, 1, color);
    fill_arc_quadrant_aa(img, x2 - r, y1 + r, radius, 0, color);
    fill_arc_quadrant_aa(img, x1 + r, y2 - r, radius, 2, color);
    fill_arc_quadrant_aa(img, x2 - r, y2 - r, radius, 3, color);
    
    i32 cx1 = x1 + r;
    i32 cx2 = x2 - r;
    i32 cy1 = y1 + r;
    i32 cy2 = y2 - r;
    
    if (cx1 <= cx2) {
        draw_rect_solid(img, cx1, y1, cx2, y2, color);
    }
    if (cy1 <= cy2) {
        draw_rect_solid(img, x1, cy1, cx1 - 1, cy2, color);
        draw_rect_solid(img, cx2 + 1, cy1, x2, cy2, color);
    }
}

#define TGA_HEADER(buf,w,h,b) \
    header[2]  = 2;\
    header[12] = (w) & 0xFF;\
    header[13] = ((w) >> 8) & 0xFF;\
    header[14] = (h) & 0xFF;\
    header[15] = ((h) >> 8) & 0xFF;\
    header[16] = (b);\
    header[17] |= 0x20 

void export_image(image_view_t const *color_buf, const char *filename) 
{
    FILE *file = fopen(filename, "wb");

    if (!file) {
        perror("Failed to open file");
        return;
    }

    uint8_t header[18] = {0};
    TGA_HEADER(header, color_buf->width, color_buf->height, 32);

    fwrite(header, sizeof(uint8_t), 18, file);

    for (size_t y = 0; y < color_buf->height; ++y) {
        for (size_t x = 0; x < color_buf->width; ++x) {
            color4_t pixel = BUF_AT(color_buf, x, y);
            uint8_t bgra[4] = { pixel.b, pixel.g, pixel.r, pixel.a };
            fwrite(bgra, sizeof(uint8_t), 4, file);
        }
    }
    
    fclose(file);
}

// check whether a point is inside a rectangle
bool inside_rect(i32 x, i32 y, rect_t *s)
{
    bool check_x = (x >= s->x) && (x < (s->x + (i32)s->w));
    bool check_y = (y >= s->y) && (y < (s->y + (i32)s->h));
    return (check_x && check_y);
}

// Get the intersection rect of two rectangles
rect_t intersect_rects(const rect_t *a, const rect_t *b)
{
    i32 left = (a->x > b->x) ? a->x : b->x;
    i32 top  = (a->y > b->y) ? a->y : b->y;
    i32 right = ((a->x + (i32)a->w) < (b->x + (i32)b->w)) ? 
                (a->x + (i32)a->w) : (b->x + (i32)b->w);
    i32 bottom = ((a->y + (i32)a->h) < (b->y + (i32)b->h)) ? 
                 (a->y + (i32)a->h) : (b->y + (i32)b->h);
    
    rect_t result = {0};
    if (right > left && bottom > top) {
        result.x = left;
        result.y = top;
        result.w = (u32)(right - left);
        result.h = (u32)(bottom - top);
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
    scissor_enabled = found_first && (effective.w > 0 && effective.h > 0);
}

bool push_scissor(i32 x, i32 y, u32 width, u32 height)
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

bool pop_scissor()
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

bool rect_intersects_current_scissor(i32 x, i32 y, u32 width, u32 height)
{
    if (!scissor_enabled)
        return true;
    
    rect_t *s = &current_scissor;
    
    if (x >= (s->x + (i32)s->w)  ||
        y >= (s->y + (i32)s->h)  ||
        s->x >= (x + (i32)width) ||
        s->y >= (y + (i32)height))
    {
        return false;
    }
    
    return true;
}

void clip_rect_to_current_scissor(i32 *x, i32 *y, u32 *width, u32 *height)
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

void set_pixel_scissored(image_view_t *img, i32 x, i32 y, color4_t color)
{
    if (!pixel_in_current_scissor(x, y)){
        return;
    }
    
    draw_pixel(img, x, y, color);
}

void draw_rect_scissored(image_view_t const *img, i32 x, i32 y, u32 width, u32 height, color4_t color)
{
    if (!rect_intersects_current_scissor(x, y, width, height))
        return;
    
    i32 clipped_x = x;
    i32 clipped_y = y;
    u32 clipped_width = width;
    u32 clipped_height = height;
    
    clip_rect_to_current_scissor(&clipped_x, &clipped_y, &clipped_width, &clipped_height);
    
    draw_rect_solid_wh(img, clipped_x, clipped_y, clipped_width, clipped_height, color);
}

void draw_rounded_rect_scissored(image_view_t *img, i32 x, i32 y, u32 width, u32 height, i32 rad, color4_t color)
{
    if (!rect_intersects_current_scissor(x, y, width, height))
        return;
    
    i32 clipped_x = x;
    i32 clipped_y = y;
    u32 clipped_width = width;
    u32 clipped_height = height;
    
    clip_rect_to_current_scissor(&clipped_x, &clipped_y, &clipped_width, &clipped_height);
    
    fill_rounded_rectangle_wh(img, clipped_x, clipped_y, clipped_width, clipped_height, rad, color);
}

void clear_scissor_stack()
{
    scissor_stack_size = 0;
    scissor_enabled = false;
}


static inline void polar_to_cartesian(polar_t polar, f32 center_x, f32 center_y,  f32 scale_x, f32 scale_y, f32 *out_x, f32 *out_y) 
{
    *out_x = center_x + (polar.radius * cosf(polar.angle)) * scale_x;
    *out_y = center_y + (polar.radius * sinf(polar.angle)) * scale_y;
}

static inline f32 radial_angle_step(i32 num_segments, f32 gap_factor) 
{
    f32 full_step = (2.0f * M_PI) / num_segments;
    return full_step * (1.0f - gap_factor * 0.5f);
}

static inline radial_segment_t radial_get_segment(radial_layout_t *layout, i32 i, f32 height) 
{
    f32 angle_step = radial_angle_step(layout->num_segments, layout->gap_factor);
    
    radial_segment_t seg;
    seg.angle_start = (i * angle_step) + layout->rotation;
    seg.angle_end = ((i + 1) * angle_step) + layout->rotation;
    seg.inner_radius = layout->inner_radius;
    seg.outer_radius = layout->inner_radius + height * (layout->outer_radius - layout->inner_radius);
    
    return seg;
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
