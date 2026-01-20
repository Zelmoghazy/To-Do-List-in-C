#ifndef FONT_H_
#define FONT_H_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "base_graphics.h"
#include "util.h"

#include "stb_truetype.h"

#define STAR_ICON           u8"\uf005"
#define CALENDAR_ICON       u8"\ueab0"
#define TIME_ICON           u8"\ue641"
#define NOTE_ICON           u8"\ueb26"
#define PLUS_ICON           u8"\uf067"
#define CHECK_ICON          u8"\uf05d"
#define CLOSE_ICON          u8"\uf00d"
#define CHECK_LIST_ICON     u8"\uf0ae"
#define ADD_ICON            u8"\ueadc"
#define REMOVE_ICON         u8"\uf00d"
#define EDIT_ICON           u8"\uf044"
#define DOWN_ARROW          u8"\uf063"
#define UP_ARROW            u8"\uf062"
#define RIGHT_CIRCLE_ARROW  u8"\uf0a9"
#define DOWN_CIRCLE_ARROW  u8"\uf0ab"

#define GLYPH_CACHE_SIZE 120

typedef struct 
{
    u32 x, y, w, h;            
    i32 bearing_x, bearing_y;
    f32 advance;
    u8 *bitmap;
} glyph_info_t;

typedef struct 
{
    stbtt_fontinfo info;
    u8 *font_buffer;
    f32 scale;
    i32 ascent, descent, line_gap;
    
    glyph_info_t glyph_cache[GLYPH_CACHE_SIZE];
    u8 cache_valid[GLYPH_CACHE_SIZE];
} font_tt;

typedef struct 
{
    font_tt *font;
    char *string;
    vec2_t pos;
    color4_t color;
} rendered_text_tt;

typedef struct 
{
    const char *str;
    size_t pos;
} utf8_decoder_t;

font_tt* load_font_from_file(const char *filename, f32 font_size);
font_tt* init_font_tt(u8 *font_buffer, f32 font_size);
void free_font(font_tt *font);
glyph_info_t* get_glyph(font_tt *font, u32 codepoint);
void render_glyph_to_buffer_tt(font_tt *font, u32 codepoint, 
                           image_view_t *color_buf,
                           u32 dst_x, u32 dst_y, color4_t color);
void render_glyph_to_buffer_tt_scissored(font_tt *font, u32 codepoint, 
                                         image_view_t  *color_buf,
                                         u32 dst_x, u32 dst_y, color4_t color);
float get_string_width(font_tt *font, const char *text);
float get_char_width(font_tt *font, char c);
int get_line_height(font_tt *font);
void wrap_text(font_tt *font, const char *text, float max_width, char *out, size_t out_len, int *line_n, float *max_line_w);

void render_text_tt(image_view_t *color_buf, rendered_text_tt *text);
void render_text_tt_scissored(image_view_t *color_buf, rendered_text_tt *text);
void render_text_simple(image_view_t *color_buf, font_tt *font, const char *text, float x, float y, color4_t color);
void utf8_decoder_init(utf8_decoder_t *decoder, const char *str);
int utf8_decode_next(utf8_decoder_t *decoder, u32 *out_char);
void init_unicode_support();
float get_string_width_unicode(font_tt *font, const char *utf8_str, float scale);
void render_string_unicode(image_view_t const *color_buf, rendered_text_tt *text);


#endif