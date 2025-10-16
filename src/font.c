#include "font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// Load font from file
font_tt* load_font_from_file(const char *filename, f32 font_size)
{
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open font file: %s\n", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    u8 *buffer = malloc(size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    fread(buffer, 1, size, file);
    fclose(file);
    
    return init_font_tt(buffer, font_size);
}

// Initialize font from memory buffer
font_tt* init_font_tt(u8 *font_buffer, f32 font_size)
{
    font_tt *font = CHECK_PTR(malloc(sizeof(font_tt)));
    
    memset(font, 0, sizeof(font_tt));
    font->font_buffer = font_buffer;
    
    if (!stbtt_InitFont(&font->info, font_buffer, 0)) 
    {
        fprintf(stderr, "Failed to initialize font\n");
        free(font);
        return NULL;
    }
    
    font->scale = stbtt_ScaleForPixelHeight(&font->info, font_size);
    stbtt_GetFontVMetrics(&font->info, &font->ascent, &font->descent, &font->line_gap);
    
    font->ascent = (int)(font->ascent * font->scale);
    font->descent = (int)(font->descent * font->scale);
    font->line_gap = (int)(font->line_gap * font->scale);
    
    return font;
}

glyph_info_t* get_glyph(font_tt *font, u32 codepoint)
{
    int cache_index = codepoint % GLYPH_CACHE_SIZE;
    
    // check if it exists in the cache first
    if (font->cache_valid[cache_index]) {
        return &font->glyph_cache[cache_index];
    }
    
    glyph_info_t *glyph = &font->glyph_cache[cache_index];
    
    if (glyph->bitmap) {
        free(glyph->bitmap);
        glyph->bitmap = NULL;
    }

    int glyph_index = stbtt_FindGlyphIndex(&font->info, codepoint);
    if (glyph_index == 0) {
        codepoint = '?';
        glyph_index = stbtt_FindGlyphIndex(&font->info, codepoint);
    }
    
    int advance, left_bearing;
    stbtt_GetCodepointHMetrics(&font->info, codepoint, &advance, &left_bearing);
    
    glyph->advance = advance * font->scale;
    
    int w, h, xoff, yoff;
    u8 *bitmap = stbtt_GetGlyphBitmap(&font->info, 0, font->scale,
                                      glyph_index, &w, &h, &xoff, &yoff);
    
    if (bitmap == NULL) {
        glyph->w = glyph->h = 0;
        glyph->bitmap = NULL;
        return glyph;
    }

    glyph->w = w;
    glyph->h = h;
    glyph->bearing_x = xoff;
    glyph->bearing_y = yoff;
    glyph->bitmap = bitmap;
    
    font->cache_valid[cache_index] = 1;
    
    return glyph;
}

void render_glyph_to_buffer_tt(font_tt *font, u32 codepoint, 
                           image_view_t *color_buf,
                           u32 dst_x, u32 dst_y, color4_t color)
{
    glyph_info_t *glyph = get_glyph(font, codepoint);
    if (!glyph || !glyph->bitmap){
        return;
    }
    
    int render_x = (int)dst_x + (int)glyph->bearing_x;
    int render_y = (int)dst_y + font->ascent + (int)glyph->bearing_y;
    
    rect_t glyph_rect = { render_x, render_y, (int)glyph->w, (int)glyph->h };
    rect_t buffer_rect = { 0, 0, (int)color_buf->width, (int)color_buf->height };

    // bound checking
    rect_t clipped = intersect_rects(&glyph_rect, &buffer_rect);

    int x_start = clipped.x;
    int y_start = clipped.y;

    int x_end = clipped.x + clipped.w;
    int y_end = clipped.y + clipped.h;
    
    for (int y = y_start; y < y_end; y++) 
    {
        for (int x = x_start; x < x_end; x++) 
        {
            int src_x = (int)(x - render_x);
            int src_y = (int)(y - render_y);
            
            if (src_x >= 0 && src_x < (int)glyph->w && 
                src_y >= 0 && src_y < (int)glyph->h) 
            {
                
                u8 alpha = glyph->bitmap[src_y * glyph->w + src_x];
                
                if (alpha > 0) 
                {
                    color.a = alpha;
                    draw_pixel(color_buf, x, y, color);
                }
            }
        }
    }
}

void render_glyph_to_buffer_tt_scissored(font_tt *font, u32 codepoint, 
                                         image_view_t  *color_buf,
                                         u32 dst_x, u32 dst_y, color4_t color)
{
    glyph_info_t *glyph = get_glyph(font, codepoint);

    if (!glyph || !glyph->bitmap){
        return;
    }
    
    int render_x = (int)dst_x + (int)glyph->bearing_x;
    int render_y = (int)dst_y + font->ascent + (int)glyph->bearing_y;

    rect_t glyph_rect = {render_x, render_y, (int)glyph->w, (int)glyph->h };
    rect_t buffer_rect = { 0, 0, (int)color_buf->width, (int)color_buf->height};

    // Intersect all three
    rect_t clipped = intersect_rects(&glyph_rect, &buffer_rect);
    clipped = intersect_rects(&clipped, &current_scissor);

    if (clipped.w <= 0 || clipped.h <= 0)
        return;

    int x_start = clipped.x;
    int y_start = clipped.y;

    int x_end = clipped.x + clipped.w;
    int y_end = clipped.y + clipped.h;

    for (int y = y_start; y < y_end; y++)
    {
        for (int x = x_start; x < x_end; x++)
        {
            int src_x = (x - render_x);
            int src_y = (y - render_y);

            if (src_x >= 0 && src_x < (int)glyph->w && 
                src_y >= 0 && src_y < (int)glyph->h) 
            {
                u8 alpha = glyph->bitmap[src_y * glyph->w + src_x];
                
                if (alpha > 0) 
                {
                    color.a = alpha;
                    draw_pixel(color_buf, x, y, color);
                }
            }
        }
    }
}

void render_text_tt(image_view_t *color_buf, rendered_text_tt *text)
{
    if (!text || !text->string || !text->font){
        return;
    }
    
    char *string = text->string;
    f32 x = text->pos.x;
    f32 y = text->pos.y;
    f32 original_x = x;
    
    i32 line_height = get_line_height(text->font);
    
    while (*string) 
    {
        if (*string == '\n') 
        {
            y += line_height ;
            x = original_x;
            string++;
            continue;
        }
        
        if (*string == '\t') 
        {
            glyph_info_t *space_glyph = get_glyph(text->font, ' ');
            if (space_glyph) {
                x += space_glyph->advance * TAB_SIZE;
            }
            string++;
            continue;
        }
        
        render_glyph_to_buffer_tt(text->font, *string, color_buf, (u32)x, (u32)y, text->color);
        
        glyph_info_t *glyph = get_glyph(text->font, *string);
        if (glyph) {
            x += glyph->advance ;
        }
        string++;
    }
}

void render_text_tt_scissored(image_view_t *color_buf, rendered_text_tt *text)
{
    if (!text || !text->string || !text->font){
        return;
    }
    
    char *string = text->string;
    f32 x = text->pos.x;
    f32 y = text->pos.y;
    f32 original_x = x;
    
    i32 line_height = get_line_height(text->font);
    
    while (*string) 
    {
        if (*string == '\n') 
        {
            y += line_height ;
            x = original_x;
            string++;
            continue;
        }
        
        if (*string == '\t') 
        {
            glyph_info_t *space_glyph = get_glyph(text->font, ' ');
            if (space_glyph) {
                x += space_glyph->advance * TAB_SIZE;
            }
            string++;
            continue;
        }
        
        render_glyph_to_buffer_tt_scissored(text->font, *string, color_buf, (u32)x, (u32)y, text->color);
        
        glyph_info_t *glyph = get_glyph(text->font, *string);
        if (glyph) {
            x += glyph->advance ;
        }
        string++;
    }
}

void render_text_simple(image_view_t *color_buf, font_tt *font, 
                       const char *text, float x, float y, color4_t color)
{
    rendered_text_tt render_info = {
        .font = font,
        .string = (char*)text,
        .pos = {x, y},
        .color = color
    };
    
    render_text_tt(color_buf, &render_info);
}

float get_char_width(font_tt *font, char c)
{
    glyph_info_t *glyph = get_glyph(font, c);
    if(!glyph)
        return 0.0f;

    return glyph->advance;
}

float get_string_width(font_tt *font, const char *text)
{
    if (!text || !font) return 0.0f;
    
    float width = 0.0f;
    const char *p = text;
    
    while (*p) 
    {
        if (*p == '\n') {
            break; // Only measure until newline
        }
        
        width += get_char_width(font, *p);
        p++;
    }
    
    return width;
}

int get_line_height(font_tt *font)
{
    return font->ascent - font->descent + font->line_gap;
}


void utf8_decoder_init(utf8_decoder_t *decoder, const char *str)
{
    decoder->str = str;
    decoder->pos = 0;
}

int utf8_decode_next(utf8_decoder_t *decoder, u32 *out_char)
{
    const char *s = decoder->str + decoder->pos;
    if (*s == '\0') return 0;
    
    unsigned char c = (unsigned char)*s;
    u32 codepoint;
    int bytes;
    
    if (c < 0x80) {
        // 1-byte sequence
        codepoint = c;
        bytes = 1;
    } else if ((c & 0xE0) == 0xC0) {
        // 2-byte sequence
        codepoint = c & 0x1F;
        bytes = 2;
    } else if ((c & 0xF0) == 0xE0) {
        // 3-byte sequence
        codepoint = c & 0x0F;
        bytes = 3;
    } else if ((c & 0xF8) == 0xF0) {
        // 4-byte sequence
        codepoint = c & 0x07;
        bytes = 4;
    } else {
        // Invalid UTF-8
        *out_char = '?';
        decoder->pos++;
        return 1;
    }
    
    // Decode continuation bytes
    for (int i = 1; i < bytes; i++) 
    {
        c = (unsigned char)s[i];
        if ((c & 0xC0) != 0x80) {
            // Invalid continuation byte
            *out_char = '?';
            decoder->pos++;
            return 1;
        }
        codepoint = (codepoint << 6) | (c & 0x3F);
    }
    
    // Check for overlong encoding
    if ((bytes == 2 && codepoint < 0x80) ||
        (bytes == 3 && codepoint < 0x800) ||
        (bytes == 4 && codepoint < 0x10000)) 
    {
        *out_char = '?';
        decoder->pos++;
        return 1;
    }
    
    // Check for invalid codepoints
    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) 
    {
        *out_char = '?';
        decoder->pos += bytes;
        return 1;
    }
    
    *out_char = codepoint;
    decoder->pos += bytes;
    return bytes;
}

void init_unicode_support()
{
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
    #else
        setlocale(LC_ALL, "en_US.UTF-8");
    #endif
}

float get_string_width_unicode(font_tt *font, const char *utf8_str, float scale)
{
    if (!utf8_str || !font) return 0.0f;
    
    float width = 0.0f;
    utf8_decoder_t decoder;
    utf8_decoder_init(&decoder, utf8_str);
    
    u32 wc;
    while (utf8_decode_next(&decoder, &wc)) {
        if (wc == '\n') {
            break;
        }
        
        glyph_info_t *glyph = get_glyph(font, wc);
        if (glyph) {
            width += glyph->advance * scale;
        }
    }
    
    return width;
}

void render_string_unicode(image_view_t const *color_buf, rendered_text_tt *text)
{
    if (!text || !text->string || !text->font) return;
    
    const char *utf8_str = text->string;
    float x = text->pos.x;
    float y = text->pos.y;
    float original_x = x;
    
    int line_height = get_line_height(text->font);
    utf8_decoder_t decoder;
    utf8_decoder_init(&decoder, utf8_str);
    
    u32 wc;
    while (utf8_decode_next(&decoder, &wc)) 
    {
        if (wc == '\n') {
            y += line_height;
            x = original_x;
            continue;
        }
        
        if (wc == '\t') {
            glyph_info_t *space_glyph = get_glyph(text->font, ' ');
            if (space_glyph) {
                x += space_glyph->advance  * 4;
            }
            continue;
        }
        
        render_glyph_to_buffer_tt(text->font, wc, color_buf, 
                                 (u32)x, (u32)y, text->color);
        
        glyph_info_t *glyph = get_glyph(text->font, wc);
        if (glyph) {
            x += glyph->advance;
        }
    }
}

size_t utf8_strlen(const char *utf8_str)
{
    if (!utf8_str) return 0;
    
    size_t count = 0;
    utf8_decoder_t decoder;
    utf8_decoder_init(&decoder, utf8_str);
    
    u32 wc;
    while (utf8_decode_next(&decoder, &wc)) {
        count++;
    }
    
    return count;
}

void free_font(font_tt *font)
{
    if (!font) return;
    
    for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
        if (font->cache_valid[i] && font->glyph_cache[i].bitmap) {
            free(font->glyph_cache[i].bitmap);
        }
    }
    
    if (font->font_buffer) {
        free(font->font_buffer);
    }
    
    free(font);
}
