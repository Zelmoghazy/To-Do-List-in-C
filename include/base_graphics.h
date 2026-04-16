#ifndef BASE_GR_H_
#define BASE_GR_H_

#include "util.h"
#include "stb_image.h"
#include "stb_ds.h"

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
    i32 x, y, w, h;
}rect_t;

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

typedef struct 
{
    vec2f_t p0, p1, p2, p3;
    bool is_cubic;
    bool is_linear;
}bezier_segment_t;

typedef struct 
{
    bezier_segment_t *segments;
    
    int selected_segment;
    int selected_point;  // 0=p0, 1=p1, 2=p2
    bool is_dragging;

    // bounding box
    float min_x, min_y, max_x, max_y;
} path_data_t;

#define A(c)           ((u8) ((c) >> 24))
#define R(c)           ((u8) ((c) >> 16))
#define G(c)           ((u8) ((c) >>  8))
#define B(c)           ((u8)  (c)       )

#define RGB_GREY(x) ((color4_t){x, x, x, 255})
#define INV_255     (0.00392156862f)

#define RGBA_TO_U32(r, g, b, a)  ((u8)(r) | ((u8)(g) << 8) | ((u8)(b) << 16) | ((u8)(a) << 24))
#define HEX_TO_COLOR4(hex) (color4_t){(R(hex) & 0xFF), (G(hex) & 0xFF), (B(hex) & 0xFF), 255}   
#define HEX_TO_COLOR4_NORMALIZED(hex) \
    (color4_t){(R(hex) & 0xFF) * INV_255,\
               (G(hex) & 0xFF) * INV_255,\
               (B(hex) & 0xFF) * INV_255,\
                1.0f}
#define HEX_TO_RGBA(s,hex)  \
    s.r = (R(hex) & 0xFF);  \
    s.g = (G(hex) & 0xFF);  \
    s.b = (B(hex) & 0xFF);  \
    s.a = 255     

#define IS_OPAQUE(c) (c.a == 255)
#define IS_INVIS(c) (c.a == 0)

#define COLOR_TRANSPARENT   (color4_t){0, 0, 0, 0}
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

#define QUAD_BEZIER_EVAL(p0, p1, p2, t)\
        (vec2f_t){((1.0f - t)*(1.0f - t)*p0.x + 2*(1.0f - t)*t*p1.x + t*t*p2.x), \
                  ((1.0f - t)*(1.0f - t)*p0.y + 2*(1.0f - t)*t*p1.y + t*t*p2.y)} 

#define CUBIC_BEZIER_EVAL(p0, p1, p2, p3, t)\
    (vec2f_t){(1.0f-t)*(1.0f-t)*(1.0f-t)*p0.x + 3*(1.0f-t)*(1.0f-t)*t*p1.x + 3*(1.0f-t)*t*t*p2.x + t*t*t*p3.x, \
              (1.0f-t)*(1.0f-t)*(1.0f-t)*p0.y + 3*(1.0f-t)*(1.0f-t)*t*p1.y + 3*(1.0f-t)*t*t*p2.y + t*t*t*p3.y} 

#define QUAD_BEZIER_DERIV(p0, p1, p2, t)\
    (vec2f_t){(2.0f*(1.0f-t)*(p1.x-p0.x) + 2.0f*t*(p2.x-p1.x)), \
              (2.0f*(1.0f-t)*(p1.y-p0.y) + 2.0f*t*(p2.y-p1.y))}

#define CUBIC_BEZIER_DERIV(p0, p1, p2, p3, t)\
    (vec2f_t){(3.0f*(1.0f-t)*(1.0f-t)*(p1.x-p0.x) + 6.0f*(1.0f-t)*t*(p2.x-p1.x) + 3.0f*t*t*(p3.x-p2.x)), \
              (3.0f*(1.0f-t)*(1.0f-t)*(p1.y-p0.y) + 6.0f*(1.0f-t)*t*(p2.y-p1.y) + 3.0f*t*t*(p3.y-p2.y))}

/*
    update bounding box
 */
#define UPDATE_BB(bb, px, py) \
    do { \
        if ((px) < bb->min_x) bb->min_x = (px); \
        if ((px) > bb->max_x) bb->max_x = (px); \
        if ((py) < bb->min_y) bb->min_y = (py); \
        if ((py) > bb->max_y) bb->max_y = (py); \
    } while(0)

extern scissor_region_t scissor_stack[MAX_SCISSOR_STACK];
extern u32 scissor_stack_size;
extern rect_t current_scissor;
extern bool scissor_enabled;

#define TGA_HEADER(buf,w,h,b) \
    header[2]  = 2;\
    header[12] = (w) & 0xFF;\
    header[13] = ((w) >> 8) & 0xFF;\
    header[14] = (h) & 0xFF;\
    header[15] = ((h) >> 8) & 0xFF;\
    header[16] = (b);\
    header[17] |= 0x20 

color4_t to_color4(vec3f_t const c);
color4_t linear_to_gamma(color4_t color);
void hsv_to_rgb(f32 h, f32 s, f32 v, f32 *r, f32 *g, f32 *b);
f32 gradient_noise(f32 x, f32 y);
color4_t color4_lerp(color4_t c1, color4_t c2, f32 t);
color4_t average(color4_t c, color4_t d);
color4_t dark(color4_t c);
color4_t lite(color4_t c);
color4_t rgb_from_wavelength(f64 wave);

inline void set_pixel(image_view_t const *img, i32 x, i32 y, color4_t color);
inline color4_t get_pixel(image_view_t const *img, i32 x, i32 y);
inline color4_t blend_pixel(color4_t dst, color4_t src);
inline void set_pixel_blend(image_view_t const *img, i32 x, i32 y, color4_t color);
inline void set_pixel_weighted(image_view_t *img, i32 x, i32 y, color4_t color, u8 weight);
void draw_pixel(image_view_t const *img, i32 x, i32 y, color4_t color);
void clear_screen(image_view_t const *color_buf, color4_t const color);
void clear_screen_radial_gradient(image_view_t const *color_buf,
                                       color4_t const color0,
                                       color4_t const color1);

void draw_hline(image_view_t const *color_buf, i32 y, i32 x0, i32 x1, color4_t const color);
void draw_vline(image_view_t const *color_buf, i32 x, i32 y0, i32 y1, color4_t const color);
void draw_line(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color);
void draw_aaline(image_view_t const *img, i32 x1, i32 y1, i32 x2, i32 y2, color4_t color);

void draw_rect_outline_wh(image_view_t const *color_buf, i32 x0, i32 y0, i32 w , i32 h , color4_t const color);
void draw_rect_outline(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color);
void draw_rect_solid_wh(image_view_t const *color_buf, i32 x0, i32 y0, i32 w , i32 h , color4_t const color);
void draw_rect_solid(image_view_t const *color_buf, i32 x0, i32 y0, i32 x1, i32 y1, color4_t const color);

void draw_triangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 x3, i32 y3, color4_t color);
void draw_triangle_filled_scanline(image_view_t *img, i32 y, i32 x1, i32 x2, color4_t color);
void draw_triangle_filled(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 x3, i32 y3, color4_t color);

void draw_ellipse(image_view_t *img, i32 cx, i32 cy, i32 rx, i32 ry, color4_t color);
void draw_ellipse_filled(image_view_t *img, i32 cx, i32 cy, i32 rx, i32 ry, color4_t color);

void draw_circle(image_view_t *img, i32 cx, i32 cy, i32 radius, color4_t color);
void draw_circle_filled(image_view_t *img, i32 cx, i32 cy, i32 radius, color4_t color);
void draw_circle_aa(image_view_t *img, i32 cx, i32 cy, f32 radius, color4_t color);
void draw_circle_filled_aa(image_view_t *img, i32 cx, i32 cy, f32 radius, color4_t color);

void draw_arc_quadrant(image_view_t *img, i32 cx, i32 cy, i32 radius, i32 quadrant, color4_t color);
void draw_rounded_rectangle(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 radius, color4_t color);
void draw_rounded_rectangle_filled_wh(image_view_t *img, i32 x, i32 y, i32 width, i32 height, i32 radius, color4_t color);
void draw_rounded_rectangle_filled(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, i32 radius, color4_t color);
void draw_rounded_rectangle_filled_aa(image_view_t *img, i32 x1, i32 y1, i32 x2, i32 y2, f32 radius, color4_t color);

void export_image(image_view_t const *color_buf, const char *filename);

bool inside_rect(i32 x, i32 y, rect_t *s);
rect_t intersect_rects(const rect_t *a, const rect_t *b);
void update_current_scissor(void);
bool push_scissor(i32 x, i32 y, i32 width, i32 height);
bool pop_scissor();
bool pixel_in_current_scissor(i32 x, i32 y);
bool rect_intersects_current_scissor(i32 x, i32 y, i32 width, i32 height);
void clip_rect_to_current_scissor(i32 *x, i32 *y, i32 *width, i32 *height);
void set_pixel_scissored(image_view_t const*img, i32 x, i32 y, color4_t color);
void draw_rect_scissored(image_view_t const *img, i32 x, i32 y, i32 width, i32 height, color4_t color);
void draw_rounded_rect_scissored(image_view_t *img, i32 x, i32 y, i32 width, i32 height, i32 rad, color4_t color);
void clear_scissor_stack();

void polar_to_cartesian(polar_t polar, f32 center_x, f32 center_y, f32 scale_x, f32 scale_y, f32 *out_x, f32 *out_y);
void cartesian_to_polar(f32 x, f32 y,
                        f32 center_x, f32 center_y,
                        f32 scale_x, f32 scale_y,
                        polar_t *out_polar);
f32 normalize_angle(f32 a);

f32 radial_angle_step(i32 num_segments, f32 gap_factor);
radial_segment_t radial_get_segment(radial_layout_t *layout, i32 i, f32 height);
bool point_in_radial_segment(f32 px, f32 py, radial_layout_t *layout, radial_segment_t *seg);

void draw_radial_segment_filled(image_view_t *img,
                                radial_layout_t *layout, 
                                 radial_segment_t *seg,
                                 color4_t color) ;
void catmull_rom_vertex(f32 t, vertex_t p[4], vertex_t *out);

path_data_t * path_new(void);
void add_bezier_segment(path_data_t *path, vec2f_t p0, vec2f_t p1, vec2f_t p2, vec2f_t p3, bool is_cubic, bool is_linear); 

image_view_t* load_texture(const char *filepath);
void free_texture(image_view_t *texture);
void render_texture_to_buffer(image_view_t const *color_buf, image_view_t const *texture, 
                               u32 dst_x, u32 dst_y, f32 scale, rect_t *out_rect);
#endif