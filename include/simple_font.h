#ifndef BASE_S_FONT
#define BASE_S_FONT

/*
    Quick way to get debug ascii text on screen without having 
    to deal with any external library or font files
 */

#include "util.h"
#include "base_graphics.h"

#define FONT_ROWS           6   
#define FONT_COLS           18

#define ASCII_LOW           32
#define ASCII_HIGH          126

extern const uint8_t font_pixels[];

typedef struct
{
    u32 const       *font_pixels;
    u32              font_width;
    u32              font_height;
    u32              font_char_width;
    u32              font_char_height;
    rect_t           glyph_table[ASCII_HIGH - ASCII_LOW + 1];
}simple_font_t;

typedef struct
{
    simple_font_t    *font;
    char             *string;
    u32              size;
    vec2_t           pos;       
    color4_t         color;     
    u32              scale;     
}rendered_text_t;

void render_glyph_to_buffer(rendered_text_t *text, u32 glyph_idx,
                               image_view_t const *color_buf,
                               u32 dst_x, u32 dst_y);
void render_text(image_view_t const *color_buf, rendered_text_t *text);
simple_font_t* init_simple_font(u32 *font_pixels);

#endif