#include "base_graphics.h"

scissor_region_t scissor_stack[MAX_SCISSOR_STACK];
u32 scissor_stack_size;
rect_t current_scissor;
bool scissor_enabled;

/*
    convert normalized color to 0->255 range
*/
color4_t to_color4(vec3f_t const c)
{
    color4_t res = {
        .r = (u8)MAX(0.0f, MIN(255.0f, c.x * 255.0f)),
        .g = (u8)MAX(0.0f, MIN(255.0f, c.y * 255.0f)),
        .b = (u8)MAX(0.0f, MIN(255.0f, c.z * 255.0f)),
        .a = (u8)255
    };

    return res;
}

vec3f_t linear_to_gamma(vec3f_t color)
{
    vec3f_t res = {0.0f,0.0f,0.0f};
    if(color.x > 0) res.x = sqrt_f32(color.x);
    if(color.y > 0) res.y = sqrt_f32(color.y);
    if(color.z > 0) res.z = sqrt_f32(color.z);
    return res;
}

void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b) 
{
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    
    switch (i % 6) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        case 5: *r = v; *g = p; *b = q; break;
    }
}

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
    if (x >= 0 && x < (i32)img->width &&
        y >= 0 && y < (i32)img->height) 
    {
        return BUF_AT(img, x, y);
    }
    return (color4_t){0, 0, 0, 0};
}

/* alpha blending to combine stuff depending on transparency */
inline color4_t blend_pixel(color4_t dst, color4_t src) 
{
    if (src.a == 255) return src;
    if (src.a == 0)   return dst;
    
    u8 inv_alpha = 255 - src.a;

    // Linear interpolation between source and destination
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
        if (color.a == 255) 
        {
            BUF_AT(img, x, y) = color;
        } 
        else 
        {
            color4_t dst = get_pixel(img, x, y);
            BUF_AT(img, x, y) = blend_pixel(dst, color);
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
            set_pixel(color_buf, x, y, color);    
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
        swap(&x0, &x1);
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
        swap(&y0, &y1);
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
        swap(&x0, &y0);
        swap(&x1, &y1);
        steep = true;
    }
    
    // Make it left-to-right
    if (x0 > x1) {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }
    
    int dx = x1 - x0;
    int dy = y1 - y0;
    int derror2 = abs(dy) * 2;
    int error2 = 0;
    int y = y0;
    
    for (int x = x0; x <= x1; x++) 
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
    i32 dx = abs(x2 - x1);
    i32 dy = abs(y2 - y1);
    
    if (dx == 0) {
        draw_vline(img, x1, y1, y2, color);
        return;
    }
    if (dy == 0) {
        draw_hline(img, y1, x1, x2,color);
        return;
    }
    
    i32 sx = (x1 < x2) ? 1 : -1;
    i32 sy = (y1 < y2) ? 1 : -1;
    
    if (dy > dx) 
    {
        // Steep line
        i32 gradient = (dx << 16) / dy;
        i32 xend = x1 << 16;
        
        for (i32 y = y1; y != y2; y += sy) {
            i32 x = xend >> 16;
            u8 alpha1 = 255 - ((xend & 0xFFFF) >> 8);
            u8 alpha2 = (xend & 0xFFFF) >> 8;
            
            set_pixel_weighted(img, x, y, color, alpha1);
            set_pixel_weighted(img, x + sx, y, color, alpha2);
            
            xend += gradient * sy;
        }
    } 
    else
    {
        // Shallow line
        i32 gradient = (dy << 16) / dx;
        i32 yend = y1 << 16;
        
        for (i32 x = x1; x != x2; x += sx) 
        {
            i32 y = yend >> 16;
            u8 alpha1 = 255 - ((yend & 0xFFFF) >> 8);
            u8 alpha2 = (yend & 0xFFFF) >> 8;
            
            set_pixel_weighted(img, x, y, color, alpha1);
            set_pixel_weighted(img, x, y + sy, color, alpha2);
            
            yend += gradient * sx;
        }
    }
    set_pixel_blend(img, x2, y2, color);
}

void draw_rect_outline_wh(image_view_t const *color_buf, int x0, int y0, int w , int h , color4_t const color)
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
      int x1 = x0+w-1, y1 = y0+h-1;
      draw_hline(color_buf, y0,x0,x1-1, color);
      draw_vline(color_buf, x1,y0,y1-1, color);
      draw_hline(color_buf, y1,x0+1,x1, color);
      draw_vline(color_buf, x0,y0+1,y1, color);
   }
}

void draw_rect_outline(image_view_t const *color_buf, int x0, int y0, int x1, int y1, color4_t const color)
{
    if (x1 < x0) 
    { 
        swap(&x1, &x0);
    }
    if (y1 < y0) 
    { 
        swap(&y1, &y0);
    }
    draw_rect_outline_wh(color_buf, x0,y0, x1-x0+1, y1-y0+1, color);
}

void draw_rect_solid_wh(image_view_t const *color_buf, int x0, int y0, int w , int h , color4_t const color)
{
    if (w > 0) 
    {   
        int j, x1 = x0 + w -1;
        for (j=0; j < h; ++j)
        {
            draw_hline(color_buf, y0+j, x0, x1, color);
        }
    }
}
  
void draw_rect_solid(image_view_t const *color_buf, int x0, int y0, int x1, int y1, color4_t const color)
{
    if (x1 < x0) 
    { 
        swap(&x1, &x0);
    }
    if (y1 < y0) 
    { 
        swap(&y1, &y0);
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
    if (x1 > x2) { i32 tmp = x1; x1 = x2; x2 = tmp; }
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
            case 0: // Top-right
                set_pixel_blend(img, cx + x, cy - y, color);
                set_pixel_blend(img, cx + y, cy - x, color);
                break;
            case 1: // Top-left
                set_pixel_blend(img, cx - x, cy - y, color);
                set_pixel_blend(img, cx - y, cy - x, color);
                break;
            case 2: // Bottom-left
                set_pixel_blend(img, cx - x, cy + y, color);
                set_pixel_blend(img, cx - y, cy + x, color);
                break;
            case 3: // Bottom-right
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

 void fill_arc_quadrant(image_view_t *img, i32 cx, i32 cy, i32 radius, int quadrant, color4_t color) 
{
    for (i32 y = 0; y <= radius; y++) {
        for (i32 x = 0; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                switch (quadrant) {
                    case 0: // Top-right
                        set_pixel_blend(img, cx + x, cy - y, color);
                        break;
                    case 1: // Top-left
                        set_pixel_blend(img, cx - x, cy - y, color);
                        break;
                    case 2: // Bottom-left
                        set_pixel_blend(img, cx - x, cy + y, color);
                        break;
                    case 3: // Bottom-right
                        set_pixel_blend(img, cx + x, cy + y, color);
                        break;
                }
            }
        }
    }
}

void draw_rounded_rectangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 radius, color4_t color) 
{
    if (x1 > x2) { i32 tmp = x1; x1 = x2; x2 = tmp; }
    if (y1 > y2) { i32 tmp = y1; y1 = y2; y2 = tmp; }
    
    i32 w = x2 - x1;
    i32 h = y2 - y1;
    
    // Clamp radius to maximum possible
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;
    
    if (radius <= 1) {
        draw_rect_outline(img, x1, y1, x2, y2, color);
        return;
    }
    
    // Draw corner arcs
    draw_arc_quadrant(img, x1 + radius, y1 + radius, radius, 1, color); // Top-left
    draw_arc_quadrant(img, x2 - radius, y1 + radius, radius, 0, color); // Top-right
    draw_arc_quadrant(img, x1 + radius, y2 - radius, radius, 2, color); // Bottom-left
    draw_arc_quadrant(img, x2 - radius, y2 - radius, radius, 3, color); // Bottom-right
    
    // Draw straight edges
    if (x1 + radius <= x2 - radius) {
        draw_hline(img, y1, x1 + radius, x2 - radius, color); // Top
        draw_hline(img, y2, x1 + radius, x2 - radius, color); // Bottom
    }
    if (y1 + radius <= y2 - radius) {
        draw_vline(img, x1, y1 + radius, y2 - radius, color); // Left
        draw_vline(img, x2, y1 + radius, y2 - radius, color); // Right
    }
}

void fill_rounded_rectangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 radius, color4_t color) 
{
    if (x1 > x2) { i32 tmp = x1; x1 = x2; x2 = tmp; }
    if (y1 > y2) { i32 tmp = y1; y1 = y2; y2 = tmp; }
    
    i32 w = x2 - x1;
    i32 h = y2 - y1;
    
    // Clamp radius to maximum possible
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;
    
    if (radius <= 1) {
        draw_rect_solid(img, x1, y1, x2, y2, color);
        return;
    }
    
    // Fill corner arcs
    fill_arc_quadrant(img, x1 + radius, y1 + radius, radius, 1, color); // Top-left
    fill_arc_quadrant(img, x2 - radius, y1 + radius, radius, 0, color); // Top-right
    fill_arc_quadrant(img, x1 + radius, y2 - radius, radius, 2, color); // Bottom-left
    fill_arc_quadrant(img, x2 - radius, y2 - radius, radius, 3, color); // Bottom-right
    
    // Fill main rectangular areas
    if (x1 + radius <= x2 - radius) {
        draw_rect_solid(img, x1 + radius, y1, x2 - radius, y2, color); // Middle section
    }
    if (y1 + radius <= y2 - radius) {
        draw_rect_solid(img, x1, y1 + radius, x1 + radius - 1, y2 - radius, color); // Left section
        draw_rect_solid(img, x2 - radius + 1, y1 + radius, x2, y2 - radius, color); // Right section
    }
}

void fill_rounded_rectangle_wh(image_view_t *img, i32 x, i32 y, i32 width, i32 height, i32 radius, color4_t color) 
{
    i32 x2 = x + width;
    i32 y2 = y + height;
    
    fill_rounded_rectangle(img, x, y, x2, y2, radius, color);
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

void set_pixel_scissored(image_view_t const *img, i32 x, i32 y, color4_t color)
{
    if (x < 0 || y < 0 || 
        x >= (i32)img->width || 
        y >= (i32)img->height)
    {
        return;
    }
    
    // Apply scissor test
    if (!pixel_in_current_scissor(x, y)){
        return;
    }
    
    set_pixel(img, x, y, color);
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

animation_t animation_items[ANIMATION_MAX_ITEMS];
int animation_item_count;

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
    for (int i = 0; i < animation_item_count; i++) 
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
    for(int i = animation_item_count-1; i>=0; i--)
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

void animation_get(u64 id, f32 *current)
{
    for (int i = 0; i < animation_item_count; i++) 
    {
        animation_t *it = &animation_items[i];

        if (it->id == id) {
            *current = it->current;
        }
    }
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

typedef struct 
{
    f32 radius;
    f32 angle;  // rad
} polar_t;

typedef struct 
{
    f32 inner_radius;
    f32 outer_radius;
    f32 angle_start;
    f32 angle_end;
} radial_segment_t;

typedef struct 
{
    f32 center_x, center_y;
    f32 scale_x, scale_y;
    i32   num_segments;
    f32 inner_radius;
    f32 outer_radius;
    f32 rotation;              
    f32 gap_factor;    
} radial_layout_t;

static inline void polar_to_cartesian(polar_t polar, f32 center_x, f32 center_y, 
                                      f32 scale_x, f32 scale_y,
                                      f32 *out_x, f32 *out_y) 
{
    *out_x = center_x + (polar.radius * cosf(polar.angle)) * scale_x;
    *out_y = center_y + (polar.radius * sinf(polar.angle)) * scale_y;
}

static inline f32 radial_angle_step(i32 num_segments, f32 gap_factor) 
{
    f32 full_step = (2.0f * M_PI) / num_segments;
    return full_step * (1.0f - gap_factor * 0.5f);
}

// Get segment definition for index i
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

// Generate 4 corner vertices for a radial segment (trapezoid)
typedef struct {
    f32 x, y;
} point2d_t;

typedef struct {
    point2d_t inner1, inner2, outer1, outer2;
} radial_quad_t;

static inline radial_quad_t radial_segment_to_quad(radial_layout_t *layout, radial_segment_t seg) 
{
    radial_quad_t quad;
    
    polar_to_cartesian((polar_t){seg.inner_radius, seg.angle_start}, 
                       layout->center_x, layout->center_y, layout->scale_x, layout->scale_y,
                       &quad.inner1.x, &quad.inner1.y);
    
    polar_to_cartesian((polar_t){seg.inner_radius, seg.angle_end}, 
                       layout->center_x, layout->center_y, layout->scale_x, layout->scale_y,
                       &quad.inner2.x, &quad.inner2.y);
    
    polar_to_cartesian((polar_t){seg.outer_radius, seg.angle_start}, 
                       layout->center_x, layout->center_y, layout->scale_x, layout->scale_y,
                       &quad.outer1.x, &quad.outer1.y);
    
    polar_to_cartesian((polar_t){seg.outer_radius, seg.angle_end}, 
                       layout->center_x, layout->center_y, layout->scale_x, layout->scale_y,
                       &quad.outer2.x, &quad.outer2.y);
    
    return quad;
}

void render_radial_menu(const char **items, int num_items) 
{
    radial_layout_t layout = {
        .center_x = 0.0f, .center_y = 0.0f,
        .scale_x = 1.0f, .scale_y = 1.0f,
        .num_segments = num_items,
        .inner_radius = 0.2f,
        .outer_radius = 0.5f,
        .rotation = 0.0f,
        .gap_factor = 0.1f
    };
    
    for (int i = 0; i < num_items; i++) {
        radial_segment_t seg = radial_get_segment(&layout, i, 1.0f);
        // Calculate segment center for text placement
        f32 mid_angle = (seg.angle_start + seg.angle_end) / 2.0f;
        f32 mid_radius = (seg.inner_radius + seg.outer_radius) / 2.0f;
        
        f32 text_x, text_y;
        polar_to_cartesian((polar_t){mid_radius, mid_angle}, 
                          layout.center_x, layout.center_y, 
                          layout.scale_x, layout.scale_y,
                          &text_x, &text_y);
        
        // Draw segment background + text at (text_x, text_y)
    }
}