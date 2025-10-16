#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
    #include "./external/include/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_WRITE_IMPLEMENTATION
    #include "./external/include/stb_image_write.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION

#define STB_DS_IMPLEMENTATION
    #include "./external/include/stb_ds.h"
#undef STB_DS_IMPLEMENTATION 

#define PROF_IMPLEMENTATION
#include "./include/util.h"
#include "./include/arena.h"
#include "./include/base_graphics.h"
#include "./include/prof.h"
#include "./include/font.h"
#include "./include/todo.h"

#define FRAME_HISTORY_SIZE  64
#define BUFFER_SIZE         512
#define MAX_PANELS          256

/*
    Stuff that we wish to retain between frames
*/
typedef struct 
{
    i32 scroll_start_x, scroll_start_y;
    i32 scroll_x, scroll_y;

    bool dragging;
    f32 drag_start_x, drag_start_y;
} panel_state_t;

typedef struct 
{
    f32  value;
    bool hover;
    bool clicked;
    bool toggled;
    bool released;
    bool dragging;
    f32 drag_delta_x, drag_delta_y;
} block_state_t;

typedef struct { char *key; panel_state_t value; } panel_state_map_t;
typedef struct { char *key; block_state_t value; } block_state_map_t;

typedef enum
{
    VERTICAL_LAYOUT,
    HORIZONTAL_LAYOUT
}layout_t;

typedef struct block_t block_t;
typedef struct panel_t panel_t;

struct panel_t
{
    // also used as ID to uniquely identify the panel
    // Ensure that it is unique !
    char *title;

    // Each panel has a single layout option 
    // so its easier to calculate later
    layout_t layout;

    i32 x, y;                    
    i32 w, h;

    // Panels can be scrolled so they can contain
    // stuff beyond its dimensions
    i32 content_width, content_height;

    i32 child_spacing;
    i32 padding_top, padding_bottom, padding_left, padding_right;

    i32 default_child_height, default_child_width;

    // stuff inside the panel should only be drawn inside the panel
    scissor_region_t scissor_region;

    i32 scroll_x, scroll_y; 
    i32 max_scroll_x, max_scroll_y;
    
    color4_t bg_color;

    color4_t border_color;
    i32 border_width;

    bool visible;
    bool scrollable;
    
    // a panel can contain inside it another panels
    // as well as blocks we need to keep track of them
    // so we can calculate the size correctly

    struct panel_t *parent;
    struct panel_t *children;
    struct panel_t *next_sibling;
    
    i32 child_count;

    block_t *blocks;
    
    // Interaction state
    bool hover;
    bool dragging;
    f32 drag_start_x, drag_start_y;
    i32 scroll_start_x, scroll_start_y;
    
    i32 id;
};

/*
    a block is the basic unit of all widgets
    it should provide all necessary information 
    required to create more complex widgets
*/
struct block_t
{
    // also used as id in the map so must be unique
    char *title; 

    // all the information needed to do generic layout of the block
    // everything is a rect if you think about it 
    i32 x, y;
    i32 w, h;

    // style
    color4_t color;
    color4_t border_color;
    color4_t hover_color;

    rendered_text_tt text;

    // all the possible interactions and states block can have
    // stuff not needed can simply be ignored
    bool visible;
    bool enabled;

    bool hover;

    bool clickable;
    bool clicked;
    bool released;

    bool draggable;
    bool dragging;

    bool toggleable;
    bool toggled;

    f32 drag_start_x, drag_start_y;
    f32 drag_delta_x, drag_delta_y;

    f32 value;
    f32 min_value;
    f32 max_value;

    // pass functions to control how the block is rendered
    void (*custom_render)(struct panel_t *panel, struct block_t *block);
    void (*custom_update)(struct panel_t *panel, struct block_t *block);

    // if you want to add custom stuff
    void *user_data;
};

typedef struct ui_context_t
{
    panel_t *root_panel;
    panel_t *active_panel;          
    panel_t *panels[MAX_PANELS];    
    i32 panel_count;
    i32 next_panel_id;
} ui_context_t;

#define TEXT_BUFFER_MAX 512

typedef struct 
{
    char buffer [TEXT_BUFFER_MAX];
    i32 buffer_len;
    i32 cursor_pos;

    // ---- 
    i32 selection_start;
    i32 selection_end;

    i32 scroll_offset;
    f64 cursor_blink_timer;
    bool cursor_visible;
    bool focused;

    bool mouse_selecting;

    bool dragging;
    f32 drag_start_x;
}text_edit_state_t;

typedef struct { char *key; text_edit_state_t value; } text_edit_map_t;

typedef struct 
{
    u32 start_x, end_x;
    u32 start_y, end_y;
    u32 width, height;
} tile_data_t;

struct context_t
{
    void               *window;
    u32                 screen_width;
    u32                 screen_height;
    image_view_t        draw_buffer;

    GLFWcursor          *hand_cursor;
    GLFWcursor          *regular_cursor;

    GLuint              texture;
    GLuint              read_fbo;

    i32                 mouse_x;
    i32                 mouse_last_x;
    i32                 mouse_y;
    i32                 mouse_last_y;
    bool                first_mouse;
    bool                left_button_down;
    bool                right_button_down;

    ui_context_t        *ui_ctx;
    panel_state_map_t   *state_map;
    block_state_map_t   *block_map;
    text_edit_map_t     *text_map;

    char                *focused_text_edit;

    f32                 side_panel_x;
    i32                 curr_list;

    /* Text */
    font_tt             *font;
    font_tt             *icon_font;

    /* FLAGS */
    bool                running;
    bool                resize;
    bool                rescale;
    bool                render;
    bool                dock;
    bool                capture;
    bool                profile;
    bool                changed;

    /* TIME */
    i32                 start_time;
    f64                 dt;
    #ifdef _WIN32
        LARGE_INTEGER   last_frame_start;
    #else
        struct timespec last_frame_start;
    #endif
    f64                 frame_history[FRAME_HISTORY_SIZE];
    i32                 frame_idx;
    f64                 average_frame_time;
    f64                 current_fps;

    arena_t             *frame_arena;
}gc;

char frametime[BUFFER_SIZE];

char prof_buf[64][BUFFER_SIZE];
i32 prof_buf_count;
i32 prof_max_width;

bool key_pressed[GLFW_KEY_LAST];
bool key_just_pressed[GLFW_KEY_LAST];

char input_char_queue[32];
i32 input_char_count = 0;

void render_all(void);
void scroll_panel(panel_t *panel, i32 delta_x, i32 delta_y);
void handle_panel_mouse_move(ui_context_t *ctx, f32 mouse_x, f32 mouse_y);
void handle_panel_mouse_button(ui_context_t *ctx);
void handle_panel_scroll(ui_context_t *ctx, f32 scroll_x, f32 scroll_y);
void block_abs_postion(panel_t *panel, block_t *block, i32 *abs_x, i32 *abs_y);
void block_load_state(block_t *block);
void block_save_state(block_t *block);
void update_block_interaction(panel_t *panel, block_t *block);
void render_block(panel_t *panel, block_t *block);


void set_dark_mode(GLFWwindow *window)
{
    #ifdef _WIN32
        HWND hwnd = glfwGetWin32Window(window);
        if (!hwnd) return;
        BOOL value = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    #endif
}

void set_window_icon(char *path)
{
    // Load and set window icon
    GLFWimage icon;
    icon.pixels = stbi_load(path, &icon.width, &icon.height, NULL, 4);
    if (icon.pixels) {
        glfwSetWindowIcon(gc.window, 1, &icon);
        stbi_image_free(icon.pixels);
    } else {
        fprintf(stderr, "Failed to load window icon\n");
    }
}

void prof_record_results(void)
{
    for (int i = 0; i < g_prof_storage.count; i++) 
    {
        if (g_prof_storage.entries[i].hit_count > 0) 
        {
            snprintf(prof_buf[i], BUFFER_SIZE, "[PROFILE] %s[%llu]: %.6f ms (total)", 
                   g_prof_storage.entries[i].label,
                   (u64)g_prof_storage.entries[i].hit_count,
                   g_prof_storage.entries[i].elapsed_ms);

            i32 text_length = (i32)strlen(prof_buf[i]);
            if(text_length > prof_max_width){
                prof_max_width = text_length;
            }
            
            prof_buf_count++;
        }
    }
}

void render_prof_entries(void)
{
    prof_max_width = 0;

    i32 height = get_line_height(gc.font)*prof_buf_count;
    i32 width = 0;

    for(int i = 0; i < prof_buf_count; i++)
    {
        width = MAX(width, get_string_width(gc.font, prof_buf[i]));
    }

    draw_rect_solid_wh(&gc.draw_buffer, 0, 0, width+10, height+10, HEX_TO_COLOR4(0x282a36));

    for(int i = 0; i < prof_buf_count; i++)
    {
        rendered_text_tt text = {
            .font = gc.font,
            .pos = {.x=0,.y=i*get_line_height(gc.font)},
            .color = COLOR_WHITE,
            .string = prof_buf[i]
        };
        render_text_tt(&gc.draw_buffer, &text);
    }
}

void reset_input_for_frame(void) 
{
    for (i32 i = 0; i < GLFW_KEY_LAST; i++) {
        key_just_pressed[i] = false;
    }
    input_char_count = 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    (void)window;
    gc.screen_width  = width;
    gc.screen_height = height;

    glViewport(0, 0, gc.screen_width, gc.screen_height);
}

void error_callback(int error, const char* description) 
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void window_refresh_callback(GLFWwindow* window)
{
    glfwSwapBuffers(window);
}

void get_mouse_delta(f32 *xoffset, f32 *yoffset)
{
    if (gc.first_mouse)
    {
        gc.mouse_last_x = gc.mouse_x;
        gc.mouse_last_y = gc.mouse_y;
        gc.first_mouse = false;
    }

    *xoffset = (f32)gc.mouse_x - (f32)gc.mouse_last_x;
    *yoffset = (f32)gc.mouse_last_y - (f32)gc.mouse_y; // reversed since y-coordinates range from bottom to top

    gc.mouse_last_x = gc.mouse_x;
    gc.mouse_last_y = gc.mouse_y;
}

void mouse_callback(GLFWwindow* window, f64 xpos, f64 ypos)
{
    (void)window;

    gc.mouse_x = (u32)xpos;
    gc.mouse_y = (u32)ypos;

    f32 xoffset, yoffset;
    get_mouse_delta(&xoffset, &yoffset);

    handle_panel_mouse_move(gc.ui_ctx, xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) 
{
    (void)window;
    (void)xoffset;
    (void)yoffset;

    f32 scroll_sensitivity = 25.0f;
    f32 scaled_x = (f32)xoffset * scroll_sensitivity;
    f32 scaled_y = (f32)yoffset * scroll_sensitivity;

    handle_panel_scroll(gc.ui_ctx, scaled_x, -scaled_y);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    (void)window;
    (void)mods;
    
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        gc.left_button_down = (action == GLFW_PRESS);
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        gc.right_button_down = (action == GLFW_PRESS);
    }

    handle_panel_mouse_button(gc.ui_ctx);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) 
{
    (void)window;
    (void)scancode;
    (void)mods;

    if (action == GLFW_PRESS) 
    {
        key_pressed[key] = true;
        key_just_pressed[key] = true;
    } 
    else if (action == GLFW_RELEASE) 
    {
        key_pressed[key] = false;
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) 
    {
        switch (key) {
            case GLFW_KEY_BACKSPACE:
                break;
            case GLFW_KEY_ENTER:
                break;
            case GLFW_KEY_TAB:
                break;
            case GLFW_KEY_F11:
                gc.profile ^= 1;
                break;
            case GLFW_KEY_F10:
                gc.capture = true;
                break;
            case GLFW_KEY_F7:
                gc.dock = !gc.dock;
                if(gc.dock){
                    animation_start((u64)&gc.side_panel_x, gc.side_panel_x, 500, 1, EASE_IN_OUT_CUBIC);
                }else{
                    animation_start((u64)&gc.side_panel_x, gc.side_panel_x, 10, 1, EASE_IN_OUT_CUBIC);
                }
                break;
            case GLFW_KEY_W:
                break;
            case GLFW_KEY_S:
                break;
            case GLFW_KEY_D:
                break;
            case GLFW_KEY_A:
                break;
            case GLFW_KEY_I:
                break;
            case GLFW_KEY_K:
                break;
            case GLFW_KEY_UP:
                gc.side_panel_x += 10;
                break;
                case GLFW_KEY_DOWN:
                gc.side_panel_x -= 10;
                break;
            case GLFW_KEY_1:
                break;
            case GLFW_KEY_2:
                break;
            case GLFW_KEY_3:
                break;
            case GLFW_KEY_4:
                break;
            default:
                break;
        }
    }
}

void char_callback(GLFWwindow* window, unsigned int codepoint) 
{
    (void)window;

    if (codepoint >= 32 && codepoint <= 126) {
        if (input_char_count < 31) {
            input_char_queue[input_char_count++] = (char)codepoint;
        }
    }
}

void poll_events(void)
{
    glfwPollEvents();
}

/* Panel stuff */
panel_t* create_panel(ui_context_t *ctx, char *title, i32 x, i32 y, u32 width, u32 height, layout_t layout)
{
    if (ctx->panel_count >= MAX_PANELS)
    {
        return NULL;
    } 
    panel_t *panel = ARENA_ALLOC_ZEROED(gc.frame_arena, 1, sizeof(panel_t));

    panel->title = title;
    panel->layout = layout;
    panel->blocks = NULL;

    panel->x = panel->scissor_region.x = x;
    panel->y = panel->scissor_region.y = y;
    panel->w = panel->scissor_region.w = width;
    panel->h = panel->scissor_region.h = height;

    panel->content_width = (i32)width;
    panel->content_height = (i32)height;
    panel->bg_color = HEX_TO_COLOR4(0x282a36);
    panel->border_color = HEX_TO_COLOR4(0x3a3d4e);

    panel->border_width = 5;
    panel->child_spacing = 15;
    panel->padding_top = panel->border_width + panel->child_spacing;
    panel->padding_bottom = panel->padding_left = panel->padding_right = panel->padding_top;

    panel->default_child_height = 50;
    panel->default_child_width = panel->w - panel->padding_left - panel->padding_right;

    panel->visible = true;
    panel->scrollable = true;
    panel->id = ctx->next_panel_id++;
    
    ctx->panels[ctx->panel_count] = panel;
    ctx->panel_count++;

    panel_state_map_t *state = shgetp_null(gc.state_map, title);

    // get cached values
    if(state)
    {
        panel->scroll_x          = state->value.scroll_x;
        panel->scroll_y          = state->value.scroll_y;
        panel->dragging          = state->value.dragging;
        panel->drag_start_x      = state->value.drag_start_x;
        panel->drag_start_y      = state->value.drag_start_y;
        panel->scroll_start_x    = state->value.scroll_start_x;
        panel->scroll_start_y    = state->value.scroll_start_y;
    }
    else
    {
        panel_state_t state = {
            .scroll_x=0,
            .scroll_y=0,
            .dragging = false,
            .drag_start_x = 0,
            .drag_start_y = 0,
            .scroll_start_x = 0,
            .scroll_start_y = 0
        };
    
        shput(gc.state_map, title, state);
    }
    
    return panel;
}

#define LIST_FOR_EACH(pos, head) \
    for ((pos) = (head); (pos) != NULL; (pos) = (pos)->parent)

/* 
    parent->child->child->child...
    child->parent
 */
void add_child_panel(panel_t *parent, panel_t *child)
{
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    parent->child_count++;
}

void set_panel_content_size(panel_t *panel, i32 content_width, i32 content_height)
{
    panel->content_width = content_width;
    panel->content_height = content_height;
    
    /* 
        Maximum scroll area is the difference between actual content size 
        and the viewport size so that in a sense maximum scroll = all content
    */
    panel->max_scroll_x = (content_width > (i32)panel->w) ? 
                         content_width - (i32)panel->w : 0;
    panel->max_scroll_y = (content_height > (i32)panel->h) ? 
                         content_height - (i32)panel->h : 0;
    
    panel->scroll_x = Clamp(0, panel->scroll_x, panel->max_scroll_x);
    panel->scroll_y = Clamp(0, panel->scroll_y, panel->max_scroll_y);
}

void panel_to_screen_coords(panel_t *panel, i32 panel_x, i32 panel_y, i32 *screen_x, i32 *screen_y)
{
    *screen_x = panel_x - panel->scroll_x;
    *screen_y = panel_y - panel->scroll_y;
    
    // any drawing inside a panel is relative to it 
    // so we have to calculate its screen position when needed
    panel_t *current = NULL;
    LIST_FOR_EACH(current, panel)
    {
        *screen_x += current->x;
        *screen_y += current->y;
    }
}

void screen_to_panel_coords(panel_t *panel, i32 screen_x, i32 screen_y, i32 *panel_x, i32 *panel_y)
{
    *panel_x = screen_x;
    *panel_y = screen_y;
    
    // Walk up the parent chain to subtract parent offsets
    panel_t *current = NULL;
    LIST_FOR_EACH(current, panel)
    {
        *panel_x -= current->x;
        *panel_y -= current->y;
        current = current->parent;
    }
    
    // Add scroll offset
    *panel_x += panel->scroll_x;
    *panel_y += panel->scroll_y;
}

void panel_abs_pos(panel_t *panel, i32 *abs_x, i32 *abs_y)
{
    *abs_x = panel->x;
    *abs_y = panel->y;

    panel_t *current = NULL;

    LIST_FOR_EACH(current, panel->parent)
    {
        abs_x += current->x;
        abs_y += current->y;
        current = current->parent;
    }
}

bool point_in_panel(panel_t *panel, i32 screen_x, i32 screen_y)
{
    if (!panel->visible) return false;
    
    i32 abs_x = 0;
    i32 abs_y = 0;

    panel_abs_pos(panel, &abs_x, &abs_y);

    rect_t panel_screen_rect = {
        .x = abs_x,
        .y = abs_y,
        .w = (i32)panel->w,
        .h = (i32)panel->h
    };

    return inside_rect(screen_x, screen_y, &panel_screen_rect);
}

panel_t* find_panel_at_point(ui_context_t *ctx, i32 screen_x, i32 screen_y)
{
    for (i32 i = (i32)ctx->panel_count - 1; i >= 0; i--) 
    {
        if (point_in_panel(ctx->panels[i], screen_x, screen_y)) {
            return ctx->panels[i];
        }
    }
    return NULL;
}

void set_pixel_in_panel(panel_t *panel, i32 panel_x, i32 panel_y, color4_t color)
{
    i32 screen_x, screen_y;
    panel_to_screen_coords(panel, panel_x, panel_y, &screen_x, &screen_y);
    
    i32 panel_screen_x = 0;
    i32 panel_screen_y = 0;

    panel_abs_pos(panel, &panel_screen_x, &panel_screen_y);

    panel->scissor_region.x = panel_screen_x;
    panel->scissor_region.y = panel_screen_y;

    push_scissor(panel->scissor_region.x,
                 panel->scissor_region.y,
                 panel->scissor_region.w,
                 panel->scissor_region.h);

        set_pixel_scissored(&gc.draw_buffer, screen_x, screen_y, color);

    pop_scissor();
}

void draw_rect_in_panel(panel_t *panel, i32 panel_x, i32 panel_y, u32 width, u32 height, color4_t color)
{
    i32 screen_x, screen_y;
    panel_to_screen_coords(panel, panel_x, panel_y, &screen_x, &screen_y);
    
    i32 panel_screen_x = 0;
    i32 panel_screen_y = 0;

    panel_abs_pos(panel, &panel_screen_x, &panel_screen_y);

    panel->scissor_region.x = panel_screen_x;
    panel->scissor_region.y = panel_screen_y;

    push_scissor(panel->scissor_region.x,
                 panel->scissor_region.y,
                 panel->scissor_region.w,
                 panel->scissor_region.h);

        draw_rect_scissored(&gc.draw_buffer, screen_x, screen_y, width, height, color);
        
    pop_scissor();
}

void draw_rounded_rect_in_panel(panel_t *panel, i32 panel_x, i32 panel_y, u32 width, u32 height, color4_t color)
{
    i32 screen_x, screen_y;
    panel_to_screen_coords(panel, panel_x, panel_y, &screen_x, &screen_y);
    
    i32 panel_screen_x = 0;
    i32 panel_screen_y = 0;

    panel_abs_pos(panel, &panel_screen_x, &panel_screen_y);

    panel->scissor_region.x = panel_screen_x;
    panel->scissor_region.y = panel_screen_y;

    push_scissor(panel->scissor_region.x,
                 panel->scissor_region.y,
                 panel->scissor_region.w,
                 panel->scissor_region.h);

        draw_rounded_rect_scissored(&gc.draw_buffer, screen_x, screen_y, width, height, 25, color);

    pop_scissor();
}

void render_text_in_panel(panel_t *panel, block_t *block)
{
    i32 screen_x, screen_y;
    panel_to_screen_coords(panel, block->text.pos.x, block->text.pos.y, &screen_x, &screen_y);
    
    i32 panel_screen_x = 0;
    i32 panel_screen_y = 0;

    panel_abs_pos(panel, &panel_screen_x, &panel_screen_y);

    panel->scissor_region.x = panel_screen_x;
    panel->scissor_region.y = panel_screen_y;

    push_scissor(panel->scissor_region.x,
                 panel->scissor_region.y,
                 panel->scissor_region.w,
                 panel->scissor_region.h);

        block->text.pos.x = screen_x;
        block->text.pos.y = screen_y;

        render_text_tt_scissored(&gc.draw_buffer, &block->text);

    pop_scissor();
}

void draw_panel_background(panel_t *panel)
{
    if (!panel->visible) return;
    
    i32 panel_screen_x = 0;
    i32 panel_screen_y = 0;

    panel_abs_pos(panel, &panel_screen_x, &panel_screen_y);
    
    panel->scissor_region.x = panel_screen_x;
    panel->scissor_region.y = panel_screen_y;

    push_scissor(panel->scissor_region.x,
                 panel->scissor_region.y,
                 panel->scissor_region.w,
                 panel->scissor_region.h);

        draw_rect_scissored(&gc.draw_buffer, 
                            panel_screen_x, 
                            panel_screen_y, 
                            panel->w, 
                            panel->h, 
                            panel->bg_color);
    
        if (panel->border_width > 0) 
        {
            draw_rect_scissored(&gc.draw_buffer,
                                panel_screen_x,
                                panel_screen_y,
                                panel->w,
                                panel->border_width,
                                panel->border_color);
            
            draw_rect_scissored(&gc.draw_buffer,
                                panel_screen_x,
                                panel_screen_y + panel->h - panel->border_width,
                                panel->w,
                                panel->border_width,
                                panel->border_color);
            
            draw_rect_scissored(&gc.draw_buffer,
                                panel_screen_x,
                                panel_screen_y,
                                panel->border_width,
                                panel->h,
                                panel->border_color);
            
            draw_rect_scissored(&gc.draw_buffer,
                                panel_screen_x + panel->w - panel->border_width,
                                panel_screen_y,
                                panel->border_width,
                                panel->h,
                                panel->border_color);
        }
    pop_scissor();
}

void scroll_panel(panel_t *panel, i32 delta_x, i32 delta_y)
{
    if (!panel->scrollable){
        return;
    }
    
    panel->scroll_x += delta_x;
    panel->scroll_y += delta_y;
    
    panel->scroll_x = Clamp(0, panel->scroll_x, panel->max_scroll_x);
    panel->scroll_y = Clamp(0, panel->scroll_y, panel->max_scroll_y);

    panel_state_map_t *state = shgetp_null(gc.state_map, panel->title);
    if(state)
    {
        state->value.scroll_x = panel->scroll_x;
        state->value.scroll_y = panel->scroll_y;
    }
}

void handle_panel_mouse_move(ui_context_t *ctx, f32 delta_x, f32 delta_y)
{
    for (i32 i = 0; i < ctx->panel_count; i++) 
    {
        panel_t *panel = ctx->panels[i];

        // Hovering 
        panel->hover = point_in_panel(panel, (i32)gc.mouse_x, (i32)gc.mouse_y);

        if(panel->hover)
        {
            for(i32 i = 0; i < arrlen(panel->blocks); i++)
            {
                block_t *block = &panel->blocks[i];

                if(!block->enabled)
                    continue;

                i32 abs_x, abs_y = 0;

                block_abs_postion(panel, block, &abs_x, &abs_y);

                rect_t block_screen_rect = {
                    .x = abs_x,
                    .y = abs_y,
                    .w = block->w,
                    .h = block->h
                };

                block->hover = inside_rect((i32)gc.mouse_x, (i32)gc.mouse_y, &block_screen_rect);

                if(block->dragging)
                {
                    block->drag_delta_x = (f32)gc.mouse_x - block->drag_start_x;
                    block->drag_delta_y = (f32)gc.mouse_y - block->drag_start_y;
                }

                // do custom update if required
                if(block->custom_update)
                {
                    block->custom_update(panel, block);
                }

                block_save_state(block);
            }
        }
        else
        {
            for(i32 i = 0; i < arrlen(panel->blocks); i++)
            {
                block_t *block = &panel->blocks[i];
                block->hover = false;

                block_state_map_t *state = shgetp_null(gc.block_map, block->title);
                if(state)
                {
                    state->value.hover = block->hover;
                }
            }
        }

        // dragged panel 
        if (panel->dragging && gc.right_button_down) 
        {
            scroll_panel(panel, (i32)delta_x, (i32)delta_y);
        }
    }
}

void handle_panel_mouse_button(ui_context_t *ctx)
{
    panel_t *target = find_panel_at_point(ctx, (i32)gc.mouse_x, (i32)gc.mouse_y);

    if (target)
    {
        for(i32 i = 0; i < arrlen(target->blocks); i++)
        {
            block_t *block = &target->blocks[i];

            if (gc.left_button_down)
            {
                if(block->hover)
                {
                    block->clicked = true;
                    printf("Block : %s clicked\n", block->title);

                    // handle dragging 
                    if(block->draggable && !block->dragging)
                    {
                        // first time getting dragged
                        block->dragging = true;
                        block->drag_start_x = (f32) gc.mouse_x;
                        block->drag_start_y = (f32) gc.mouse_y;
                    }
                }
            }
            else
            {
                // Was clicked and not anymore
                if(block->clicked && block->hover)
                {
                    block->released = true;
                    printf("Block : %s released\n", block->title);
                    if (block->toggleable) {
                        block->toggled = !block->toggled;
                    }
                }
                else
                {
                    block->released  = false;
                }

                // reset state
                block->clicked = false;
                block->dragging = false;
            }
            block_save_state(block);
        }
    }

    if (gc.right_button_down) 
    {
        // Start dragging the topmost panel under mouse
        panel_t *target = find_panel_at_point(ctx, (i32)gc.mouse_x, (i32)gc.mouse_y);
        if (target && target->scrollable) {
            panel_state_map_t *state = shgetp_null(gc.state_map, target->title);
            if(state)
            {
                target->dragging = state->value.dragging = true;
                target->drag_start_x = state->value.drag_start_x = (i32)gc.mouse_x;
                target->drag_start_y = state->value.drag_start_y = (i32)gc.mouse_y;
                target->scroll_start_x = state->value.scroll_start_x = target->scroll_x;
                target->scroll_start_y = state->value.scroll_start_y = target->scroll_y;
            }
        }
    } 
    else 
    {
        for (i32 i = 0; i < ctx->panel_count; i++) 
        {
            panel_state_map_t *state = shgetp_null(gc.state_map, ctx->panels[i]->title);
            if(state)
            {
                ctx->panels[i]->dragging = state->value.dragging = false;
            }
        }
    }
}

void handle_panel_scroll(ui_context_t *ctx, f32 scroll_x, f32 scroll_y)
{
    panel_t *target = find_panel_at_point(ctx, (i32)gc.mouse_x, (i32)gc.mouse_y);

    if (target && target->scrollable) {
        scroll_panel(target, (i32)(scroll_x), (i32)(scroll_y));
    }
}

void begin_panel(ui_context_t *ctx, panel_t *panel)
{
    ctx->active_panel = panel;
    draw_panel_background(panel);
}

void panel_layout_pass(ui_context_t *ctx)
{
    if (ctx->active_panel) 
    {
        const i32 item_spacing = ctx->active_panel->child_spacing;
        const i32 padding_top = ctx->active_panel->padding_top;
        const i32 padding_bottom = ctx->active_panel->padding_bottom;
        const i32 padding_left = ctx->active_panel->padding_left;
        const i32 padding_right = ctx->active_panel->padding_right;

        const i32 default_item_height = ctx->active_panel->default_child_height;
        const i32 default_item_width = ctx->active_panel->default_child_width;

        i32 content_height = padding_top;
        i32 content_width = 0;

        i32 block_count = arrlen(ctx->active_panel->blocks);

        switch(ctx->active_panel->layout)
        {
            case VERTICAL_LAYOUT:

                if (block_count == 0) {
                    break;
                }

                for(int i = 0; i < block_count; i++)
                {
                    block_t *block = &ctx->active_panel->blocks[i];

                    i32 block_width = (block->w > 0) ? block->w : default_item_width;
                    i32 block_height = (block->h > 0) ? block->h : default_item_height;

                    content_width = MAX(block_width, content_width);
                    content_height += block_height;
                    
                    if (i < block_count - 1) {
                        content_height += item_spacing;
                    }
                }

                content_height += padding_bottom;
                content_width += padding_left + padding_right;

                i32 current_y = padding_top;
                i32 base_x = padding_left;

                for(int i = 0; i < block_count; i++)
                {
                    block_t *block = &ctx->active_panel->blocks[i];
                    if (block->w <= 0) block->w = default_item_width;
                    if (block->h <= 0) block->h = default_item_height;
        
                    block->x = base_x;
                    block->y = current_y;

                    f32 text_width = get_string_width(block->text.font, block->text.string);
                    f32 text_height = get_line_height(block->text.font);

                    block->text.pos.x = base_x + (block->w - text_width)/2;
                    block->text.pos.y = current_y + (default_item_height-text_height)/2;

                    current_y += block->h + item_spacing;
                }

                break;
            case HORIZONTAL_LAYOUT:
                break;
        }

        set_panel_content_size(ctx->active_panel, content_width, content_height);
    }
}

void draw_scroll_bar(ui_context_t *ctx)
{
    i32 gap = ctx->active_panel->border_width*4;
    i32 scroll_thumb_width = ctx->active_panel->border_width*3;
    
    i32 scroll_bar_x = ctx->active_panel->w-gap;
    
    i32 content_height = ctx->active_panel->max_scroll_y + ctx->active_panel->h;
    if (content_height <= 0) return;
    
    f32 visible_ratio = (f32)ctx->active_panel->h / (f32)content_height;
    visible_ratio = Clamp(visible_ratio, 0.1f, 1.0f); 
    
    i32 scroll_thumb_height = (i32)(ctx->active_panel->h * visible_ratio);
    scroll_thumb_height = MAX(scroll_thumb_height, scroll_thumb_width * 2);

    i32 scroll_bar_min_y = ctx->active_panel->y + ctx->active_panel->scroll_y;
    i32 scroll_bar_max_y = scroll_bar_min_y + ctx->active_panel->h - scroll_thumb_height;
    
    f32 scroll_bar_ratio = fabsf((f32)(ctx->active_panel->scroll_y)/(f32)(ctx->active_panel->max_scroll_y));

    i32 scroll_bar_y = LERP_F32(scroll_bar_min_y, scroll_bar_max_y,scroll_bar_ratio);
    
    draw_rect_in_panel(ctx->active_panel ,scroll_bar_x,scroll_bar_y,scroll_thumb_width,scroll_thumb_height,(color4_t){72, 73, 80,125});
}

void end_panel(ui_context_t *ctx)
{
    panel_layout_pass(ctx);

    for(int i = 0; i < arrlen(ctx->active_panel->blocks); i++)
    {
        block_t *block = &ctx->active_panel->blocks[i];

        update_block_interaction(ctx->active_panel, block);

        render_block(ctx->active_panel, block);
    }

    if(ctx->active_panel->scrollable)
    {
        draw_scroll_bar(ctx);
    }

    ctx->active_panel = NULL;

    reset_input_for_frame();
}

/* Block stuff */

void add_block(ui_context_t *ctx, block_t block)
{
    if (ctx->active_panel)
    {
        block.enabled = true;
        block.visible = true;
        block.clickable = true;

        block.color = (color4_t){156, 147, 249,255};
        block.hover_color = (color4_t){255, 121, 185,255}; 

        block.text.font = gc.font;
        block.text.string = block.title;
        block.text.color = (color4_t){232, 255, 255, 255};
    
        block_load_state(&block);

        arrput(ctx->active_panel->blocks, block);
    }
}

void block_abs_postion(panel_t *panel, block_t *block, i32 *abs_x, i32 *abs_y)
{
    i32 x,y;

    panel_abs_pos(panel, &x, &y);

    *abs_x = x + block->x;
    *abs_y = y+block->y-panel->scroll_y;
}

void block_load_state(block_t *block)
{
    block_state_map_t *state = shgetp_null(gc.block_map, block->title);
    if(state)
    {
        block->value=state->value.value;
        block->hover=state->value.hover;
        block->clicked=state->value.clicked;
        block->toggled=state->value.toggled;
        block->released=state->value.released;
        block->dragging=state->value.dragging;
        block->drag_delta_x=state->value.drag_delta_x; 
        block->drag_delta_y=state->value.drag_delta_y;
    }
    else
    {
        block_state_t state = {0};
        shput(gc.block_map, block->title, state);
    }
}

void block_save_state(block_t *block)
{
    block_state_t state = {
        .value = block->value,
        .hover = block->hover,
        .clicked = block->clicked,
        .toggled = block->toggled,
        .released = block->released,
        .dragging = block->dragging,
        .drag_delta_x = block->drag_delta_x,
        .drag_delta_y = block->drag_delta_y
    };

    shput(gc.block_map, block->title, state);
}

void update_block_interaction(panel_t *panel, block_t *block)
{
    if (!block->enabled) 
    {
        return;
    }

    if (block->custom_update) 
    {
        block->custom_update(panel, block);
    }

    block_save_state(block);
}

void render_block(panel_t *panel, block_t *block)
{
    if(!block->visible)
    {
        return;
    }

    if (block->custom_render) {
        block->custom_render(panel, block);
        return;
    }

    color4_t color = block->hover?block->hover_color:block->color;
    
    // default way to render a block
    draw_rect_in_panel(panel, block->x, block->y, block->w, block->h, color);
    render_text_in_panel(panel, block);
}


/* -------------- Checkbox Widget Stuff -------------- */

void render_checkbox(panel_t *panel, block_t *block)
{
    color4_t color = block->hover?block->hover_color:block->color;

    draw_rect_in_panel(panel, block->x, block->y, block->w, block->h, color);

    i32 box_size = 20;
    i32 box_x = block->x + 10;
    i32 box_y = block->y + ((block->h - box_size)/2);

    color4_t box_color = {200, 200, 200, 255};
    draw_rect_in_panel(panel, box_x, box_y, box_size, box_size, box_color);

    if (block->toggled) 
    {
        i32 padding = 4;
        color4_t check_color = {100, 200, 100, 255};
        draw_rect_in_panel(panel, 
                          box_x + padding, 
                          box_y + padding,
                          box_size - padding * 2, 
                          box_size - padding * 2,
                          check_color);
    }
    
    if (block->text.string) 
    {
        block->text.pos.x = box_x + box_size + 10;
        block->text.pos.y = block->y + (block->h - get_line_height(block->text.font)) / 2;
        render_text_in_panel(panel, block);

        rendered_text_tt icon = {
            .font = gc.icon_font,
            .pos = {.x = block->text.pos.x+get_string_width(gc.font, block->text.string)+10, .y = block->text.pos.y},
            .color = {.r = 255, .g = 184, .b = 108, .a = 255},
            .string = EDIT_ICON
        };

        render_string_unicode(&gc.draw_buffer, &icon);
    }
}

bool ui_checkbox(ui_context_t *ctx, char *title)
{
    block_t checkbox = {0};

    if (ctx->active_panel)
    {
        checkbox.enabled = true;
        checkbox.visible = true;
        checkbox.clickable = true;
        checkbox.toggleable = true;

        checkbox.color = (color4_t){156, 147, 249,255};
        checkbox.hover_color = (color4_t){255, 121, 185,255}; 

        checkbox.h = 40;
        checkbox.title = title;

        checkbox.text.font = gc.font;
        checkbox.text.string = checkbox.title;
        checkbox.text.color = (color4_t){232, 255, 255, 255};
    
        block_load_state(&checkbox);

        checkbox.custom_render = render_checkbox;

        arrput(ctx->active_panel->blocks, checkbox);
    }

    return checkbox.toggled;
}

/* -------------- Button Widget Stuff -------------- */

void render_button(panel_t *panel, block_t *block)
{
    color4_t color = block->hover?block->hover_color:block->color;

    draw_rounded_rect_in_panel(panel, block->x, block->y, block->w, block->h, color);
    render_text_in_panel(panel, block);
}

bool ui_button(ui_context_t *ctx, char *title)
{
    block_t button = {0};
    bool release = false;

    if (ctx->active_panel)
    {
        button.enabled = true;
        button.visible = true;
        button.clickable = true;

        button.title = title;

        button.text.font = gc.font;
        button.text.string = button.title;
        button.text.color = (color4_t){232, 255, 255, 255};

        button.color = (color4_t){156, 147, 249,255};
        button.hover_color = (color4_t){255, 121, 185,255}; 
    
        block_load_state(&button);

        button.custom_render = render_button;

        release = button.released;
    
        if(release) {
            button.released = false;
        }
        arrput(ctx->active_panel->blocks, button);
    }

    return release;
}

/* -------------- Radio Button Widget Stuff -------------- */

void render_radio(panel_t *panel, block_t *block)
{
    color4_t color = block->hover?block->hover_color:block->color;

    draw_rect_in_panel(panel, block->x, block->y, block->w, block->h, color);

    bool selected = block->user_data;

    i32 box_size = 20;
    i32 box_x = block->x + 10;
    i32 box_y = block->y + ((block->h - box_size)/2);


    color4_t box_color = {200, 200, 200, 255};
    draw_rect_in_panel(panel, box_x, box_y, box_size, box_size, box_color);

    if (selected) 
    {
        i32 padding = 4;
        color4_t check_color = {100, 200, 100, 255};
        draw_rect_in_panel(panel, 
                          box_x + padding, 
                          box_y + padding,
                          box_size - padding * 2, 
                          box_size - padding * 2,
                          check_color);
    }
    
    // Draw label to the right
    if (block->text.string) 
    {
        block->text.pos.x = box_x + box_size + 10;
        block->text.pos.y = block->y + (block->h - get_line_height(block->text.font)) / 2;
        render_text_in_panel(panel, block);
        
        /*         
        if (selected)
        {
            rendered_text_tt icon = {
                .font = gc.icon_font,
                .pos = {.x = block->text.pos.x+get_string_width(gc.font, block->text.string)+10, .y = block->text.pos.y},
                .color = HEX_TO_COLOR4(0xe7f8f2),
                .string = CHECK_ICON
            };
            render_string_unicode(&gc.draw_buffer, &icon);
        } */
    }
}

void ui_radio(ui_context_t *ctx, char *title, i32 *var, i32 val)
{
    block_t radio = {0};

    if (ctx->active_panel)
    {
        radio.enabled = true;
        radio.visible = true;
        radio.clickable = true;
        radio.toggleable = true;

        radio.h = 40;
        radio.title = title;

        radio.color = (color4_t){156, 147, 249,255};
        radio.hover_color = (color4_t){255, 121, 185,255}; 

        radio.text.font = gc.font;
        radio.text.string = radio.title;
        radio.text.color = (color4_t){232, 255, 255, 255};
    
        block_load_state(&radio);

        radio.user_data = (void *)(intptr_t)(*var == val ? 1 : 0);

        radio.custom_render = render_radio;

        arrput(ctx->active_panel->blocks, radio);

        if(radio.clicked)
        {
            *var = val;
        }
    }
}

/* -------------- Text Edit Widget Stuff -------------- */

void buffer_ripple_remove(text_edit_state_t *state, size_t position, size_t count) 
{
    memmove(&state->buffer[position], &state->buffer[position + count], 
            state->buffer_len - (position + count));
    state->buffer_len -= count;
}

void buffer_ripple_remove_slice(text_edit_state_t* state, size_t start, size_t end) 
{
    buffer_ripple_remove(state, start, end - start);
}

void buffer_ripple_remove_char(text_edit_state_t* state, size_t position) 
{
    buffer_ripple_remove(state, position, 1);
}

void buffer_make_room(text_edit_state_t* state, size_t position, size_t count) 
{
    memmove(&state->buffer[position + count], 
            &state->buffer[position], 
            state->buffer_len - position);
    state->buffer_len += count;
}

i32 text_edit_get_char_at_x(panel_t *panel, block_t *block, text_edit_state_t *state, i32 mouse_x)
{
    i32 abs_x, abs_y = 0;

    block_abs_postion(panel, block, &abs_x, &abs_y);

    // we are now in the block space
    i32 local_x = mouse_x - abs_x;
    // acount for scroll
    local_x += state->scroll_offset;

    f32 cumulative_width = 5;

    for (i32 i = 0; i < state->buffer_len; i++) {
        f32 char_width = get_char_width(block->text.font, state->buffer[i]);

        // inside the char area
        if(local_x < (cumulative_width + (char_width/2)))
        {
            // found it
            return i;
        }
        cumulative_width += char_width;
    }

    // not inside it
    return state->buffer_len;
}

void text_edit_remove_selection(text_edit_state_t *state)
{
    if(state->selection_start >= 0)
    {
        if(state->selection_start > state->selection_end)
        {
            SWAP(state->selection_start, state->selection_end, i32);
        }
        i32 start = state->selection_start;
        i32 end = state->selection_end;

        buffer_ripple_remove_slice(state, start, end);

        state->cursor_pos = start;
        // reset selection
        state->selection_start = -1;
    }
}

void text_edit_insert_char(text_edit_state_t *state, char c)
{
    if(state->buffer_len > (TEXT_BUFFER_MAX - 1))
        return;

    // if we have selection remove it
    text_edit_remove_selection(state);

    // make room for one character
    buffer_make_room(state, state->cursor_pos, 1);

    state->buffer[state->cursor_pos] = c;
    // make sure we are null terminated
    state->buffer[state->buffer_len] = '\0';
    // advance the cursor
    state->cursor_pos++;
}

void text_edit_delete_char(text_edit_state_t *state)
{
    text_edit_remove_selection(state);

    if(state->cursor_pos == 0){
        return;
    }

    buffer_ripple_remove_slice(state, state->cursor_pos - 1, state->cursor_pos);

    state->buffer[state->buffer_len] = '\0';
    // advance the cursor
    state->cursor_pos--;
}

void text_edit_delete_slice(text_edit_state_t *state, i32 count)
{
    assert(state->cursor_pos > count);

    text_edit_remove_selection(state);

    if(state->cursor_pos == 0){
        return;
    }

    buffer_ripple_remove_slice(state, state->cursor_pos - count, state->cursor_pos);

    state->buffer[state->buffer_len] = '\0';
    // advance the cursor
    state->cursor_pos-=count;
}

void text_edit_delete_char_forward(text_edit_state_t *state)
{
    text_edit_remove_selection(state);

    if (state->cursor_pos >= state->buffer_len) return;

    buffer_ripple_remove_slice(state, state->cursor_pos, state->cursor_pos+1);

    state->buffer[state->buffer_len] = '\0';
}

void text_edit_delete_slice_forward(text_edit_state_t *state, i32 count)
{
    text_edit_remove_selection(state);

    if (state->cursor_pos >= state->buffer_len) return;

    buffer_ripple_remove_slice(state, state->cursor_pos, state->cursor_pos+count);

    state->buffer[state->buffer_len] = '\0';
}

void text_edit_clamp_cursor(text_edit_state_t *state)
{
    state->cursor_pos = Clamp(0, state->cursor_pos, state->buffer_len);
}

void text_edit_update(panel_t *panel, block_t *block)
{
    text_edit_map_t *map_entry = shgetp_null(gc.text_map, block->title);
    if (!map_entry) return;
    
    text_edit_state_t *state = &map_entry->value;

    state->cursor_blink_timer += gc.dt;
    if (state->cursor_blink_timer > 500.0) {
        state->cursor_visible = !state->cursor_visible;
        state->cursor_blink_timer = 0.0;
    }

    if(block->clicked)
    {
        gc.focused_text_edit = block->title;
        state->focused =  true;

        state->cursor_pos = text_edit_get_char_at_x(panel, block, state, gc.mouse_x);
        state->selection_start = -1;
        state->cursor_visible = true;
        state->cursor_blink_timer = 0.0;

    }
    else if (gc.left_button_down && !block->hover && state->focused)
    {
        // Clicked outside - lose focus
        if (gc.focused_text_edit && strcmp(gc.focused_text_edit, block->title) == 0) {
            gc.focused_text_edit = NULL;
        }
        state->focused = false;
        state->selection_start = -1;
    }

    if(state->focused && block->dragging)
    {
        if(!state->mouse_selecting)
        {
            state->mouse_selecting = true;
            state->drag_start_x = text_edit_get_char_at_x(panel, block, state, block->drag_start_x);
            state->selection_start = state->drag_start_x;
        }

        i32 current_pos = text_edit_get_char_at_x(panel, block, state, gc.mouse_x);
        state->selection_end = current_pos;
        state->cursor_pos = current_pos;

        if (state->selection_start == state->selection_end) {
            state->selection_start = -1;
        }
    }

    if (!block->dragging)
    {
        state->mouse_selecting = false;
    }

    if(!state->focused)
        return;

    if (key_just_pressed[GLFW_KEY_BACKSPACE]) 
    {
        if (key_pressed[GLFW_KEY_LEFT_CONTROL] ||
            key_pressed[GLFW_KEY_RIGHT_CONTROL]) 
        {
            i32 start = 0;            
            for(i32 i = state->cursor_pos-1; i > 0; i--)
            {
                if(state->buffer[i] == ' ')
                {
                    start = i;
                    break;
                }
            }

            text_edit_delete_slice(state,state->cursor_pos-start);
            state->cursor_visible = true;
            state->cursor_blink_timer = 0.0;
        }
        else
        {
            text_edit_delete_char(state);
            state->cursor_visible = true;
            state->cursor_blink_timer = 0.0;
        }
    }

    if (key_just_pressed[GLFW_KEY_DELETE]) 
    {
        if (key_pressed[GLFW_KEY_LEFT_CONTROL] ||
            key_pressed[GLFW_KEY_RIGHT_CONTROL]) 
        {
            i32 end = state->buffer_len-1;

            for(i32 i = state->cursor_pos; i < state->buffer_len; i++)
            {
                if(state->buffer[i] == ' ' || state->buffer[i] == '\0')
                {
                    end = i;
                    break;
                }
            }
            text_edit_delete_slice_forward(state,end-state->cursor_pos);
            state->cursor_visible = true;
            state->cursor_blink_timer = 0.0;
        }
        else
        {
            text_edit_delete_char_forward(state);
            state->cursor_visible = true;
            state->cursor_blink_timer = 0.0;
        }
    }

    if (key_just_pressed[GLFW_KEY_LEFT]) 
    {
        if (key_pressed[GLFW_KEY_LEFT_SHIFT] ||
            key_pressed[GLFW_KEY_RIGHT_SHIFT]) 
        {
            if (state->selection_start < 0) {
                state->selection_start = state->cursor_pos;
            }
            state->cursor_pos--;
            text_edit_clamp_cursor(state);
            state->selection_end = state->cursor_pos;
        }
        else if (key_pressed[GLFW_KEY_LEFT_CONTROL] ||
                key_pressed[GLFW_KEY_RIGHT_CONTROL]) 
        {
            for(i32 i = state->cursor_pos-1; i > 0; i--)
            {
                if(state->buffer[i] == ' ')
                {
                    state->cursor_pos = i;
                    break;
                }
            }
            text_edit_clamp_cursor(state);
            state->selection_start = -1; // Clear selection
        } 
        else 
        {
            state->cursor_pos--;
            text_edit_clamp_cursor(state);
            state->selection_start = -1; // Clear selection
        }
        state->cursor_visible = true;
        state->cursor_blink_timer = 0.0;
    }

    if (key_just_pressed[GLFW_KEY_RIGHT]) 
    {
        if (key_pressed[GLFW_KEY_LEFT_SHIFT] ||
            key_pressed[GLFW_KEY_RIGHT_SHIFT]) 
        {
            if (state->selection_start < 0) {
                state->selection_start = state->cursor_pos;
            }
            state->cursor_pos++;
            text_edit_clamp_cursor(state);
            state->selection_end = state->cursor_pos;
        }
        else if (key_pressed[GLFW_KEY_LEFT_CONTROL] ||
                key_pressed[GLFW_KEY_RIGHT_CONTROL]) 
        {
            for(i32 i = state->cursor_pos; i < state->buffer_len; i++)
            {
                if(state->buffer[i] == ' ' || state->buffer[i] == '\0')
                {
                    state->cursor_pos = i;
                    break;
                }
            }
            state->cursor_pos++;
            text_edit_clamp_cursor(state);
            state->selection_start = -1; // Clear selection
        }  
        else 
        {
            state->cursor_pos++;
            text_edit_clamp_cursor(state);
            state->selection_start = -1; // Clear selection
        }
        state->cursor_visible = true;
        state->cursor_blink_timer = 0.0;
    }

    if (key_just_pressed[GLFW_KEY_HOME]) {
        state->cursor_pos = 0;
        state->selection_start = -1;
        state->cursor_visible = true;
        state->cursor_blink_timer = 0.0;
    }
    
    if (key_just_pressed[GLFW_KEY_END]) {
        state->cursor_pos = state->buffer_len;
        state->selection_start = -1;
        state->cursor_visible = true;
        state->cursor_blink_timer = 0.0;
    }

    if (key_pressed[GLFW_KEY_LEFT_CONTROL] && key_just_pressed[GLFW_KEY_A]) 
    {
        state->selection_start = 0;
        state->selection_end = state->buffer_len;
        state->cursor_pos = state->buffer_len;
    }

    for (i32 i = 0; i < input_char_count; i++) 
    {
        text_edit_insert_char(state, input_char_queue[i]);
        state->cursor_visible = true;
        state->cursor_blink_timer = 0.0;
    }
    input_char_count = 0;

    f32 cursor_x = 5; // Left padding
    for (i32 i = 0; i < state->cursor_pos; i++) 
    {
        cursor_x += get_char_width(block->text.font, state->buffer[i]);
    }
    
    i32 visible_width = block->w - 10; // Account for padding
    if (cursor_x - state->scroll_offset > visible_width) {
        state->scroll_offset = (i32)(cursor_x - visible_width);
    }
    if (cursor_x - state->scroll_offset < 0) {
        state->scroll_offset = (i32)cursor_x;
    }
}

void render_text_edit(panel_t *panel, block_t *block) 
{
    text_edit_map_t *map_entry = shgetp_null(gc.text_map, block->title);
    if (!map_entry) return;
    
    text_edit_state_t *state = &map_entry->value;
    
    // Background color based on focus
    color4_t bg_color = state->focused ? 
        (color4_t){60, 60, 80, 255} : 
        (color4_t){40, 42, 54, 255};
    
    draw_rect_in_panel(panel, block->x, block->y, block->w, block->h, bg_color);
    
    // Border
    color4_t border_color = state->focused ? 
        (color4_t){100, 150, 255, 255} : 
        (color4_t){100, 100, 100, 255};
    
    i32 border = 2;
    // Top
    draw_rect_in_panel(panel, block->x, block->y, block->w, border, border_color);
    // Bottom
    draw_rect_in_panel(panel, block->x, block->y + block->h - border, block->w, border, border_color);
    // Left
    draw_rect_in_panel(panel, block->x, block->y, border, block->h, border_color);
    // Right
    draw_rect_in_panel(panel, block->x + block->w - border, block->y, border, block->h, border_color);
    
    // Set up scissor for text (don't draw outside box)
    i32 abs_x, abs_y;
    block_abs_postion(panel, block, &abs_x, &abs_y);
    
    push_scissor(abs_x + border, abs_y + border, 
                 block->w - border * 2, block->h - border * 2);
    
    // Draw selection highlight
    if (state->selection_start >= 0) {
        i32 start = MIN(state->selection_start, state->selection_end);
        i32 end = MAX(state->selection_start, state->selection_end);
        
        f32 sel_start_x = 5;
        for (i32 i = 0; i < start; i++) {
            sel_start_x += get_char_width(block->text.font, state->buffer[i]);
        }
        
        f32 sel_width = 0;
        for (i32 i = start; i < end; i++) {
            sel_width += get_char_width(block->text.font, state->buffer[i]);
        }
        
        i32 sel_x = block->x + (i32)sel_start_x - state->scroll_offset;
        i32 sel_y = block->y + 5;
        i32 sel_h = block->h - 10;
        
        color4_t selection_color = {100, 150, 200, 128};
        draw_rect_in_panel(panel, sel_x, sel_y, (i32)sel_width, sel_h, selection_color);
    }
    
    // Draw text
    if (state->buffer_len > 0) {
        rendered_text_tt text = {
            .font = block->text.font,
            .pos = {
                .x = block->x + 5 - state->scroll_offset,
                .y = block->y + (block->h - get_line_height(block->text.font)) / 2
            },
            .color = {255, 255, 255, 255},
            .string = state->buffer
        };
        block->text = text;
        render_text_in_panel(panel, block);
    }
    
    // Draw cursor
    if (state->focused && state->cursor_visible) {
        f32 cursor_x = 5;
        for (i32 i = 0; i < state->cursor_pos; i++) {
            cursor_x += get_char_width(block->text.font, state->buffer[i]);
        }
        
        i32 cx = block->x + (i32)cursor_x - state->scroll_offset;
        i32 cy = block->y + 5;
        i32 ch = block->h - 10;
        
        color4_t cursor_color = {255, 255, 255, 255};
        draw_rect_in_panel(panel, cx, cy, 2, ch, cursor_color);
    }
    
    pop_scissor();
}

text_edit_state_t *ui_text_edit(ui_context_t *ctx, char *title, char *initial_text)
{
    text_edit_map_t *map_entry = shgetp_null(gc.text_map, title);

    if (!map_entry) 
    {
        text_edit_state_t state = {0};
        if (initial_text) {
            strncpy(state.buffer, initial_text, TEXT_BUFFER_MAX-1);
            state.buffer_len = (i32)strlen(state.buffer);
        }
        state.selection_start = -1;
        shput(gc.text_map, title, state);
    }

    block_t block = {0};
    if (ctx->active_panel)
    {
        block.enabled = true;
        block.visible = true;
        block.clickable = true;
        block.toggleable = true;

        block.custom_render = render_text_edit;
        block.custom_update = text_edit_update;
        block.h = 40;
        block.title = title;

        block.text.font = gc.font;
        block.text.string = block.title;
        block.text.color = (color4_t){232, 255, 255, 255};
        block_load_state(&block);

        arrput(ctx->active_panel->blocks, block);
    }

    return &shgetp_null(gc.text_map, title)->value;
}

void reset_ui_ctx(ui_context_t *ctx)
{
    for (i32 i = 0; i < ctx->panel_count; i++) 
    {
        panel_t *panel = ctx->panels[i];
        arrsetlen(panel->blocks,0);
    }
    ctx->next_panel_id = 1;
    ctx->panel_count = 0;
}

ui_context_t* create_ui_context(void)
{
    ui_context_t *ctx = (ui_context_t*)calloc(1, sizeof(ui_context_t));
    ctx->next_panel_id = 1;

    gc.state_map = NULL;
    gc.block_map = NULL;

    return ctx;
}

/* For when I feel like experimenting with multithreading */
thread_func_ret_t render_tile(thread_func_param_t data) 
{
    tile_data_t *tile = (tile_data_t *)data;

    fast_srand(tile->start_x * 1000 + tile->start_y + 1);

    for (u32 y = tile->start_y; y < tile->end_y; ++y) 
    {
        for (u32 x = tile->start_x; x < tile->end_x; ++x) 
        {
        }
    }

    #ifdef _WIN32
        return 0;
    #else
        return NULL;
    #endif
}

void render_all_parallel(void)
{
    gc.draw_buffer.height = gc.screen_height;
    gc.draw_buffer.width  = gc.screen_width;

    u32 height  = gc.draw_buffer.height;
    u32 width   = gc.draw_buffer.width;

    gc.draw_buffer.pixels = ARENA_ALLOC(gc.frame_arena, height * width * sizeof(color4_t));

    clear_screen(&gc.draw_buffer, HEX_TO_COLOR4(0x282a36));

    i32 num_threads = get_core_count();
    const u32 tile_size = 64;

    u32 tiles_x = CEIL_DIV(width, tile_size);
    u32 tiles_y = CEIL_DIV(height, tile_size);
    u32 total_tiles = tiles_x * tiles_y;

    tile_data_t* tiles = ARENA_ALLOC(gc.frame_arena, total_tiles * sizeof(tile_data_t));
    thread_handle_t* threads = ARENA_ALLOC(gc.frame_arena, num_threads * sizeof(thread_handle_t));

    i32 tile_idx = 0;
    
    for (u32 ty = 0; ty < tiles_y; ty++) 
    {
        u32 start_y = ty * tile_size;
        u32 end_y = (start_y + tile_size > height) ? height : start_y + tile_size;
        
        for (u32 tx = 0; tx < tiles_x; tx++) 
        {
            u32 start_x = tx * tile_size;
            u32 end_x = (start_x + tile_size > width) ? width : start_x + tile_size;
            
            tiles[tile_idx] = (tile_data_t){
                .start_x = start_x, .end_x = end_x,
                .start_y = start_y, .end_y = end_y,
                .width = width, .height = height
            };
            
            int thread_slot = tile_idx % num_threads;
            
            if (tile_idx >= num_threads) {
                PROFILE("Waiting for a slot"){
                    join_thread(threads[thread_slot]);
                }
            }
            
            PROFILE("Actually creating a thread, does it matter ?")
            {
                threads[thread_slot] = create_thread(render_tile, &tiles[tile_idx]);
            }
            
            tile_idx++;
        }
    }

    int remaining_threads = (total_tiles < num_threads) ? total_tiles : num_threads;

    PROFILE("Waiting to join")
    {
        for (int i = 0; i < remaining_threads; i++) {
            join_thread(threads[i]);
        }
    }

    u32 pos = gc.screen_width-20*10;

    rendered_text_tt text = {
        .font = gc.font,
        .pos = {.x=pos, .y=0},
        .color = {.r = 255, .g = 255, .b = 255, .a=255},
        .string = frametime
    };
    
    snprintf(frametime, BUFFER_SIZE, "%.2f ms, %d cores", gc.average_frame_time*1000,num_threads);
    render_text_tt(&gc.draw_buffer, &text);

    if(gc.profile)
    {
        render_prof_entries();
    }

    prof_buf_count = 0;
    
    if(gc.capture)
    {
        export_image(&gc.draw_buffer, "capture.tga");
        gc.capture = false;
    }
}

void blit_to_screen(void *pixels, u32 width, u32 height)
{
    glBindTexture(GL_TEXTURE_2D, gc.texture);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                    (int)width, (int)height,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gc.read_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // make the default framebuffer active again ?
    
    // transfer pixel values from one region of a read framebuffer to another region of a draw framebuffer.
    glBlitFramebuffer(0, 0, width, height,  // src
                      0, height, width, 0,  // dst (flipped)    
                      GL_COLOR_BUFFER_BIT,    
                      GL_NEAREST);      
}

void present_frame(image_view_t* view) 
{
    blit_to_screen(view->pixels, view->width, view->height);
    glfwSwapBuffers((GLFWwindow*)gc.window);
}

void render_all(void)
{
    gc.draw_buffer.height = gc.screen_height;
    gc.draw_buffer.width  = gc.screen_width;

    u32 height  = gc.draw_buffer.height;
    u32 width   = gc.draw_buffer.width;

    gc.draw_buffer.pixels = ARENA_ALLOC(gc.frame_arena, height * width * sizeof(color4_t));
    
    PROFILE("Clearing the screen why does it take so long ?")
    {
        clear_screen(&gc.draw_buffer, HEX_TO_COLOR4(0x282a36));
    }

    PROFILE("Building and rendering the ui")
    {
        panel_t *side_panel = create_panel(gc.ui_ctx, "side_panel", 0, 0, gc.side_panel_x, gc.screen_height, VERTICAL_LAYOUT);
        panel_t *main_panel = create_panel(gc.ui_ctx, "main panel",gc.side_panel_x, 0, gc.screen_width-gc.side_panel_x, gc.screen_height, VERTICAL_LAYOUT);

        text_edit_state_t *text_state;

        begin_panel(gc.ui_ctx, main_panel);

            todo_list *curr_list = &main_list[gc.curr_list];

            for(i32 i = 0; i < arrlen(curr_list->todo_items); i++)
            {
                if(ui_checkbox(gc.ui_ctx, curr_list->todo_items[i].todo))
                {
                    curr_list->todo_items[i].completed = true;
                }
            }

            text_state = ui_text_edit(gc.ui_ctx, "TextEdit1", "Add your task ..");

            if(ui_button(gc.ui_ctx, "Submit"))
            {
                todo_item new_item = {0};
                todo_item_add_content(&new_item, text_state->buffer);
                todo_list_add(curr_list, new_item);
            }
        
        end_panel(gc.ui_ctx);
        
        begin_panel(gc.ui_ctx, side_panel);
            for(i32 i = 0; i < arrlen(main_list); i++)
            {
                ui_radio(gc.ui_ctx, main_list[i].name, &gc.curr_list, i);
            }
        end_panel(gc.ui_ctx);
    }

    rendered_text_tt frame_time = {
        .font = gc.font,
        .pos = {.x=gc.screen_width - (10+get_string_width(gc.font, frametime)), .y=0},
        .color = COLOR_WHITE,
        .string = frametime
    };
    
    snprintf(frametime, BUFFER_SIZE, "%.2f ms", gc.average_frame_time*1000);
    render_text_tt(&gc.draw_buffer, &frame_time);

    if(gc.profile)
    {
        render_prof_entries();
    }

    prof_buf_count = 0;
    
    if(gc.capture)
    {
        export_image(&gc.draw_buffer, "capture.tga");
        gc.capture = false;
    }
}

void update_time()
{
    gc.dt = get_time_difference(&gc.last_frame_start);
    
    gc.frame_history[gc.frame_idx] = gc.dt;
    gc.frame_idx = (gc.frame_idx + 1) % FRAME_HISTORY_SIZE;
    
    // Calculate average frame time to not look crazy when printed
    f64 totalTime = 0.0;
    for (int i = 0; i < FRAME_HISTORY_SIZE; i++) {
        totalTime += gc.frame_history[i];
    }
    gc.average_frame_time = totalTime / FRAME_HISTORY_SIZE;
    gc.current_fps = (gc.average_frame_time > 0.0) ? (1.0 / gc.average_frame_time) : 0.0;
}

void update_all()
{
    gc.dt = get_time_difference(&gc.last_frame_start);
    
    gc.frame_history[gc.frame_idx] = gc.dt;
    gc.frame_idx = (gc.frame_idx + 1) % FRAME_HISTORY_SIZE;
    
    // Calculate average frame time
    f64 totalTime = 0.0;
    for (int i = 0; i < FRAME_HISTORY_SIZE; i++) {
        totalTime += gc.frame_history[i];
    }
    gc.average_frame_time = totalTime / FRAME_HISTORY_SIZE;
    gc.current_fps = (gc.average_frame_time > 0.0) ? (1.0 / gc.average_frame_time) : 0.0;

    animation_update(gc.dt);

    animation_get((u64)&gc.side_panel_x, &gc.side_panel_x);
}

void cleanup_all()
{
    arena_reset(gc.frame_arena);

    reset_ui_ctx(gc.ui_ctx);

    prof_sort_results();
    prof_record_results();
    // prof_print_results();
    prof_reset();
}

void init_framebuffer(void)
{
    glGenTextures(1, &gc.texture);
    glBindTexture(GL_TEXTURE_2D, gc.texture);

    // Generate a full screen texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
                 (int)1920, (int)1080,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &gc.read_fbo);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gc.read_fbo);

    // attach the texture to the framebuffer
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, gc.texture, 0);
}

void *create_window(u32 width, u32 height, char *title)
{
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return NULL;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 8); // Enable multisampling for smoother edges
    
    GLFWwindow *window = CHECK_PTR(glfwCreateWindow((int)width, (int)height, title, NULL, NULL));
    
    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);
    
    glViewport(0, 0, (int)width, (int)height);
    
    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetErrorCallback(error_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowRefreshCallback(window, window_refresh_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW initialization error: %s\n", glewGetErrorString(err));
        return NULL;
    }

    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    return window;
}

bool init_all(void)
{
    gc.screen_width = 1100;
    gc.screen_height = 800;

    gc.window = create_window(gc.screen_width, gc.screen_height, "UI");

    if(!gc.window)
    {
        return false;
    }

    gc.frame_arena = arena_new();
    (void)ARENA_ALLOC(gc.frame_arena, MB(10));
    arena_reset(gc.frame_arena);

    init_framebuffer();

    gc.font = load_font_from_file("..\\Font\\Lato.ttf", 32.0f);
    gc.icon_font = load_font_from_file("..\\Font\\Icons.ttf", 32.0f);

    if(!(gc.font && gc.icon_font))
    {
        return false;
    }

    gc.ui_ctx = create_ui_context();
    
    gc.side_panel_x = 500;

    todo_list_new("Default list");
    todo_list_new("Hobbies");
    todo_list_new("Work stuff");
    todo_list_new("Programming");

    prof_init();

    gc.running     = true;
    gc.resize      = true;
    gc.rescale     = true;
    gc.render      = true;
    gc.dock        = false;
    gc.profile     = false;
    gc.changed     = true;


    get_time(&gc.last_frame_start);

    for (int i = 0; i < FRAME_HISTORY_SIZE; i++) {
        gc.frame_history[i] = 1.0 / 60.0; 
    }
    gc.frame_idx = 0;
    gc.current_fps = 60.0;

    gc.hand_cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    gc.regular_cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);

    set_dark_mode(gc.window);
    set_window_icon("..\\Resources\\icon.png");

    return true;
}

int main(void)
{   
    if(!init_all())
    {
        exit(EXIT_FAILURE);
    }

    while(!glfwWindowShouldClose((GLFWwindow*)gc.window))
    {
        update_all();
        
        PROFILE("Rendering all")
        {
            render_all();
        }
        
        present_frame(&gc.draw_buffer);
        
        PROFILE("Events")
        {
            poll_events();
        }
        
        cleanup_all();

    }
    return 0;
}
