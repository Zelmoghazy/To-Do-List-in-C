#ifndef BASE_GR_H_
#define BASE_GR_H_

#include "util.h"

typedef struct 
{
    f32 x, y;     
    f32 r, g, b;
    f32 s, t;
} vertex_t;

typedef struct color4_t
{
    u8 r, g, b, a;
}color4_t;

typedef struct image_view_t
{
    color4_t    *pixels;
    u32         width, height;
}image_view_t;

#define BUF_AT(C,x,y)   (C)->pixels[(x)+(y)*C->width]

typedef struct {
    i32 x,y,w,h;
    bool enabled;
}scissor_region_t;

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
    i32 num_segments;
    f32 inner_radius;
    f32 outer_radius;
    f32 rotation;              
    f32 gap_factor;    
} radial_layout_t;

#define A(c)           ((u8) ((c) >> 24))
#define R(c)           ((u8) ((c) >> 16))
#define G(c)           ((u8) ((c) >>  8))
#define B(c)           ((u8)  (c)       )

#define RGB_GREY(x) ((color4_t){x, x, x, 255})


#define RGBA_TO_U32(r, g, b, a)  ((u8)(r) | ((u8)(g) << 8) | ((u8)(b) << 16) | ((u8)(a) << 24))
#define HEX_TO_COLOR4(hex) (color4_t){(R(hex) & 0xFF), (G(hex) & 0xFF), (B(hex) & 0xFF), 255}   
#define HEX_TO_COLOR4_NORMALIZED(hex) \
    (color4_t){(R(hex) & 0xFF) * (1.0f/255.0f),\
               (G(hex) & 0xFF) * (1.0f/255.0f),\
               (B(hex) & 0xFF) * (1.0f/255.0f),\
                1.0f}
#define HEX_TO_RGBA(s,hex)  \
    s.r = (R(hex) & 0xFF);  \
    s.g = (G(hex) & 0xFF);  \
    s.b = (B(hex) & 0xFF);  \
    s.a = 255     

#define IS_OPAQUE(c) (c.a == 255)
#define IS_INVIS(c) (c.a == 0)

#define COLOR_RED           (color4_t){255, 0, 0, 255}
#define COLOR_GREEN         (color4_t){0, 255, 0, 255}
#define COLOR_BLUE          (color4_t){0, 0, 255, 255}
#define COLOR_WHITE         (color4_t){255, 255, 255, 255}
#define COLOR_BLACK         (color4_t){0, 0, 0, 255}
#define COLOR_YELLOW        (color4_t){255, 255, 0, 255}
#define COLOR_CYAN          (color4_t){0, 255, 255, 255}
#define COLOR_MAGENTA       (color4_t){255, 0, 255, 255}
#define COLOR_ORANGE        (color4_t){255, 165, 0, 255}
#define COLOR_PURPLE        (color4_t){128, 0, 128, 255}
#define COLOR_PINK          (color4_t){255, 192, 203, 255}
#define COLOR_GRAY          (color4_t){128, 128, 128, 255}
#define COLOR_LIGHT_GRAY    (color4_t){211, 211, 211, 255}
#define COLOR_DARK_GRAY     (color4_t){169, 169, 169, 255}
#define COLOR_BROWN         (color4_t){139, 69, 19, 255}
#define COLOR_NAVY          (color4_t){0, 0, 128, 255}
#define COLOR_LIME          (color4_t){0, 255, 0, 255}
#define COLOR_TEAL          (color4_t){0, 128, 128, 255}
#define COLOR_MAROON        (color4_t){128, 0, 0, 255}
#define COLOR_OLIVE         (color4_t){128, 128, 0, 255}

#define MAX_SCISSOR_STACK 16

extern scissor_region_t scissor_stack[MAX_SCISSOR_STACK];
extern u32 scissor_stack_size;
extern rect_t current_scissor;
extern bool scissor_enabled;

color4_t to_color4(vec3f_t const c);
vec3f_t linear_to_gamma(vec3f_t color);
void hsv_to_rgb(f32 h, f32 s, f32 v, f32 *r, f32 *g, f32 *b);
f32 gradient_noise(f32 x, f32 y);
color4_t color4_lerp(color4_t c1, color4_t c2, f32 t);
color4_t average(color4_t c, color4_t d);
color4_t dark(color4_t c);
color4_t lite(color4_t c);

inline void set_pixel(image_view_t const *img, i32 x, i32 y, color4_t color);
inline color4_t get_pixel(image_view_t const *img, i32 x, i32 y);
inline color4_t blend_pixel(color4_t dst, color4_t src);
inline void set_pixel_blend(image_view_t const *img, i32 x, i32 y, color4_t color);
inline void set_pixel_weighted(image_view_t *img, i32 x, i32 y, color4_t color, i8 weight);
void draw_pixel(image_view_t *img, i32 x, i32 y, color4_t color);
void clear_screen(image_view_t const *color_buf, color4_t const color);
void clear_screen_radial_gradient(image_view_t const *color_buf,
                                       color4_t const color0,
                                       color4_t const color1);
void draw_hline(image_view_t const *color_buf, i32 y, i32 x0, i32 x1, color4_t const color);
void draw_vline(image_view_t const *color_buf, i32 x, i32 y0, i32 y1, color4_t const color);
void draw_line(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color);
void draw_aaline(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, color4_t color);
void draw_rect_outline_wh(image_view_t const *color_buf, i32 x0, i32 y0, i32 w , i32 h , color4_t const color);
void draw_rect_outline(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color);
void draw_rect_solid_wh(image_view_t const *color_buf, i32 x0, i32 y0, i32 w , i32 h , color4_t const color);
void draw_rect_solid(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color);
void draw_circle(image_view_t *img, i32 cx, i32 cy, i32 radius, color4_t color);
void fill_circle(image_view_t *img, i32 cx, i32 cy, i32 radius, color4_t color);
void draw_circle_aa(image_view_t *img, i32 cx, i32 cy, f32 radius, color4_t color);
void draw_circle_filled_aa(image_view_t *img, i32 cx, i32 cy, f32 radius, color4_t color);
void draw_ellipse(image_view_t *img, i32 cx, i32 cy, i32 rx, i32 ry, color4_t color);
void fill_ellipse(image_view_t *img, i32 cx, i32 cy, i32 rx, i32 ry, color4_t color);
void draw_triangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 x3, i32 y3, color4_t color);
void fill_triangle_scanline(image_view_t *img, i32 y, i32 x1, i32 x2, color4_t color);
void fill_triangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 x3, i32 y3, color4_t color);
void draw_arc_quadrant(image_view_t *img, i32 cx, i32 cy, i32 radius, i32 quadrant, color4_t color);
void fill_arc_quadrant(image_view_t *img, i32 cx, i32 cy, i32 radius, i32 quadrant, color4_t color);
void draw_rounded_rectangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 radius, color4_t color);
void fill_rounded_rectangle_wh(image_view_t *img, i32 x, i32 y, i32 width, i32 height, i32 radius, color4_t color);
void fill_arc_quadrant_aa(image_view_t *img, i32 cx, i32 cy, f32 radius, i32 quadrant, color4_t color);
void fill_rounded_rectangle_aa(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, f32 radius, color4_t color);

void export_image(image_view_t const *color_buf, const char *filename);

bool inside_rect(i32 x, i32 y, rect_t *s);
rect_t intersect_rects(const rect_t *a, const rect_t *b);
void update_current_scissor(void);
bool push_scissor(i32 x, i32 y, u32 width, u32 height);
bool pop_scissor();
bool pixel_in_current_scissor(i32 x, i32 y);
bool rect_intersects_current_scissor(i32 x, i32 y, u32 width, u32 height);
void clip_rect_to_current_scissor(i32 *x, i32 *y, u32 *width, u32 *height);
void set_pixel_scissored(image_view_t *img, i32 x, i32 y, color4_t color);
void draw_rect_scissored(image_view_t const *img, i32 x, i32 y, u32 width, u32 height, color4_t color);
void draw_rounded_rect_scissored(image_view_t *img, i32 x, i32 y, u32 width, u32 height, i32 rad, color4_t color);
void clear_scissor_stack();

void catmull_rom_point(f32 t, vertex_t p[4], vertex_t *out);

void polar_to_cartesian(polar_t polar, f32 center_x, f32 center_y, f32 scale_x, f32 scale_y, f32 *out_x, f32 *out_y);
f32 radial_angle_step(i32 num_segments, f32 gap_factor);
radial_segment_t radial_get_segment(radial_layout_t *layout, i32 i, f32 height);

#endif