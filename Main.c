#define WIN32_LEAN_AND_MEAN 
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
#define MAX_BLOCKS          256

/*
    Stuff that we wish to retain between frames
*/
typedef struct 
{
    i32 scroll_start_x, scroll_start_y;
    i32 scroll_x, scroll_y;

    f32 drag_start_x, drag_start_y;

    f32  value;
    bool hover;
    bool clicked;
    bool toggled;
    bool released;
    bool dragging;
    bool resizing;
    bool resize_left, resize_right, resize_top, resize_bottom;
} ui_block_state_t;

typedef struct { char *key; ui_block_state_t value; } ui_block_map_t;

typedef enum
{
    NO_LAYOUT,
    VERTICAL_LAYOUT,
    HORIZONTAL_LAYOUT,
}layout_t;

typedef enum 
{
    HIT_REGION_NONE = 0,
    HIT_REGION_LEFT_EDGE,
    HIT_REGION_RIGHT_EDGE,
    HIT_REGION_TOP_EDGE,
    HIT_REGION_BOTTOM_EDGE,
    HIT_REGION_DRAG_HANDLE,
} hit_region_t;

/*
    a block is the basic unit of all widgets
    it should provide all necessary information 
    required to create more complex widgets
*/
typedef struct ui_block_t
{
    // also used to calculate the ID that is used
    // to uniquely identify the block
    // Ensure that it is unique !
    char *title;

    u32 id;

    // Each block has a single layout option 
    // so its easier to calculate later
    layout_t layout;

    // position relative to parent
    i32 x, y;                    
    i32 w, h;

    // the absolute screen position
    i32 abs_x, abs_y;

    // Blocks can be scrolled, so they can contain
    // stuff beyond its dimensions we have to keep track
    // of dimensions on screen as well as its overall capacity
    i32 content_width, content_height;

    i32 child_spacing;
    i32 padding_top, padding_bottom, padding_left, padding_right;
    i32 default_child_height, default_child_width;
    i32 border_width;

    // stuff inside the panel should only be drawn inside the panel
    scissor_region_t scissor_region;

    i32 scroll_start_x, scroll_start_y;
    i32 scroll_x, scroll_y; 
    i32 max_scroll_x, max_scroll_y;
    
    color4_t bg_color;
    color4_t hover_color;
    color4_t border_color;

    // all the possible interactions and states block can have
    // stuff not needed can simply be ignored
    bool enabled;
    bool visible;

    bool hover;

    bool clickable;
    bool clicked;
    bool released;

    bool scrollable;

    bool draggable;
    bool dragging;

    bool resizeable;
    bool resizing;
    bool resize_left, resize_right, resize_top, resize_bottom;

    bool toggleable;
    bool toggled;

    f32 drag_start_x, drag_start_y;

    f32 value;
    f32 min_value;
    f32 max_value;

    /* used to animate each has value from 0->1 */
    f32 hot_anim_t;
    f32 active_anim_t;

    rendered_text_tt text;

    // pass functions to control how the block is rendered
    // and wheter you want to do custom update on it
    void (*custom_render)(struct ui_block_t *block);
    void (*custom_update)(struct ui_block_t *block);

    // if you want to add custom stuff
    void *user_data;
    
    // we want to traverse the tree forward and backward
    struct ui_block_t *parent;
    struct ui_block_t *first;
    struct ui_block_t *next;
    struct ui_block_t *prev;
    struct ui_block_t *last;

    i32 child_count;
    
    bool is_leaf; 
}ui_block_t;

#define IS_CONTAINER(b) (!(b)->is_leaf)
#define IS_LEAF(b) ((b)->is_leaf)

#define LIST_FOR_EACH_UP(pos, head) \
    for ((pos) = (head); (pos) != NULL; (pos) = (pos)->parent)

#define LIST_FOR_EACH_UP_REVERSE(pos, tail) \
    for ((pos) = (tail); (pos) != NULL; (pos) = (pos)->prev)

#define LIST_FOR_EACH_DOWN(pos, head) \
    for ((pos) = (head); (pos) != NULL; (pos) = (pos)->next)

typedef struct
{
    ui_block_t *root_block;
    ui_block_t *current_block;          

    ui_block_t *modal_block;
    u32 modal_active;

    ui_block_t **blocks;
    i32 block_count;

    // flatten the post order
    ui_block_t **layout_order;
    i32 layout_count;

    u32 hot_block_id;
    u32 active_block_id;
} ui_context_t;

#define TEXT_BUFFER_MAX 512

typedef struct 
{
    char buffer[TEXT_BUFFER_MAX];
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

    image_view_t        draw_buffer;
    u32                 screen_width;
    u32                 screen_height;

    GLuint              texture;
    GLuint              read_fbo;
    GLuint              shader;
    GLuint              quad_vao;
    GLuint              quad_vbo;

    GLFWcursor          *hand_cursor;
    GLFWcursor          *regular_cursor;

    i32                 mouse_x;
    i32                 mouse_last_x;
    i32                 mouse_y;
    i32                 mouse_last_y;
    bool                first_mouse;
    bool                left_button_down;
    bool                right_button_down;

    bool                key_pressed[GLFW_KEY_LAST];
    bool                key_just_pressed[GLFW_KEY_LAST];

    char                input_char_queue[32];
    i32                 input_char_count;

    ui_context_t        *ui_ctx;
    ui_block_map_t      *block_map;
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
    bool                debug;

    bool                scroll;
    f32                 scroll_x;
    f32                 scroll_y;

    bool                mouse_move;
    f32                 mouse_xoffs;
    f32                 mouse_yoffs;

    bool                mouse_btn;


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
    arena_t             *persistent_arena;  // Survives reloads
}gc;

char frametime[BUFFER_SIZE];

char prof_buf[64][BUFFER_SIZE];
i32 prof_buf_count;

void render_all(void);
void ui_scroll_block(ui_block_t *block, i32 delta_x, i32 delta_y);
void ui_handle_mouse_move(ui_context_t *ctx, f32 mouse_x, f32 mouse_y);
void ui_handle_mouse_button(ui_context_t *ctx);
void ui_handle_scroll(ui_context_t *ctx, f32 scroll_x, f32 scroll_y);
void ui_block_abs_pos (ui_block_t *block, i32 *abs_x, i32 *abs_y);
void ui_block_load_state(ui_block_t *block);
void ui_block_save_state(ui_block_t *block);
void ui_update_block_interaction(ui_block_t *block);
void ui_render_block(ui_block_t *block);
void reset_block_interaction(ui_block_t *block);
void ui_render_modal(ui_context_t *ctx);
ui_block_t *ui_block_find_by_id(ui_context_t *ctx, u32 block_id);
bool is_in_drag_handle(ui_block_t *block, i32 mouse_x, i32 mouse_y, rect_t *drag_rect);
hit_region_t is_in_hit_region(ui_block_t *block, i32 mx, i32 my, rect_t *drag_rect_out);
void ui_calculate_absolute_positions(ui_context_t *ctx);

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
    GLFWimage icon;
    icon.pixels = stbi_load(path, &icon.width, &icon.height, NULL, 4);
    if (icon.pixels) {
        glfwSetWindowIcon(gc.window, 1, &icon);
        stbi_image_free(icon.pixels);
    } else {
        fprintf(stderr, "Error : Failed to load window icon\n");
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

            prof_buf_count++;
        }
    }
}

void render_prof_entries(void)
{
    i32 height = get_line_height(gc.font)*prof_buf_count;
    i32 width = 0;

    for(int i = 0; i < prof_buf_count; i++)
    {
        width = MAX(width, (int)get_string_width(gc.font, prof_buf[i]));
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
        gc.key_just_pressed[i] = false;
    }
    gc.input_char_count = 0;
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

void drop_callback(GLFWwindow* window, int count, const char** paths) 
{
    (void)window;
    for (int i = 0; i < count; i++){
        printf("Dropped file: %s\n", paths[i]);
    }
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

    gc.mouse_xoffs+=xoffset;
    gc.mouse_yoffs+=xoffset;

    gc.mouse_move = true;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) 
{
    (void)window;
    (void)xoffset;
    (void)yoffset;

    f32 scroll_sensitivity = 25.0f;
    gc.scroll_x += (f32)xoffset * scroll_sensitivity;
    gc.scroll_y += (f32)yoffset * scroll_sensitivity;

    gc.scroll = true;
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
    gc.mouse_btn = true;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) 
{
    (void)window;
    (void)scancode;
    (void)mods;

    if (action == GLFW_PRESS) 
    {
        gc.key_pressed[key] = true;
        gc.key_just_pressed[key] = true;
    } 
    else if (action == GLFW_RELEASE) 
    {
        gc.key_pressed[key] = false;
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
            case GLFW_KEY_F8:
                gc.debug ^= 1;
                break;
            case GLFW_KEY_F7:
                gc.dock = !gc.dock;
                if(gc.dock){
                    animation_start((u64)&gc.side_panel_x, gc.side_panel_x, 0.5f, 1, EASE_IN_OUT_CUBIC);
                }else{
                    animation_start((u64)&gc.side_panel_x, gc.side_panel_x, 0.01f, 1, EASE_IN_OUT_CUBIC);
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
        if (gc.input_char_count < 31) {
            gc.input_char_queue[gc.input_char_count++] = (char)codepoint;
        }
    }
}

void poll_events(void)
{
    glfwPollEvents();
    // glfwWaitEvents(); // find a way to make it work with animations
    // glfwWaitEventsTimeout(0.016);
}

ui_context_t* ui_new_ctx(void)
{
    ui_context_t *ctx = (ui_context_t*)calloc(1, sizeof(ui_context_t));
    gc.block_map = NULL;

    return ctx;
}

void ui_new_frame(ui_context_t *ctx)
{
    ctx->blocks = ARENA_ALLOC_ZEROED(gc.frame_arena, MAX_BLOCKS, sizeof(ui_block_t*));
    ctx->block_count = 0;

    ctx->layout_order = ARENA_ALLOC_ZEROED(gc.frame_arena, MAX_BLOCKS, sizeof(ui_block_t*));
    ctx->layout_count = 0;
}

/* Create a block with defaults */
ui_block_t* ui_new_block(ui_context_t *ctx, char *title, i32 x, i32 y, u32 width, u32 height, layout_t layout)
{
    if (ctx->block_count >= MAX_BLOCKS){
        fprintf(stderr, "Error : failed to add a new block, exceeded maximum number of blocks");
        return NULL;
    } 
    
    ui_block_t *block = ARENA_ALLOC_ZEROED(gc.frame_arena, 1, sizeof(ui_block_t));
    
    block->id = djb2_hash(title);
    
    // check for duplicates
    for (i32 i = 0; i < ctx->block_count; i++){
        if (ctx->blocks[i]->id == block->id){
            fprintf(stderr, "Error : failed to add a new block, duplicate ID");
            return NULL;
        }
    }

    block->enabled = true;
    block->visible = true;

    block->title = title;
    block->layout = layout;

    block->x = block->scissor_region.x = x;
    block->y = block->scissor_region.y = y;
    block->w = block->scissor_region.w = width;
    block->h = block->scissor_region.h = height;

    block->content_width  = width;
    block->content_height = height;

    block->bg_color = HEX_TO_COLOR4(0x282a36);
    block->border_color = HEX_TO_COLOR4(0x3a3d4e);
    block->hover_color = HEX_TO_COLOR4(0x363948);

    block->border_width = 5;
    block->child_spacing = 15;
    block->padding_top = block->border_width + block->child_spacing;
    block->padding_bottom = block->padding_left = block->padding_right = block->padding_top;

    block->default_child_height = 50;
    block->default_child_width = 20;

    block->text.font = gc.font;
    block->text.string = block->title;
    block->text.color = HEX_TO_COLOR4(0xf8f8f2);

    ctx->blocks[ctx->block_count++] = block;

    /*
        Load any persistent state from previous frame 
     */
    ui_block_load_state(block);

    // if not hot just reset all interaction states
    if(block->id != ctx->hot_block_id)
    {
        block->hover = false;
        block->released = false;
    }

    if(block->id != ctx->active_block_id)
    {
        block->dragging = false;
        block->resizing = false;
        block->clicked = false;
    }
    
    return block;
}

/* 
    parent<=>child<=>child<=>child...
 */
void ui_add_child(ui_context_t *ctx, ui_block_t *child)
{
    child->parent = ctx->current_block;
    child->next = NULL;
    child->prev = NULL;

    // first child
    if (ctx->current_block->first == NULL)
    {
        ctx->current_block->first = child;
        ctx->current_block->last = child;
    }
    else
    {
        child->prev = ctx->current_block->last;
        ctx->current_block->last->next = child;
        ctx->current_block->last = child;
    }
    ctx->current_block->child_count++;

    // leaf nodes just add them to the layout order
    if(IS_LEAF(child)){
        ctx->layout_order[ctx->layout_count++]=child;
    }
}

void ui_set_block_content_size(ui_block_t *block, i32 content_width, i32 content_height)
{
    block->content_width = content_width;
    block->content_height = content_height;
    
    /* 
        Maximum scroll area is the difference between actual content size 
        and the viewport size so that in a sense maximum scroll = all content
    */
    block->max_scroll_x = ClampBot((content_width - block->w), 0);
    block->max_scroll_y = ClampBot((content_height - block->h), 0);
    
    block->scroll_x = Clamp(0, block->scroll_x, block->max_scroll_x);
    block->scroll_y = Clamp(0, block->scroll_y, block->max_scroll_y);
}

/* 
    any drawing inside a block is in a way relative to it
    you dont want to really think about the absolute position
    of a child element you want to just specify its position
    relative to its parent so we have to find a way to make this
    conversion and calculate its screen position when needed
 */
void ui_block_to_screen_coords(ui_block_t *block, i32 block_x, i32 block_y, i32 *screen_x, i32 *screen_y)
{
   *screen_x = block->abs_x + block_x;
   *screen_y = block->abs_y + block_y;
}

void ui_screen_to_block_coords(ui_block_t *block, i32 screen_x, i32 screen_y, i32 *block_x, i32 *block_y)
{
    *block_x = screen_x - block->abs_x;
    *block_y = screen_y - block->abs_y;
}

bool ui_point_in_block(ui_block_t *block, i32 screen_x, i32 screen_y)
{
    if (!block->visible)
    {
        return false;
    }
    
    rect_t block_screen_rect = {
        .x = block->abs_x,
        .y = block->abs_y,
        .w = (i32)block->w,
        .h = (i32)block->h
    };

    return inside_rect(screen_x, screen_y, &block_screen_rect);
}

ui_block_t* ui_block_at_point(ui_context_t *ctx, i32 screen_x, i32 screen_y)
{
    // get the child before the parent
    for (i32 i = ctx->block_count - 1; i >= 0; i--) 
    {
        if (ui_point_in_block(ctx->blocks[i], screen_x, screen_y)) {
            return ctx->blocks[i];
        }
    }
    return NULL;
}

ui_block_t *ui_block_find_by_id(ui_context_t *ctx, u32 block_id)
{
    for(i32 i = ctx->block_count-1 ; i >= 0; i--)
    {
        if(ctx->blocks[i]->id == block_id)
        {
            return ctx->blocks[i];
        }
    }
    return NULL;
}

/*
    Functions to draw relative to the block
 */
void ui_set_pixel_in_block(ui_block_t *block, i32 block_x, i32 block_y, color4_t color)
{
    i32 screen_x, screen_y;
    ui_block_to_screen_coords(block, block_x, block_y, &screen_x, &screen_y);
    set_pixel_scissored(&gc.draw_buffer, screen_x, screen_y, color);
}

void ui_draw_rect_in_block(ui_block_t *block, i32 block_x, i32 block_y, u32 width, u32 height, color4_t color)
{
    i32 screen_x, screen_y;
    ui_block_to_screen_coords(block, block_x, block_y,
                                     &screen_x, &screen_y);
    draw_rect_scissored(&gc.draw_buffer, screen_x, screen_y,
                         width, height, color);
}

void ui_draw_rounded_rect_in_block(ui_block_t *block, i32 block_x, i32 block_y, u32 width, u32 height, color4_t color)
{
    i32 screen_x, screen_y;
    ui_block_to_screen_coords(block, block_x, block_y,
                                     &screen_x, &screen_y);
    draw_rounded_rect_scissored(&gc.draw_buffer, screen_x, screen_y,
                                 width, height, 25, color);
}

void ui_render_text_in_block(ui_block_t *block)
{
    i32 screen_x, screen_y;
    ui_block_to_screen_coords(block, block->text.pos.x, block->text.pos.y,
                             &screen_x, &screen_y);
    
    block->text.pos.x = screen_x;
    block->text.pos.y = screen_y;

    render_text_tt_scissored(&gc.draw_buffer, &block->text);
}

void ui_draw_block_background(ui_block_t *block)
{
    if (!block->visible)
    {
        return;
    }

    draw_rect_scissored(&gc.draw_buffer, 
                        block->abs_x, 
                        block->abs_y, 
                        block->w, 
                        block->h, 
                        block->bg_color);

    if(block->draggable || block->resizeable)
    {
        rect_t handle_rect;
        if(is_in_hit_region(block, gc.mouse_x, gc.mouse_y, &handle_rect))
        {
            draw_rect_scissored(&gc.draw_buffer,
                                handle_rect.x,
                                handle_rect.y,
                                handle_rect.w,
                                handle_rect.h,
                                lite(block->bg_color));
        }
    }
    if (block->border_width > 0) 
    {
        draw_rect_scissored(&gc.draw_buffer,
                            block->abs_x,
                            block->abs_y,
                            block->w,
                            block->border_width,
                            block->border_color);
        
        draw_rect_scissored(&gc.draw_buffer,
                            block->abs_x,
                            block->abs_y + block->h - block->border_width,
                            block->w,
                            block->border_width,
                            block->border_color);
        
        draw_rect_scissored(&gc.draw_buffer,
                            block->abs_x,
                            block->abs_y,
                            block->border_width,
                            block->h,
                            block->border_color);
        
        draw_rect_scissored(&gc.draw_buffer,
                            block->abs_x + block->w - block->border_width,
                            block->abs_y,
                            block->border_width,
                            block->h,
                            block->border_color);
    }
}

void ui_scroll_block(ui_block_t *block, i32 delta_x, i32 delta_y)
{
    if (!block->scrollable){
        return;
    }
    
    block->scroll_x += delta_x;
    block->scroll_y += delta_y;
    
    block->scroll_x = Clamp(0, block->scroll_x, block->max_scroll_x);
    block->scroll_y = Clamp(0, block->scroll_y, block->max_scroll_y);

    ui_calculate_absolute_positions(gc.ui_ctx);

    // save scroll values
    ui_block_map_t *state = shgetp_null(gc.block_map, block->title);
    if(state)
    {
        state->value.scroll_x = block->scroll_x;
        state->value.scroll_y = block->scroll_y;
    }
}

void ui_handle_mouse_move(ui_context_t *ctx, f32 delta_x, f32 delta_y)
{
    (void)delta_x;
    (void)delta_y;

    ui_block_t *target = ui_block_at_point(ctx, (i32)gc.mouse_x, (i32)gc.mouse_y);

    // block interaction not inside the modal block when active
    if (ctx->modal_active && ctx->modal_block) 
    {
        if (!ui_point_in_block(ctx->modal_block, (i32)gc.mouse_x, (i32)gc.mouse_y)) {
            return;
        }
    }
        
    if(target && target->visible)
    {
        ctx->hot_block_id = target->id;

        // was not hot last frame
        if(!target->hover)
        {
            target->hover = true;
            if(!animation_get(target->id, &target->hot_anim_t)){
                animation_start((u64)target->id, 0.0f, 1.0f, 0.2f, EASE_IN_OUT_CUBIC);
            }
        }

        // do custom update if required
        if(target->custom_update)
        {
            target->custom_update(target);
        }
        ui_block_save_state(target);

        glfwSetCursor(gc.window, gc.hand_cursor);
    }
    else
    {
        glfwSetCursor(gc.window, gc.regular_cursor);
        ctx->hot_block_id = 0;
    }
}

hit_region_t is_in_hit_region(ui_block_t *block, i32 mx, i32 my, rect_t *drag_rect_out) 
{
    if (!block->visible) return HIT_REGION_NONE;
    
    i32 drag_handle_height = block->border_width + block->padding_top;
    i32 resize_width = 10;

    rect_t drag_handle = {.x = block->abs_x, .y = block->abs_y,
                          .w = block->w, .h = drag_handle_height};
    rect_t left_edge = {.x = block->abs_x, .y = block->abs_y,
                        .w = resize_width, .h = block->h};
    rect_t right_edge = {.x = block->abs_x + block->w - resize_width, .y = block->abs_y,
                         .w = resize_width, .h = block->h};
    rect_t top_edge = {.x = block->abs_x, .y = block->abs_y,
                       .w = block->w, .h = resize_width};
    rect_t bottom_edge = {.x = block->abs_x, .y = block->abs_y + block->h - resize_width,
                          .w = block->w, .h = resize_width};
    
    if (inside_rect(mx, my, &left_edge)) 
    {
        if (drag_rect_out) {
            *drag_rect_out = left_edge;
        }
        return HIT_REGION_LEFT_EDGE;
    }
    
    if (inside_rect(mx, my, &right_edge)) 
    {
        if (drag_rect_out) {
            *drag_rect_out = right_edge;
        }
        return HIT_REGION_RIGHT_EDGE;
    }
    
    if (inside_rect(mx, my, &top_edge)) 
    {
        if (drag_rect_out) {
            *drag_rect_out = top_edge;
        }
        return HIT_REGION_TOP_EDGE;
    }
    
    if (inside_rect(mx, my, &bottom_edge)) 
    {
        if (drag_rect_out) {
            *drag_rect_out = bottom_edge;
        }
        return HIT_REGION_BOTTOM_EDGE;
    }
    
    if (inside_rect(mx, my, &drag_handle)) 
    {
        if (drag_rect_out) {
            *drag_rect_out = drag_handle;
        }
        return HIT_REGION_DRAG_HANDLE;
    }
    
    return HIT_REGION_NONE;
}

void ui_handle_mouse_button(ui_context_t *ctx)
{
    ui_block_t *target = ui_block_at_point(ctx, (i32)gc.mouse_x, (i32)gc.mouse_y);

    if (target && target->visible)
    {
        if (gc.left_button_down)
        {
            if(target->hover)
            {
                ctx->active_block_id = target->id;

                target->clicked = true;
                printf("Block : %s clicked\n", target->title);

                rect_t drag_rect;
                hit_region_t region = is_in_hit_region(target, gc.mouse_x, gc.mouse_y, &drag_rect);

                switch(region)
                {
                    case HIT_REGION_NONE:
                        break;
                    case HIT_REGION_LEFT_EDGE:
                        if(target->resizeable && !target->resizing)
                        {
                            target->resizing = true;
                            target->resize_left = true;
                            target->drag_start_x = target->abs_x - gc.mouse_x;
                        }
                        break;
                    case HIT_REGION_RIGHT_EDGE:
                        if(target->resizeable && !target->resizing)
                        {
                            target->resizing = true;
                            target->resize_right = true;
                            target->drag_start_x = (target->abs_x + target->w) - gc.mouse_x;
                        }
                        break;
                    case HIT_REGION_TOP_EDGE:
                        if(target->resizeable && !target->resizing)
                        {
                            target->resizing = true;
                            target->resize_top = true;
                            target->drag_start_y = target->abs_y - gc.mouse_y;
                        }
                        break;
                    case HIT_REGION_BOTTOM_EDGE:
                        if(target->resizeable && !target->resizing)
                        {
                            target->resizing = true;
                            target->resize_bottom = true;
                            target->drag_start_y = (target->abs_y + target->h) - gc.mouse_y;
                        }
                        break;
                    case HIT_REGION_DRAG_HANDLE:
                        if(target->draggable && !target->dragging)
                        {
                            target->dragging = true;
                            target->drag_start_x = target->abs_x - gc.mouse_x;
                            target->drag_start_y = target->abs_y - gc.mouse_y;
                        }
                        break;
                }
            }
        }
        else
        {
            // Was clicked and not anymore
            if(ctx->active_block_id == target->id)
            {
                target->released = true;
                printf("Block : %s released\n", target->title);
                if (target->toggleable) {
                    target->toggled = !target->toggled;
                }
            }
            else
            {
                target->released  = false;
            }

            target->clicked = false;
            target->dragging = false;
            target->resizing = false;
            target->resize_left = target->resize_right = false;
            target->resize_top = target->resize_bottom = false;

            ctx->active_block_id = 0;
        }
        
        if (gc.right_button_down) 
        {
        } 
        else 
        {
        }

        ui_block_save_state(target);
    }
    else
    {
        ctx->active_block_id = 0;
    }

}

void ui_handle_scroll(ui_context_t *ctx, f32 scroll_x, f32 scroll_y)
{
    ui_block_t *target = ui_block_at_point(ctx, (i32)gc.mouse_x, (i32)gc.mouse_y);

    if(target && target->visible)
    {
        // traverse upwards until you find a scrollable block
        ui_block_t *current = NULL;
        LIST_FOR_EACH_UP(current, target)
        {
            if (current && current->scrollable) {
                ui_scroll_block(current, (i32)(scroll_x), (i32)(scroll_y));
                // scrolling will change cursor position relative to the blocks
                ui_handle_mouse_move(ctx, 0.0f, 0.0f);
                return;
            }
        }
    }
}

void ui_calculate_absolute_positions(ui_context_t *ctx)
{
    for (i32 i = 0; i < ctx->block_count; i++)
    {
        ui_block_t *block = ctx->blocks[i];
        if (!block) continue;
        
        if (block->parent == NULL)
        {
            // Root block
            block->abs_x = block->x;
            block->abs_y = block->y;
        }
        else
        {
            block->abs_x = block->parent->abs_x + block->x - block->parent->scroll_x;
            block->abs_y = block->parent->abs_y + block->y - block->parent->scroll_y;
        }
        
        block->scissor_region.x = block->abs_x;
        block->scissor_region.y = block->abs_y;
        block->scissor_region.w = block->w;
        block->scissor_region.h = block->h;
    }
}

void ui_layout_constraints(ui_block_t *block)
{
    if (block) 
    {
        // leaf blocks nothing to layout
        if(IS_LEAF(block) || !block->first)
            return;

        i32 default_item_height = 0;
        i32 default_item_width = 0;

        f32 text_width = 0;
        f32 text_height = 0;

        switch(block->layout)
        {
            case VERTICAL_LAYOUT:
            {
                // fill the entire block width and use the height in vertical layout
                default_item_width = block->w - block->padding_left - block->padding_right;
                default_item_height = block->default_child_height;

                ui_block_t *child = NULL;
                LIST_FOR_EACH_DOWN(child, block->first)
                {                        
                    if (IS_LEAF(child) && child->text.string){
                        text_height = get_line_height(child->text.font);
                    }
                    // zero dimensions -> default
                    child->w  = (child->w > 0) ? MIN(default_item_width, child->w) : default_item_width;
                    child->h  = (child->h > 0) ? child->h : MAX(default_item_height, text_height);
                }
                break;
            }
            case HORIZONTAL_LAYOUT:
            {
                // fill the entire block height when horizontal layout
                default_item_height = block->h - block->padding_top - block->padding_bottom;
                default_item_width = block->default_child_width;

                ui_block_t *child = NULL;
                LIST_FOR_EACH_DOWN(child,block->first)
                {
                    if (IS_LEAF(child) && child->text.string){
                        text_width = get_string_width(child->text.font, child->text.string);
                    }
                    child->h = (child->h > 0) ? MIN(child->h, default_item_height) : default_item_height;  
                    child->w = (child->w > 0) ? child->w : MAX(default_item_width,text_width);
                }

                break;
            }
            case NO_LAYOUT:
                break;
        }
    }
}

bool is_block_visible(ui_block_t *child, ui_block_t *parent)
{
    if(!parent || !child)
        return true;

    rect_t child_rect = {
        .x = child->abs_x,
        .y = child->abs_y,
        .w = child->w,
        .h = child->h
    };

    rect_t parent_scissor = {
        .x = parent->abs_x,
        .y = parent->abs_y,
        .w = parent->w,
        .h = parent->h
    };

    rect_t intersection = intersect_rects(&child_rect, &parent_scissor);

    return (intersection.x || intersection.y || intersection.w || intersection.h);
}

void ui_block_layout(ui_context_t *ctx, ui_block_t *block)
{
    if (block) 
    {
        // leaf blocks nothing to layout
        if(IS_LEAF(block) || !block->first)
            return;

        const i32 item_spacing = block->child_spacing;
        const i32 padding_top = block->padding_top;
        const i32 padding_bottom = block->padding_bottom;
        const i32 padding_left = block->padding_left;
        const i32 padding_right = block->padding_right;

        i32 default_item_height = block->default_child_height;
        i32 default_item_width = block->default_child_width;

        i32 content_height = 0;
        i32 content_width = 0;

        switch(block->layout)
        {
            case VERTICAL_LAYOUT:
            {
                /* 
                    First pass calculate the size of the content
                 */
                ui_block_t *child = NULL;
                LIST_FOR_EACH_DOWN(child, block->first)
                {
                    i32 child_width  = child->w;
                    i32 child_height = child->h;

                    content_width = MAX(child_width, content_width);
                    content_height += child_height;
                    
                    if (child->next) {
                        content_height += item_spacing;
                    }
                }

                content_height += padding_top + padding_bottom;
                content_width += padding_left + padding_right;

                i32 current_y = padding_top;
                i32 base_x = padding_left;

                /* 
                    Second pass position stuff in the right place
                 */
                child = NULL;
                LIST_FOR_EACH_DOWN(child,block->first)
                {
                    child->x = child->x + base_x;
                    child->y = current_y;
                    
                    // Only leaf block can have a text item
                    if (IS_LEAF(child) && child->text.string)
                    {
                        f32 text_width = get_string_width(child->text.font, child->text.string);
                        f32 text_height = get_line_height(child->text.font);
                        
                        i32 center_x = ((child->w - text_width)/2);
                        i32 center_y = (default_item_height-text_height)/2;

                        child->text.pos.x = center_x < 0 ? 10 : center_x;
                        child->text.pos.y = center_y < 0 ? 10 : center_y;
                    }

                    current_y += child->h + item_spacing;
                }
                break;
            }
            case HORIZONTAL_LAYOUT:
            {
                ui_block_t *child = NULL;
                LIST_FOR_EACH_DOWN(child,block->first)
                {
                    i32 child_width  = child->w;
                    i32 child_height = child->h;  

                    content_width += child_width;
                    content_height = MAX(child_height, content_height);

                    if (child->next) {
                        content_width += item_spacing;
                    }
                }

                content_height += padding_top + padding_bottom;
                content_width += padding_left + padding_right;

                i32 current_x = padding_left;
                i32 base_y = padding_top;

                /* 
                    Second pass position stuff in the right place
                 */
                child = NULL;
                LIST_FOR_EACH_DOWN(child,block->first)
                {
                    child->x = current_x;
                    child->y = base_y;

                    if (IS_LEAF(child) && child->text.string)
                    {
                        f32 text_width = get_string_width(child->text.font, child->text.string);
                        f32 text_height = get_line_height(child->text.font);

                        child->text.pos.x = (child->w - text_width)/2;
                        child->text.pos.y = (default_item_height-text_height)/2;
                    }

                    current_x += child->w + item_spacing;
                }

                break;
            }
            case NO_LAYOUT:
                break;
        }

        ui_set_block_content_size(block, content_width, content_height);
    }
}

void draw_scroll_bar(ui_block_t *block)
{
    if (block->max_scroll_y <= 0) {
        return;
    }
    
    i32 gap = block->border_width * 4;
    i32 scroll_thumb_width = block->border_width * 2;
    
    i32 scroll_bar_x = block->w - gap + block->border_width;
    
    i32 content_height = block->max_scroll_y + block->h;

    if (content_height <= 0){
        return;
    }
    
    f32 visible_ratio = Clamp(((f32)block->h / (f32)content_height), 0.1f, 1.0f);
    
    i32 scroll_thumb_height = (i32)(block->h * visible_ratio);
    scroll_thumb_height = MAX(scroll_thumb_height, scroll_thumb_width * 2);

    i32 scroll_bar_min_y = 0;
    i32 scroll_bar_max_y = scroll_bar_min_y + block->h - scroll_thumb_height;
    
    f32 scroll_bar_ratio = fabsf((f32)(block->scroll_y)/(f32)(block->max_scroll_y));

    i32 scroll_bar_y = LERP_F32(scroll_bar_min_y, scroll_bar_max_y, scroll_bar_ratio);
    
    ui_draw_rect_in_block(block, scroll_bar_x, scroll_bar_y, 
                          scroll_thumb_width, scroll_thumb_height, HEX_TO_COLOR4(0x525356));
}

void ui_begin_panel(ui_context_t *ctx, char *title, i32 x, i32 y, u32 width, u32 height, layout_t layout)
{
    ui_block_t *panel = CHECK_PTR(ui_new_block(ctx, title, x, y, width, height, layout));

    if(!panel){
        return;
    }

    panel->is_leaf = false;
    panel->scrollable = true;

    if(ctx->current_block)
    {
        ui_add_child(ctx, panel);
    }
    ctx->current_block = panel;
}

void ui_update_floating_panel(ui_block_t *block)
{
    static bool first_update = true;
    static rect_t new_pos ={0};
    static i32 snap_x = 0;
    static i32 snap_y = 0;
    static bool snap = false;

    if(first_update){
        new_pos.x = block->x;
        new_pos.y = block->y;
        new_pos.w = block->w;
        new_pos.h = block->h;
        first_update=false;
    }

    if(block->id && gc.ui_ctx->active_block_id)
    {
        if(block->dragging)
        {   
            new_pos.x = (f32)gc.mouse_x + block->drag_start_x;
            new_pos.y = (f32)gc.mouse_y + block->drag_start_y;

            snap = false;

            color4_t color = HEX_TO_COLOR4(0x1a2226);
            color.a = 100;

            if((new_pos.x > ((i32)gc.screen_width - block->w)) 
                && new_pos.y < block->h)
            {
                snap = true;
                snap_x = (gc.screen_width - block->w);
                snap_y = 0;
                draw_rect_solid_wh(&gc.draw_buffer, snap_x, snap_y,
                                   block->w,block->h, color);
            }
            else if(new_pos.x < block->w 
                    && new_pos.y < block->h)
            {
                snap = true;
                snap_x = 0;
                snap_y = 0;
                draw_rect_solid_wh(&gc.draw_buffer, snap_x, snap_y,
                                    block->w, block->h, color);
            }
            else if((new_pos.x > ((i32)gc.screen_width - block->w)) 
                && (new_pos.y > ((i32)gc.screen_height - block->h)))
            {
                snap = true;
                snap_x = (gc.screen_width - block->w);
                snap_y = (gc.screen_height - block->h);
                draw_rect_solid_wh(&gc.draw_buffer, snap_x, snap_y,
                                   block->w, block->h, color);
            }
            else if(new_pos.x < block->w 
                    && (new_pos.y > ((i32)gc.screen_height - block->h)))
            {
                color4_t color = HEX_TO_COLOR4(0x1a2226);
                color.a = 100;

                snap = true;
                snap_x = 0;
                snap_y = (gc.screen_height - block->h);
                draw_rect_solid_wh(&gc.draw_buffer, snap_x, snap_y,
                                    block->w, block->h, color);
            }
        }

        if (block->resizing)
        {
            if (block->resize_left) {
                i32 delta = gc.mouse_x + block->drag_start_x - new_pos.x;
                new_pos.x += delta;
                new_pos.w -= delta;
            }
            if (block->resize_right) {
                new_pos.w = gc.mouse_x + block->drag_start_x - new_pos.x;
            }
            if (block->resize_top) {
                i32 delta = gc.mouse_y + block->drag_start_y - new_pos.y;
                new_pos.y += delta;
                new_pos.h -= delta;
            }
            if (block->resize_bottom) {
                new_pos.h = gc.mouse_y + block->drag_start_y - new_pos.y;
            }
            
            if (new_pos.w < 100) new_pos.w = 100;
            if (new_pos.h < 100) new_pos.h = 100;
        }
    }
    else
    {
        if(snap)
        {
            new_pos.x=snap_x;
            new_pos.y=snap_y;
        }
    }

    block->x=new_pos.x;
    block->y=new_pos.y;
    block->w=new_pos.w;
    block->h=new_pos.h;

    ui_calculate_absolute_positions(gc.ui_ctx);
}

void ui_begin_panel_floating(ui_context_t *ctx, char *title, i32 x, i32 y, u32 width, u32 height, layout_t layout)
{
    ui_block_t *panel = CHECK_PTR(ui_new_block(ctx, title, x, y, width, height, layout));

    if(!panel){
        return;
    }

    panel->is_leaf    = false;
    panel->scrollable = true;
    panel->draggable  = true;
    panel->resizeable = true;

    panel->custom_update = ui_update_floating_panel;

    // if(ctx->current_block)
    // {
    //     ui_add_child(ctx, panel);
    // }
    ctx->current_block = panel;
}

void ui_end_panel(ui_context_t *ctx)
{
    ctx->layout_order[ctx->layout_count++] = ctx->current_block;
    ctx->current_block = ctx->current_block->parent;
}

void ui_layout(ui_context_t *ctx)
{
    for (i32 i = ctx->layout_count - 1; i >= 0; i--)
    {
        ui_block_t *block = ctx->layout_order[i];
        ui_layout_constraints(block);
    }

    for (i32 i = 0; i < ctx->layout_count; i++)
    {
        ui_block_t *block = ctx->layout_order[i];
        ui_block_layout(ctx, block); 
    }

    ui_calculate_absolute_positions(ctx);

    /* Mark non-visible blocks */
    for (i32 i = ctx->layout_count - 1; i >= 0; i--)
    {
        ui_block_t *block = ctx->layout_order[i];
        
        if (!block->parent) {
            block->visible = true;
            continue;
        }
        
        block->visible = true;
        
        ui_block_t *ancestor;
        LIST_FOR_EACH_UP(ancestor, block->parent)
        {
            if (!ancestor->visible) {
                block->visible = false;
                break;
            }
            
            if (!is_block_visible(block, ancestor)) {
                block->visible = false;
                break;
            }
        }
    }
}

void ui_update(ui_context_t *ctx)
{
    if(gc.mouse_move)
    {
        PROFILE("Hit test"){
            ui_handle_mouse_move(ctx, gc.mouse_xoffs, gc.mouse_yoffs);
        }
        gc.mouse_move = false;
        gc.mouse_xoffs=0;
        gc.mouse_yoffs=0;
    }

    if(gc.scroll)
    {
        ui_handle_scroll(ctx, gc.scroll_x, -gc.scroll_y);
        gc.scroll = false;
        gc.scroll_x = 0;
        gc.scroll_y = 0;
    }

    if(gc.mouse_btn)
    {
        ui_handle_mouse_button(ctx);
        gc.mouse_btn = false;
    }
}

void ui_render(ui_context_t *ctx)
{
    for (i32 i = 0; i < ctx->block_count; i++)
    {
        if (ctx->blocks[i]->parent == NULL)
        {
            ui_render_block(ctx->blocks[i]);
        }
    }
    // always rendered last
    ui_render_modal(ctx);
}

void ui_block_load_state(ui_block_t *block)
{
    ui_block_map_t *state = shgetp_null(gc.block_map, block->title);

    if(state)
    {
        block->value = state->value.value;
        block->hover = state->value.hover;
        block->clicked = state->value.clicked;
        block->toggled = state->value.toggled;
        block->released = state->value.released;
        block->dragging = state->value.dragging;
        block->resizing = state->value.resizing;
        block->resize_left = state->value.resize_left;
        block->resize_right = state->value.resize_right;
        block->resize_top = state->value.resize_top;
        block->resize_bottom = state->value.resize_bottom;
        block->drag_start_x = state->value.drag_start_x; 
        block->drag_start_y = state->value.drag_start_y;
        block->scroll_y = state->value.scroll_y;
        block->scroll_x = state->value.scroll_x;
    }
    else
    {
        ui_block_state_t state = {0};
        shput(gc.block_map, block->title, state);
    }
}

void ui_block_save_state(ui_block_t *block)
{
    ui_block_state_t state = {
        .value = block->value,
        .hover = block->hover,
        .clicked = block->clicked,
        .toggled = block->toggled,
        .released = block->released,
        .dragging = block->dragging,
        .resizing   = block->resizing,
        .resize_left = block->resize_left,
        .resize_right = block->resize_right,
        .resize_top   = block->resize_top,
        .resize_bottom = block->resize_bottom,
        .drag_start_x = block->drag_start_x,
        .drag_start_y = block->drag_start_y,
        .scroll_y = block->scroll_y,
        .scroll_x = block->scroll_x,
    };

    shput(gc.block_map, block->title, state);
}

void ui_update_block_interaction(ui_block_t *block)
{
    if (!block->enabled) 
    {
        return;
    }

    if (block->custom_update) 
    {
        block->custom_update(block);
    }

    ui_block_save_state(block);
}

void ui_render_block(ui_block_t *block)
{
    if(!block || !block->visible){
        return;
    }
    
    ui_update_block_interaction(block);

    if (block->custom_render) 
    {
        block->custom_render(block);
    }
    else
    {
        // default way to render a block
        if(IS_CONTAINER(block))
        {
            ui_draw_block_background(block);
        }
        else
        {
            color4_t color = block->hover?block->hover_color:block->bg_color;
            ui_draw_rect_in_block(block, 0, 0, 
                                  block->w, block->h, color);
            if (block->text.string) {
                ui_render_text_in_block(block);
            }
        }
    }

    push_scissor(block->scissor_region.x + block->border_width,
                block->scissor_region.y + block->border_width,
                block->scissor_region.w - 2 * block->border_width,
                block->scissor_region.h - 2 *  block->border_width);


        ui_block_t *child = NULL;
        LIST_FOR_EACH_DOWN(child, block->first)
        {
            ui_render_block(child);
        }

        if(block->scrollable)
        {
            draw_scroll_bar(block);
        }
    pop_scissor();
}

void reset_block_interaction(ui_block_t *block)
{
    block->hover = false;
    // block->released = false;
    // block->clicked = false;
}

/* -------------- Checkbox Widget Stuff -------------- */

void ui_render_checkbox(ui_block_t *block)
{
    color4_t color = block->hover?block->hover_color:block->bg_color;

    ui_draw_rect_in_block(block, 0, 0, 
                          block->w, block->h, color);

    i32 box_size = 20;
    i32 box_x = 10;
    i32 box_y = ((block->h - box_size)/2);

    color4_t box_color = {200, 200, 200, 255};
    ui_draw_rect_in_block(block, box_x, box_y, 
                          box_size, box_size, box_color);

    if (block->toggled) 
    {
        i32 padding = 4;
        color4_t check_color = {100, 200, 100, 255};
        ui_draw_rect_in_block(block, 
                          box_x + padding, 
                          box_y + padding,
                          box_size - padding * 2, 
                          box_size - padding * 2,
                          check_color);
    }
    
    if (block->text.string) 
    {
        block->text.pos.x = box_x + box_size + 10;
        block->text.pos.y = (block->h - get_line_height(block->text.font)) / 2;
        ui_render_text_in_block(block);

        /*         
        rendered_text_tt icon = {
            .font = gc.icon_font,
            .pos = {.x = block->text.pos.x + get_string_width(gc.font, block->text.string)+10,
                    .y = block->text.pos.y},
            .color = {.r = 255, .g = 184, .b = 108, .a = 255},
            .string = EDIT_ICON
        };

        render_string_unicode(&gc.draw_buffer, &icon); 
        */
    }
}

bool ui_checkbox(ui_context_t *ctx, char *title)
{
    
    ui_block_t *checkbox = CHECK_PTR(ui_new_block(ctx, title, 0, 0, 0, 40, NO_LAYOUT));

    checkbox->is_leaf = true;
    checkbox->clickable = true;
    checkbox->toggleable = true;

    checkbox->bg_color = HEX_TO_COLOR4(0x47a777);
    checkbox->hover_color = lite(HEX_TO_COLOR4(0x47a777));
    checkbox->custom_render = ui_render_checkbox;
    
    ui_add_child(ctx, checkbox);

    return checkbox->toggled;
}

/* -------------- Button Widget Stuff -------------- */

void ui_render_button(ui_block_t *block)
{
    // color4_t color = block->hover?block->hover_color:block->bg_color;
    color4_t color = color4_lerp(block->bg_color,HEX_TO_COLOR4(0x47c179), block->hot_anim_t);

    float scale = 1.0f + (block->hot_anim_t * 0.05f); 
    float scaled_w = block->w * scale;
    float scaled_h = block->h * scale;
    float offset_x = (scaled_w - block->w) * 0.5f;
    float offset_y = (scaled_h - block->h) * 0.5f;
    if (block->hot_anim_t > 0.01f) {
        color4_t glow = HEX_TO_COLOR4(0x95d9ae);
        float border_thickness = 2.0f * block->hot_anim_t;
        ui_draw_rounded_rect_in_block(block,
            - offset_x - border_thickness,
            - offset_y - border_thickness,
            scaled_w + border_thickness * 2,
            scaled_h + border_thickness * 2,
            glow);
    }
    ui_draw_rounded_rect_in_block(block, 0, 0,
                                  block->w, block->h, color);
    ui_render_text_in_block(block);
}

bool ui_button(ui_context_t *ctx, char *title)
{
    if (!ctx->current_block) return false;
    
    ui_block_t *button = CHECK_PTR(ui_new_block(ctx, title, 0,0,0,0,NO_LAYOUT));

    button->is_leaf = true;
    button->clickable = true;

    button->bg_color = HEX_TO_COLOR4(0xbd93f9);
    button->hover_color = lite(HEX_TO_COLOR4(0xbd93f9));
    button->custom_render = ui_render_button;

    if(!animation_get(button->id, &button->hot_anim_t))
    {
        button->hot_anim_t = button->hover;
    }

    bool release = button->released;

    if(release){
        button->released = false;
    }

    ui_add_child(ctx, button);

    return release;
}

/* -------------- Radio Button Widget Stuff -------------- */

void ui_render_radio(ui_block_t *block)
{
    color4_t color = block->hover?block->hover_color:block->bg_color;

    ui_draw_rect_in_block(block, 0, 0, block->w, block->h, color);

    bool selected = block->user_data;

    i32 box_size = 20;
    i32 box_x = 10;
    i32 box_y = ((block->h - box_size)/2);


    color4_t box_color = {200, 200, 200, 255};
    ui_draw_rect_in_block(block, box_x, box_y, box_size, box_size, box_color);

    if (selected) 
    {
        i32 padding = 4;
        color4_t check_color = {100, 200, 100, 255};
        ui_draw_rect_in_block(block, 
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
        block->text.pos.y = (block->h - get_line_height(block->text.font)) / 2;
        ui_render_text_in_block(block);
        
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
    ui_block_t *radio = CHECK_PTR(ui_new_block(ctx, title, 0, 0, 0, 40, NO_LAYOUT));

    radio->is_leaf = true;
    radio->clickable = true;
    radio->toggleable = true;

    radio->user_data = (void *)(intptr_t)(*var == val ? 1 : 0);

    radio->bg_color = HEX_TO_COLOR4(0xf378c6);
    radio->hover_color = lite(HEX_TO_COLOR4(0xf378c6));
    radio->custom_render = ui_render_radio;

    ui_add_child(ctx, radio);

    if(radio->clicked)
    {
        *var = val;
    }
}

/* -------------- Slider Widget stuff -------------- */

void ui_render_slider(ui_block_t *block)
{
    ui_draw_rect_in_block(block, 0, 0, 
                          block->w, block->h, block->bg_color);
    
    i32 track_height = 4;
    i32 track_y = (block->h - track_height) / 2;

    f32 normalized = NORMALIZE(block->value, block->min_value, block->max_value);
    i32 handle_size = 16;
    i32 handle_x = (i32)(normalized * block->w);
    i32 handle_y = block->h / 2;

    i32 handle_start_x = handle_x - handle_size/2.0f;
    i32 handle_start_y = handle_y - handle_size/2.0f;

    ui_draw_rect_in_block(block, 0, track_y, 
                         handle_start_x, track_height, HEX_TO_COLOR4(0x6200ee));
    ui_draw_rect_in_block(block, handle_start_x, track_y,
                          block->w-handle_x, track_height, HEX_TO_COLOR4(0xcab6e6));
    
    color4_t handle_color = block->clicked ?  HEX_TO_COLOR4(0x6200ee) : dark(HEX_TO_COLOR4(0x6200ee));
    
    rect_t handle = {
        block->abs_x + handle_start_x,
        block->abs_y + handle_start_y,
        handle_size, 
        handle_size
    };

    if(inside_rect(gc.mouse_x, gc.mouse_y, &handle))
    {
        handle_color =  HEX_TO_COLOR4(0x3149c7);
    }

    draw_circle_filled_aa(&gc.draw_buffer, block->abs_x + handle_x, block->abs_y + handle_y,
                         handle_size/2.0, handle_color);
    // draw_rounded_rect_scissored(&gc.draw_buffer, handle_x, handle_y, handle_size, handle_size*2, 25, handle_color);

    
    if (block->text.string) {
        // render_text_in_block(block);
    }
}

void ui_update_slider(ui_block_t *block) 
{
    block->value = block->value < block->min_value?block->min_value:block->value;
    
    if(block->id != gc.ui_ctx->active_block_id)
    {
        block->clicked = false;
        return;
    }

    if(block->clicked)
    {
        f32 normalized = ((f32)gc.mouse_x - block->abs_x) / (f32)block->w;
        normalized = Clamp(0.0f, normalized, 1.0f);
        
        block->value = LERP_F32(block->min_value, block->max_value, normalized);
    }

}

f32 ui_slider(ui_context_t *ctx, f32 min, f32 max, char *title)
{
    ui_block_t *slider = CHECK_PTR(ui_new_block(ctx, title, 0, 0, 0, 40, NO_LAYOUT));

    slider->is_leaf = true;
    slider->clickable = true;
    slider->toggleable = true;
    slider->draggable = true;
    
    slider->min_value = min;
    slider->max_value = max;

    slider->bg_color = HEX_TO_COLOR4(0xbd93f9);
    slider->hover_color = lite(HEX_TO_COLOR4(0xbd93f9));

    slider->custom_render = ui_render_slider;
    slider->custom_update = ui_update_slider;

    ui_add_child(ctx, slider);

    return slider->value;
}

/* -------------- Tree Widget stuff -------------- */
typedef struct{
    bool collapsed;
    int depth;
}tree_node_state_t;

typedef struct {char *key; tree_node_state_t value; } tree_widget_map_t;
tree_widget_map_t *g_tree_widget_map = NULL;

void ui_update_tree_node(ui_block_t *block) 
{
}

void ui_render_tree_node(ui_block_t *block) 
{
    tree_widget_map_t *map_entry = shgetp_null(g_tree_widget_map, block->title);
    if (!map_entry) return;
    
    tree_node_state_t *node = &map_entry->value;

    bool release = block->released;

    if(release){
        block->released = false;
        TOGGLE(node->collapsed);
    }

    color4_t color = block->hover?HEX_TO_COLOR4(0x287fc9):COLOR_TRANSPARENT;

    ui_draw_rect_in_block(block, 0, 0, block->w, block->h, color);

    i32 screen_x = block->abs_x + 10;
    i32 screen_y = block->abs_y + (block->h - get_line_height(block->text.font)) / 2;

    if (block->text.string) 
    {
        if(node->collapsed)
        {
            rendered_text_tt icon = {
                .font = gc.icon_font,
                .pos = {.x = screen_x, .y = screen_y},
                .color = COLOR_WHITE,
                .string = RIGHT_CIRCLE_ARROW
            };
            render_string_unicode(&gc.draw_buffer, &icon);
        }
        else
        {
            rendered_text_tt icon = {
                .font = gc.icon_font,
                .pos = {.x = screen_x, .y = screen_y},
                .color = COLOR_WHITE,
                .string = DOWN_CIRCLE_ARROW
            };

            render_string_unicode(&gc.draw_buffer, &icon);
        }

        block->text.pos.x = 50;
        block->text.pos.y = (block->h - get_line_height(block->text.font)) / 2;

        ui_render_text_in_block(block);
    }
}
void ui_tree_node_end(ui_context_t *ctx);

bool ui_tree_node_begin(ui_context_t *ctx, char *title)
{
    tree_widget_map_t *map_entry = shgetp_null(g_tree_widget_map, title);
    if (!map_entry) 
    {
        tree_node_state_t state = {
            .collapsed = true
        };
        shput(g_tree_widget_map, title, state);
        map_entry = shgetp_null(g_tree_widget_map, title);
    }

    ui_block_t *node = CHECK_PTR(ui_new_block(ctx, title, 0, 0, 0, 0, VERTICAL_LAYOUT));

    node->is_leaf = true;
    node->clickable = true;
    node->toggleable = true;
    
    node->bg_color = HEX_TO_COLOR4(0xbd93f9);
    node->hover_color = lite(HEX_TO_COLOR4(0xbd93f9));

    if(ctx->current_block)
    {
        ui_add_child(ctx, node);
    }

    node->custom_render = ui_render_tree_node;
    node->custom_update = NULL;

    char *node_panel = ARENA_ALLOC(gc.frame_arena, 128);
    snprintf(node_panel, 128, "title-%s",title);
    if(!map_entry->value.collapsed){
        ui_begin_panel(gc.ui_ctx, node_panel, 0, 0, 0, 200, VERTICAL_LAYOUT);
    }

    return !map_entry->value.collapsed;
}

void ui_tree_node_end(ui_context_t *ctx)
{
    ui_end_panel(ctx);
}

/* -------------- Modal Widget stuff -------------- */

void ui_begin_modal(ui_context_t *ctx, char *title, i32 width, i32 height)
{
    i32 x = (gc.screen_width - width) / 2;
    i32 y = (gc.screen_height - height) / 2;
    
    ui_block_t *modal = CHECK_PTR(ui_new_block(ctx, title, x, y, width, height, VERTICAL_LAYOUT));
    
    modal->is_leaf = false;
    modal->scrollable = false;
    
    // Make it visually distinct
    modal->bg_color = HEX_TO_COLOR4(0x2e3440);
    modal->border_width = 2;
    modal->border_color = HEX_TO_COLOR4(0x88c0d0);
    
    ctx->modal_block  = modal;
    ctx->modal_active = true;

    ctx->current_block = modal;
}

void ui_end_modal(ui_context_t *ctx) {

    if (gc.key_pressed[GLFW_KEY_0]) {
        ctx->modal_active = false;
    }
}

void ui_render_modal(ui_context_t *ctx)
{
    if (ctx->modal_active && ctx->modal_block)
    {
        color4_t dim_color = {0, 0, 0, 180};
        draw_rect_solid_wh(&gc.draw_buffer, 0, 0, 
                          gc.screen_width, gc.screen_height, 
                          dim_color);
        
        ui_render_block(ctx->modal_block);
    }
}

/* -------------- Radial menu stuff -------------- */

typedef struct 
{
    char *label;
    void (*on_select)(void *user_data);
    void *user_data;
    color4_t color;
    color4_t hover_color;
    bool selected;
} radial_menu_item_t;

typedef struct 
{
    radial_layout_t layout;
    radial_menu_item_t *items;  // Dynamic array
    i32 hovered_index;
    i32 selected_index;
    bool active;
} radial_menu_state_t;

typedef struct { char *key; radial_menu_state_t value; } radial_menu_map_t;
radial_menu_map_t *g_radial_menu_map = NULL;

void ui_update_radial_menu(ui_block_t *block) 
{
    radial_menu_map_t *map_entry = shgetp_null(g_radial_menu_map, block->title);
    if (!map_entry) return;
    
    radial_menu_state_t *menu = &map_entry->value;
    if (!menu->active) return;
    
    menu->layout.center_x = block->abs_x + block->w / 2.0f;
    menu->layout.center_y = block->abs_y + block->h / 2.0f;
    
    f32 mouse_x = (f32)gc.mouse_x;
    f32 mouse_y = (f32)gc.mouse_y;
    
    menu->hovered_index = -1;
    
    for (i32 i = 0; i < arrlen(menu->items); i++) 
    {
        radial_segment_t seg = radial_get_segment(&menu->layout, i, 1.0f);
        
        if (point_in_radial_segment(mouse_x, mouse_y, &menu->layout, &seg)) 
        {
            menu->hovered_index = i;
            break;
        }
    }

    bool release = block->released;

    if(release){
        block->released = false;
    }
    
    if (release && menu->hovered_index >= 0) 
    {
        menu->selected_index = menu->hovered_index;
        
        radial_menu_item_t *item = &menu->items[menu->selected_index];
        item->selected = true;
        
        if (item->on_select) {
            item->on_select(item->user_data);
        }
        
    }
}

void ui_render_radial_menu(ui_block_t *block) 
{
    radial_menu_map_t *map_entry = shgetp_null(g_radial_menu_map, block->title);
    if (!map_entry) return;
    
    radial_menu_state_t *menu = &map_entry->value;
    if (!menu->active) return;
    
    for (i32 i = 0; i < arrlen(menu->items); i++) 
    {
        radial_menu_item_t *item = &menu->items[i];
        radial_segment_t seg = radial_get_segment(&menu->layout, i, 1.0f);
        
        color4_t color = item->color;
        if (i == menu->hovered_index) 
        {
            color = item->hover_color;
        }
        if (i == menu->selected_index) 
        {
            color = lite(color);
        }
        
        draw_radial_segment_filled(&gc.draw_buffer, &menu->layout, &seg, color);
        
        if (item->label) 
        {
            f32 mid_angle = (seg.angle_start + seg.angle_end) / 2.0f;
            f32 mid_radius = (seg.inner_radius + seg.outer_radius) / 2.0f;
            
            polar_t text_pos = { .radius = mid_radius, .angle = mid_angle };
            f32 text_x, text_y;
            polar_to_cartesian(text_pos, menu->layout.center_x, menu->layout.center_y,
                              menu->layout.scale_x, menu->layout.scale_y,
                              &text_x, &text_y);
            
            f32 text_width = get_string_width(block->text.font, item->label);
            f32 text_height = get_line_height(block->text.font);
            
            rendered_text_tt icon = {
                .font = gc.icon_font,
                .pos = {.x = text_x-10, .y = text_y-10},
                .color = HEX_TO_COLOR4(0xe7f8f2),
                .string = NOTE_ICON
            };
            render_string_unicode(&gc.draw_buffer, &icon);
        }
    }
}

void radial_menu_add_item(radial_menu_state_t *menu, char *label, void (*callback)(void*), void *user_data) 
{
    radial_menu_item_t item = {
        .label = label,
        .on_select = callback,
        .user_data = user_data,
        .color = {80, 80, 120, 255},
        .hover_color = {120, 120, 180, 255},
        .selected = false
    };
    
    arrput(menu->items, item);
    menu->layout.num_segments = arrlen(menu->items);
}

radial_menu_map_t *ui_radial_menu(ui_context_t *ctx, char *title, f32 x, f32 y, f32 radius)
{
    radial_menu_map_t *map_entry = shgetp_null(g_radial_menu_map, title);
    if (!map_entry) 
    {
        radial_menu_state_t state = {
            .layout = {
                .center_x = x,
                .center_y = y,
                .scale_x = 1.0f,
                .scale_y = 1.0f,
                .num_segments = 0,
                .inner_radius = radius * 0.3f,
                .outer_radius = radius,
                .rotation = -M_PI / 2.0f, 
                .gap_factor = 0.1f
            },
            .items = NULL,
            .hovered_index = -1,
            .selected_index = -1,
            .active = true
        };
        shput(g_radial_menu_map, title, state);
    }

    ui_block_t *radial = CHECK_PTR(ui_new_block(ctx, title, x - radius, y - radius, radius * 2, radius * 2, NO_LAYOUT));

    radial->is_leaf = true;
    radial->clickable = true;
    radial->toggleable = true;
    radial->draggable = true;
    
    radial->bg_color = HEX_TO_COLOR4(0xbd93f9);
    radial->hover_color = lite(HEX_TO_COLOR4(0xbd93f9));

    radial->custom_render = ui_render_radial_menu;
    radial->custom_update = ui_update_radial_menu;

    ui_add_child(ctx, radial);

    return map_entry;
} 

radial_menu_map_t *ui_radial_menu_modal(ui_context_t *ctx, char *title, f32 radius)
{
    ui_begin_modal(ctx,"modal1", gc.screen_width, gc.screen_height);
        radial_menu_map_t *map_entry = shgetp_null(g_radial_menu_map, title);
        if (!map_entry) 
        {
            radial_menu_state_t state = {
                .layout = {
                    .center_x = gc.screen_width / 2.0f,
                    .center_y = gc.screen_height / 2.0f,
                    .scale_x = 1.0f,
                    .scale_y = 1.0f,
                    .num_segments = 0,
                    .inner_radius = radius * 0.3f,
                    .outer_radius = radius,
                    .rotation = -M_PI / 2.0f, 
                    .gap_factor = 0.1f
                },
                .items = NULL,
                .hovered_index = -1,
                .selected_index = -1,
                .active = true
            };
            shput(g_radial_menu_map, title, state);
        }

        ui_block_t *radial = CHECK_PTR(ui_new_block(ctx, title, gc.screen_width / 2.0f - radius, gc.screen_height / 2.0f - radius, radius * 2, radius * 2, NO_LAYOUT));

        radial->is_leaf = true;
        radial->clickable = true;
        radial->toggleable = true;
        radial->draggable = true;
        
        radial->bg_color = HEX_TO_COLOR4(0xbd93f9);
        radial->hover_color = lite(HEX_TO_COLOR4(0xbd93f9));

        radial->custom_render = ui_render_radial_menu;
        radial->custom_update = ui_update_radial_menu;

        ui_add_child(ctx, radial);
    ui_end_modal(ctx);
    if (map_entry->value.selected_index >= 0 || gc.right_button_down) {
        ctx->modal_active = false;
        map_entry->value.active = false;
    }
    return map_entry;
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

i32 text_edit_get_char_at_x(ui_block_t *block, text_edit_state_t *state, i32 mouse_x)
{
    // we are now in the block space
    i32 local_x = mouse_x - block->abs_x;
    // acount for scroll
    local_x += state->scroll_offset;

    f32 cumulative_width = 5;

    for (i32 i = 0; i < state->buffer_len; i++) 
    {
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
    assert(state->cursor_pos >= count);

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

void ui_text_edit_shortcuts(text_edit_state_t *state)
{
    if (gc.key_just_pressed[GLFW_KEY_BACKSPACE]) 
    {
        if (gc.key_pressed[GLFW_KEY_LEFT_CONTROL] ||
            gc.key_pressed[GLFW_KEY_RIGHT_CONTROL]) 
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

    if (gc.key_just_pressed[GLFW_KEY_DELETE]) 
    {
        if (gc.key_pressed[GLFW_KEY_LEFT_CONTROL] ||
            gc.key_pressed[GLFW_KEY_RIGHT_CONTROL]) 
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

    if (gc.key_just_pressed[GLFW_KEY_LEFT]) 
    {
        if (gc.key_pressed[GLFW_KEY_LEFT_SHIFT] ||
            gc.key_pressed[GLFW_KEY_RIGHT_SHIFT]) 
        {
            if (state->selection_start < 0) {
                state->selection_start = state->cursor_pos;
            }
            state->cursor_pos--;
            text_edit_clamp_cursor(state);
            state->selection_end = state->cursor_pos;
        }
        else if (gc.key_pressed[GLFW_KEY_LEFT_CONTROL] ||
                gc.key_pressed[GLFW_KEY_RIGHT_CONTROL]) 
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

    if (gc.key_just_pressed[GLFW_KEY_RIGHT]) 
    {
        if (gc.key_pressed[GLFW_KEY_LEFT_SHIFT] ||
            gc.key_pressed[GLFW_KEY_RIGHT_SHIFT]) 
        {
            if (state->selection_start < 0) {
                state->selection_start = state->cursor_pos;
            }
            state->cursor_pos++;
            text_edit_clamp_cursor(state);
            state->selection_end = state->cursor_pos;
        }
        else if (gc.key_pressed[GLFW_KEY_LEFT_CONTROL] ||
                gc.key_pressed[GLFW_KEY_RIGHT_CONTROL]) 
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

    if (gc.key_just_pressed[GLFW_KEY_HOME]) {
        state->cursor_pos = 0;
        state->selection_start = -1;
        state->cursor_visible = true;
        state->cursor_blink_timer = 0.0;
    }
    
    if (gc.key_just_pressed[GLFW_KEY_END]) {
        state->cursor_pos = state->buffer_len;
        state->selection_start = -1;
        state->cursor_visible = true;
        state->cursor_blink_timer = 0.0;
    }

    if (gc.key_pressed[GLFW_KEY_LEFT_CONTROL] && gc.key_just_pressed[GLFW_KEY_A]) 
    {
        state->selection_start = 0;
        state->selection_end = state->buffer_len;
        state->cursor_pos = state->buffer_len;
    }
}

void ui_text_edit_update(ui_block_t *block)
{
    text_edit_map_t *map_entry = shgetp_null(gc.text_map, block->title);
    if (!map_entry) return;
    
    text_edit_state_t *state = &map_entry->value;

    // blinking cursor animation
    state->cursor_blink_timer += gc.dt;
    if (state->cursor_blink_timer > 0.5) {
        state->cursor_visible = !state->cursor_visible;
        state->cursor_blink_timer = 0.0;
    }

    if(block->clicked)
    {
        // gain focus
        gc.focused_text_edit = block->title;
        state->focused =  true;

        state->cursor_pos = text_edit_get_char_at_x(block, state, gc.mouse_x);
        state->selection_start = -1;
        state->cursor_visible = true;
        state->cursor_blink_timer = 0.0;
    }
    else if (gc.left_button_down && !block->hover && state->focused)
    {
        // clicked outside lose focus
        if (gc.focused_text_edit && strcmp(gc.focused_text_edit, block->title) == 0) {
            gc.focused_text_edit = NULL;
        }
        state->focused = false;
        state->selection_start = -1;
    }

    if (gc.left_button_down)
    {
        if(block->hover)
        {
            if(!state->dragging)
            {
                state->dragging = true;
                block->drag_start_x = gc.mouse_x;
            }
        }
    }
    else
    {
        state->dragging = false;
    }

    if(state->focused && state->dragging)
    {
        if(!state->mouse_selecting)
        {
            state->mouse_selecting = true;
            state->drag_start_x = text_edit_get_char_at_x(block, state, block->drag_start_x);
            state->selection_start = state->drag_start_x;
        }

        i32 current_pos = text_edit_get_char_at_x(block, state, gc.mouse_x);
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

    ui_text_edit_shortcuts(state);

    for (i32 i = 0; i < gc.input_char_count; i++) 
    {
        text_edit_insert_char(state, gc.input_char_queue[i]);
        state->cursor_visible = true;
        state->cursor_blink_timer = 0.0;
    }
    gc.input_char_count = 0;

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

void ui_render_text_edit(ui_block_t *block) 
{
    text_edit_map_t *map_entry = shgetp_null(gc.text_map, block->title);
    if (!map_entry) return;
    
    text_edit_state_t *state = &map_entry->value;
    
    color4_t bg_color = state->focused ? (color4_t){60, 60, 80, 255} : (color4_t){40, 42, 54, 255};
    
    ui_draw_rect_in_block(block, 0, 0, block->w, block->h, bg_color);
    
    color4_t border_color = state->focused ? (color4_t){100, 150, 255, 255} : (color4_t){100, 100, 100, 255};
    
    i32 border = 2;
    ui_draw_rect_in_block(block, 0, 0,
                          block->w, border, border_color);
    ui_draw_rect_in_block(block, 0, block->h - border,
                          block->w, border, border_color);
    ui_draw_rect_in_block(block, 0, 0, border,
                          block->h, border_color);
    ui_draw_rect_in_block(block, block->w - border, 0,
                         border, block->h, border_color);
    
    push_scissor(block->abs_x + border, block->abs_y + border, 
                 block->w - border * 2, block->h - border * 2);
    
        if (state->selection_start >= 0) 
        {
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
            
            i32 sel_x = (i32)sel_start_x - state->scroll_offset;
            i32 sel_y = 5;
            i32 sel_h = block->h - 10;
            
            color4_t selection_color = {100, 150, 200, 128};
            ui_draw_rect_in_block(block, sel_x, sel_y, 
                                  (i32)sel_width, sel_h, selection_color);
        }
        
        if (state->buffer_len > 0) 
        {
            rendered_text_tt text = {
                .font = block->text.font,
                .pos = {
                    .x = 5 - state->scroll_offset,
                    .y = (block->h - get_line_height(block->text.font)) / 2
                },
                .color = COLOR_WHITE,
                .string = state->buffer
            };
            block->text = text;
            ui_render_text_in_block (block);
        }
        
        if (state->focused && state->cursor_visible) 
        {
            f32 cursor_x = 5;
            for (i32 i = 0; i < state->cursor_pos; i++) {
                cursor_x += get_char_width(block->text.font, state->buffer[i]);
            }
            
            i32 cx = (i32)cursor_x - state->scroll_offset;
            i32 cy = 5;
            i32 ch = block->h - 10;
            
            color4_t cursor_color = {255, 255, 255, 255};
            ui_draw_rect_in_block(block, cx, cy, 2, ch, cursor_color);
        }
    
    pop_scissor();
}

text_edit_state_t *ui_text_edit(ui_context_t *ctx, char *title, i32 x, i32 y, i32 width, char *initial_text)
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

    ui_block_t *block = CHECK_PTR(ui_new_block(ctx, title, x, y, width, 40, NO_LAYOUT));

    block->is_leaf = true;
    block->clickable = true;
    block->toggleable = true;

    block->custom_render = ui_render_text_edit;
    block->custom_update = ui_text_edit_update;

    ui_add_child(ctx, block);

    return &shgetp_null(gc.text_map, title)->value;
}

/* -------------- Multiline text label stuff -------------- */
void ui_render_text_label (ui_block_t *block)
{
    text_edit_map_t *map_entry = shgetp_null(gc.text_map, block->title);
    if(!map_entry){
        return;
    }
    text_edit_state_t *state = &map_entry->value;

    if(block->bg_color.a > 0){
        ui_draw_rounded_rect_in_block(block, 0, 0, block->w, block->h, block->bg_color);
    }

    i32 padding = 5;
    push_scissor(block->abs_x + padding, block->abs_y + padding, 
                 block->w - padding * 2, block->h - padding * 2);
                 
        i32 line_height = get_line_height(block->text.font);
        
        i32 current_line = 0;
        i32 line_start = 0;
        
        PROFILE("Rendering Text")
        {
            for (i32 i = 0; i <= state->buffer_len; i++) 
            {
                bool is_newline = (i < state->buffer_len && state->buffer[i] == '\n');
                bool is_end = (i == state->buffer_len);
                
                // render line by line
                if (is_newline || is_end) 
                {
                    i32 line_len = i - line_start;
                    
                    if (line_len > 0) 
                    {
                        char line_buffer[TEXT_BUFFER_MAX];
                        strncpy(line_buffer, &state->buffer[line_start], line_len);
                        line_buffer[line_len] = '\0';
                        
                        rendered_text_tt text = 
                        {
                            .font = block->text.font,
                            .pos = {
                                .x = padding,
                                .y = padding + current_line * line_height
                            },
                            .color = HEX_TO_COLOR4(0x50fa7b),
                            .string = line_buffer
                        };
                        
                        block->text = text;
                        ui_render_text_in_block(block);
                    }
                    
                    current_line++;
                    line_start = i + 1;
                }
            }
        }
    pop_scissor();
}

void ui_text_label(ui_context_t *ctx, char *title, const char *text, i32 x, i32 y, i32 max_width, color4_t bg_color, bool wrap)
{
    text_edit_map_t *map_entry = shgetp_null(gc.text_map, title);

    if (!map_entry) 
    {
        text_edit_state_t state = {0};
        shput(gc.text_map, title, state);
        map_entry = shgetp_null(gc.text_map, title);
    }
    
    text_edit_state_t *state = &map_entry->value;

    // Wrap the text into buffer
    i32 line_count = 1;
    f32 max_line_width = 0;
    
    PROFILE("Wrapping text")
    {
        if(wrap)
        {
            wrap_text(gc.font, text, (float)(max_width - 10), 
                      state->buffer, TEXT_BUFFER_MAX, 
                      &line_count, &max_line_width);
        }
    }
    
    state->buffer_len = (i32)strlen(state->buffer);
    i32 line_height = get_line_height(gc.font);
    i32 total_height = line_count * line_height + 10;

    ui_block_t *block = CHECK_PTR(ui_new_block(ctx, title, x, y, 
                                  max_width, total_height, NO_LAYOUT));

    block->bg_color = bg_color;
    block->is_leaf = true;
    block->custom_render = ui_render_text_label;

    ui_add_child(ctx, block);
}

void ui_end_frame(ui_context_t *ctx)
{
    // glfwGetCursorPos(gc.window, &gc.mouse_x, &gc.mouse_y);
    ui_handle_mouse_move(ctx,0,0);
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

    clear_screen(&gc.draw_buffer, HEX_TO_COLOR4(0xFFFFFF));

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
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gc.read_fbo); // source
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);           // destination, make the default framebuffer active again ?
    
    // transfer pixel values from one region of a read framebuffer to another region of a draw framebuffer.
    glBlitFramebuffer(0, 0, width, height,  // src
                      0, height, width, 0,  // dst (flipped)    
                      GL_COLOR_BUFFER_BIT,    
                      GL_NEAREST);      
}

void present_frame(image_view_t* view) 
{
    PROFILE("blit_to_screen")
    {
        blit_to_screen(view->pixels, view->width, view->height);
    }

    PROFILE("swapping buffers")
    {
        glfwSwapBuffers((GLFWwindow*)gc.window);
    }
}

void render_frame_history_graph(void)
{
    const i32 graph_height = 60;  
    const i32 graph_width = gc.screen_width;
    const i32 margin = 5;
    
    color4_t bg_color = HEX_TO_COLOR4(0x282a36);
    bg_color.a = 180;
    
    draw_rect_solid_wh(&gc.draw_buffer, 0, 0, graph_width, graph_height, bg_color);
    
    const f64 target_time = 0.01667;//60fps
    
    f64 max_deviation = 0.0;
    for (int i = 0; i < FRAME_HISTORY_SIZE; i++) {
        f64 deviation = fabs(gc.frame_history[i] - target_time);
        if (deviation > max_deviation) {
            max_deviation = deviation;
        }
    }
    
    max_deviation = ClampBot(0.008, max_deviation);
    
    i32 center_y = graph_height / 2;
    i32 available_height = (graph_height - 2 * margin) / 2;
    
    i32 x_step = ClampBot(1, graph_width / FRAME_HISTORY_SIZE);
    
    color4_t line_color = HEX_TO_COLOR4(0x50fa7b); 
    line_color.a = 220; 
    
    for (int i = 1; i < FRAME_HISTORY_SIZE; i++) 
    {
        int prev_idx = (gc.frame_idx + i - 1) % FRAME_HISTORY_SIZE;
        int curr_idx = (gc.frame_idx + i) % FRAME_HISTORY_SIZE;
        
        f64 prev_time = gc.frame_history[prev_idx];
        f64 curr_time = gc.frame_history[curr_idx];
        
        f64 prev_offset = prev_time - target_time;
        f64 curr_offset = curr_time - target_time;
        
        i32 y1 = center_y - (i32)((prev_offset / max_deviation) * available_height);
        i32 y2 = center_y - (i32)((curr_offset / max_deviation) * available_height);
        
        i32 x1 = (i - 1) * x_step + margin;
        i32 x2 = i * x_step + margin;
        
        y1 = Clamp(margin, y1, graph_height - margin);
        y2 = Clamp(margin, y2, graph_height - margin);
        
        draw_line(&gc.draw_buffer, x1, y1, x2, y2, line_color);
    }
    
    color4_t ref_color = HEX_TO_COLOR4(0xffb86c);  
    ref_color.a = 150;
    
    for (i32 x = margin; x < graph_width - margin; x += 4) 
    {
        draw_line(&gc.draw_buffer, x, center_y, x + 2, center_y, ref_color);
    }
}

void render_mouse_stuff(void)
{
    draw_line(&gc.draw_buffer, gc.mouse_x-20, gc.mouse_y, gc.mouse_x+20, gc.mouse_y, gc.left_button_down?COLOR_RED:gc.right_button_down?COLOR_BLUE:COLOR_CYAN);
    draw_line(&gc.draw_buffer, gc.mouse_x, gc.mouse_y-20, gc.mouse_x, gc.mouse_y+20, gc.left_button_down?COLOR_RED:gc.right_button_down?COLOR_BLUE:COLOR_CYAN);
    // draw_circle_filled_aa(&gc.draw_buffer, gc.mouse_x, gc.mouse_y, 100, COLOR_CYAN);
    // draw_circle_aa(&gc.draw_buffer, gc.mouse_x, gc.mouse_y, 100, COLOR_CYAN);
    // draw_rounded_rectangle_filled_aa(&gc.draw_buffer, gc.mouse_x - 100, gc.mouse_y - 50, gc.mouse_x + 100, gc.mouse_y + 50, 20.0f, COLOR_CYAN);
}

void menu_callback_new(void *data) {
    printf("New selected!\n");
}

void menu_callback_open(void *data) {
    printf("Open selected!\n");
}

void menu_callback_save(void *data) {
    printf("Save selected!\n");
}

void menu_callback_exit(void *data) {
    printf("Exit selected!\n");
    gc.running = false;
}

void render_todo_screen(void)
{
    text_edit_state_t *text_state = NULL;

    ui_begin_panel(gc.ui_ctx, "main panel", gc.side_panel_x, 0, gc.screen_width-gc.side_panel_x, gc.screen_height, VERTICAL_LAYOUT);

        todo_list *curr_list = &main_list[gc.curr_list];

        for(i32 i = 0; i < arrlen(curr_list->todo_items); i++)
        {
            // TODO : fix! this will lead to horrible bugs as we are depending
            // that the pointers are stable when adding to the array may realloc
            if(ui_checkbox(gc.ui_ctx, curr_list->todo_items[i].todo))
            {
                curr_list->todo_items[i].completed = true;
            }
        }

        text_state = ui_text_edit(gc.ui_ctx, "TextEdit1", 0,0,200,"Add your task ..");

        if(ui_button(gc.ui_ctx, "Submit"))
        {
            todo_item new_item = {0};
            todo_item_add_content(&new_item, text_state->buffer);
            todo_list_add(curr_list, &new_item);
        }

        // ui_slider(gc.ui_ctx, 0.0f, 100.0f, "slider test");

    ui_end_panel(gc.ui_ctx);
    
    ui_begin_panel(gc.ui_ctx, "side_panel", 0, 0, gc.side_panel_x, gc.screen_height, VERTICAL_LAYOUT);

        for(i32 i = 0; i < arrlen(main_list); i++){
            ui_radio(gc.ui_ctx, main_list[i].name, &gc.curr_list, i);
        }

    ui_end_panel(gc.ui_ctx);

    ui_layout(gc.ui_ctx);

    ui_render(gc.ui_ctx);

    reset_input_for_frame();
}

void render_layout_test_screen(void)
{
    text_edit_state_t *text_state = NULL;

    PROFILE("Building UI Tree")
    {
        ui_begin_panel(gc.ui_ctx, "Root", gc.side_panel_x*gc.screen_width,
                       0, (1-gc.side_panel_x)*gc.screen_width, gc.screen_height, VERTICAL_LAYOUT);
            ui_begin_panel(gc.ui_ctx, "Test1", 0, 0, 0, 100, HORIZONTAL_LAYOUT);
                ui_begin_panel(gc.ui_ctx, "Test1-1", 0, 0, 100, 0, VERTICAL_LAYOUT);
                ui_end_panel(gc.ui_ctx);
                ui_begin_panel(gc.ui_ctx, "Test1-2", 0, 0, 50, 0, HORIZONTAL_LAYOUT);
                ui_end_panel(gc.ui_ctx);
                ui_button(gc.ui_ctx,"Buttton1");
                ui_button(gc.ui_ctx,"Buttton2");
            ui_end_panel(gc.ui_ctx);
                
            ui_begin_panel(gc.ui_ctx, "Test2", 0, 0, 0, 400, VERTICAL_LAYOUT);
                ui_begin_panel(gc.ui_ctx, "Test2-1", 0, 0, 0, 50, HORIZONTAL_LAYOUT);
                ui_end_panel(gc.ui_ctx);
                ui_begin_panel(gc.ui_ctx, "Test2-2", 0, 0, 0, 100, VERTICAL_LAYOUT);
                    ui_button(gc.ui_ctx,"test_but_1");
                    ui_button(gc.ui_ctx,"test_but_2");
                ui_end_panel(gc.ui_ctx);
                ui_button(gc.ui_ctx,"Buton1");
                ui_button(gc.ui_ctx,"Buton2");
                ui_button(gc.ui_ctx,"Buton11");
                ui_button(gc.ui_ctx,"Buton21");
                ui_button(gc.ui_ctx,"Buton12");
                ui_button(gc.ui_ctx,"Buton23");
                ui_button(gc.ui_ctx,"Buton14");
                ui_button(gc.ui_ctx,"Buton25");
            ui_end_panel(gc.ui_ctx);
            
            ui_begin_panel(gc.ui_ctx, "Test3", 0, 0, 0, 300, VERTICAL_LAYOUT);
                if(ui_tree_node_begin(gc.ui_ctx, "First Node"))
                {
                    ui_button(gc.ui_ctx,"Node does it work");
                    ui_button(gc.ui_ctx,"I truely want it to work");
                    ui_tree_node_end(gc.ui_ctx);
                } 
                if(ui_tree_node_begin(gc.ui_ctx, "Second Node"))
                {
                    ui_button(gc.ui_ctx,"done1");
                    ui_button(gc.ui_ctx,"done2");
                    if(ui_tree_node_begin(gc.ui_ctx, "Nested Node"))
                    {
                        ui_button(gc.ui_ctx,"Nested1");

                        ui_tree_node_end(gc.ui_ctx);
                    }
                    ui_tree_node_end(gc.ui_ctx);
                } 
                f32 val = ui_slider(gc.ui_ctx, 50.0f, 600.0f,"Slider");
                text_state = ui_text_edit(gc.ui_ctx, "TextEdit1", 0,0,200,"Add your task ..");
                ui_text_label(gc.ui_ctx, "MultiLine", 
                              "Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. ",
                              0, 0, val, HEX_TO_COLOR4(0x3d005c),true);

            ui_end_panel(gc.ui_ctx);

            ui_begin_panel(gc.ui_ctx, "Test4", 0, 0, 0, 300, NO_LAYOUT);
                radial_menu_map_t *menu_state = ui_radial_menu(gc.ui_ctx, "testradial", 150,150 , 150);
                    static int once = 0;
                    if (menu_state && !once) {
                        once = 1;
                        radial_menu_add_item(&menu_state->value, "New",  menu_callback_new, NULL);
                        radial_menu_add_item(&menu_state->value, "Open", menu_callback_open, NULL);
                        radial_menu_add_item(&menu_state->value, "Save", menu_callback_save, NULL);
                        radial_menu_add_item(&menu_state->value, "Exit", menu_callback_exit, NULL);
                    }
            ui_end_panel(gc.ui_ctx);

        ui_end_panel(gc.ui_ctx);
    }

    PROFILE("UI Layout")
    {
        ui_layout(gc.ui_ctx);
    }

    PROFILE("UI Rendering")
    {
        ui_render(gc.ui_ctx);
    }

    reset_input_for_frame(); 
}

void render_debug_screen(void)
{
    render_frame_history_graph();
    
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

    for(i32 i = gc.ui_ctx->block_count-1 ; i >= 0; i--)
    {
        if(gc.ui_ctx->blocks[i]->id == gc.ui_ctx->hot_block_id)
        {
            i32 screen_x = gc.ui_ctx->blocks[i]->abs_x;
            i32 screen_y = gc.ui_ctx->blocks[i]->abs_y;

            draw_rect_outline_wh(&gc.draw_buffer, screen_x, screen_y, 
                                 gc.ui_ctx->blocks[i]->w, gc.ui_ctx->blocks[i]->h, 
                                (gc.ui_ctx->blocks[i]->id == gc.ui_ctx->active_block_id) ? COLOR_GREEN:COLOR_RED);

            draw_rect_outline_wh(&gc.draw_buffer, screen_x, screen_y, 
                                  gc.ui_ctx->blocks[i]->scissor_region.w, gc.ui_ctx->blocks[i]->scissor_region.h,
                                 (gc.ui_ctx->blocks[i]->id == gc.ui_ctx->active_block_id) ? COLOR_LIME:COLOR_YELLOW);
            // draw_aaline(&gc.draw_buffer, screen_x, screen_y, gc.mouse_x, gc.mouse_y, COLOR_GREEN);
            // draw_aaline(&gc.draw_buffer, screen_x+gc.ui_ctx->blocks[i]->w, screen_y+gc.ui_ctx->blocks[i]->h, gc.mouse_x, gc.mouse_y, COLOR_GREEN);
            // draw_aaline(&gc.draw_buffer, screen_x+gc.ui_ctx->blocks[i]->w, screen_y, gc.mouse_x, gc.mouse_y, COLOR_GREEN);
            // draw_aaline(&gc.draw_buffer, screen_x, screen_y+gc.ui_ctx->blocks[i]->h, gc.mouse_x, gc.mouse_y, COLOR_GREEN);
        }
    }

    for(i32 i = gc.ui_ctx->block_count-1 ; i >= 0; i--)
    {
        if(gc.ui_ctx->blocks[i]->id == gc.ui_ctx->active_block_id)
        {
            i32 screen_x = gc.ui_ctx->blocks[i]->abs_x;
            i32 screen_y = gc.ui_ctx->blocks[i]->abs_y;
            draw_rect_outline_wh(&gc.draw_buffer, screen_x, screen_y,
                                 gc.ui_ctx->blocks[i]->w, gc.ui_ctx->blocks[i]->h, COLOR_LIME);
        }
    }

    render_mouse_stuff();
}

void render_floating_panel_test(void)
{
    text_edit_state_t *text_state = NULL;

    PROFILE("Building UI Tree")
    {
        ui_begin_panel_floating(gc.ui_ctx, "Root", 200, 300, 400, 200, VERTICAL_LAYOUT);

            if(ui_button(gc.ui_ctx, "test_floating"))
            {
                radial_menu_map_t *menu_state = ui_radial_menu_modal(gc.ui_ctx, "testing_modal", 200.0f);
                    static int only_once = 0;
                    if (menu_state && !only_once) {
                        only_once = 1;
                        radial_menu_add_item(&menu_state->value, "New",  menu_callback_new, NULL);
                        radial_menu_add_item(&menu_state->value, "Open", menu_callback_open, NULL);
                        radial_menu_add_item(&menu_state->value, "Save", menu_callback_save, NULL);
                        radial_menu_add_item(&menu_state->value, "Exit", menu_callback_exit, NULL);
                    }
            }
            ui_button(gc.ui_ctx, "test_floating2");
        ui_end_panel(gc.ui_ctx);
        
    }

    PROFILE("UI Layout")
    {
        ui_layout(gc.ui_ctx);
    }

    PROFILE("UI Rendering")
    {
        ui_render(gc.ui_ctx);
    }

    reset_input_for_frame(); 
}

void render_all(void)
{
    gc.draw_buffer.height = gc.screen_height;
    gc.draw_buffer.width  = gc.screen_width;

    u32 height  = gc.draw_buffer.height;
    u32 width   = gc.draw_buffer.width;

    gc.draw_buffer.pixels = ARENA_ALLOC(gc.frame_arena, height * width * sizeof(color4_t));
    
    PROFILE("Clearing the screen")
    {
        clear_screen(&gc.draw_buffer, HEX_TO_COLOR4(0x282a36));
        // clear_screen_radial_gradient(&gc.draw_buffer, HEX_TO_COLOR4(0x6272a4),HEX_TO_COLOR4(0x282a36));
    }
    
    ui_new_frame(gc.ui_ctx);

    // render_todo_screen();
    PROFILE("Layout Screen"){
        render_layout_test_screen();
        // render_floating_panel_test();
    }

    reset_input_for_frame(); 
 
    if(gc.debug)
    {
        PROFILE("Debug Screen"){
            render_debug_screen();
        }
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
    
    gc.frame_idx = INC_WRAP(gc.frame_idx, FRAME_HISTORY_SIZE);
    
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
    update_time();
    animation_update(gc.dt);
    animation_get((u64)&gc.side_panel_x, &gc.side_panel_x);
    ui_update(gc.ui_ctx);
}

void cleanup_all()
{
    arena_reset(gc.frame_arena);

    ui_end_frame(gc.ui_ctx);

    prof_sort_results();
    prof_record_results();
    // prof_print_results();
    prof_reset();
}

void init_framebuffer(i32 max_width, i32 max_height)
{
    glGenTextures(1, &gc.texture);
    glBindTexture(GL_TEXTURE_2D, gc.texture);

    // Generate a full screen texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
                 max_width, max_height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenFramebuffers(1, &gc.read_fbo);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gc.read_fbo);

    // attach the texture to the framebuffer
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, gc.texture, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) 
    {
        fprintf(stderr, "Framebuffer not complete!\n");
        exit(1);
    }
}

void *create_window(u32 width, u32 height, char *title)
{
    if (!glfwInit()) {
        fprintf(stderr, "Error : Failed to initialize GLFW\n");
        return NULL;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    // glfwWindowHint(GLFW_SAMPLES, 8); // Enable multisampling for smoother edges
    
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
    glfwSetDropCallback(window, drop_callback);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error : GLEW initialization failed: %s\n", glewGetErrorString(err));
        return NULL;
    }

    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    return window;
}

void init_time(void)
{
    get_time(&gc.last_frame_start);

    for (i32 i = 0; i < FRAME_HISTORY_SIZE; i++) {
        gc.frame_history[i] = 1.0 / 60.0; 
    }
    gc.frame_idx = 0;
    gc.current_fps = 60.0;
}

bool init_all(void)
{
    gc.screen_width  = 1100;
    gc.screen_height = 800;

    gc.window = CHECK_PTR(create_window(gc.screen_width, gc.screen_height, "UI"));

    if(!gc.window){
        return false;
    }

    gc.frame_arena = arena_reserve(MB(10));

    init_framebuffer(1920, 1080);

    prof_init();

    gc.font = load_font_from_file("..\\Font\\Lato.ttf", 32.0f);
    gc.icon_font = load_font_from_file("..\\Font\\Icons.ttf", 32.0f);

    if(!(gc.font && gc.icon_font)){
        return false;
    }
    
    gc.ui_ctx = CHECK_PTR(ui_new_ctx());

    if(!(gc.ui_ctx)){
        return false;
    }
    
    gc.running     = true;
    gc.resize      = true;
    gc.rescale     = true;
    gc.render      = true;
    gc.dock        = false;
    gc.profile     = false;
    gc.changed     = true;
    
    init_time();
    
    gc.hand_cursor    = CHECK_PTR(glfwCreateStandardCursor(GLFW_HAND_CURSOR));
    gc.regular_cursor = CHECK_PTR(glfwCreateStandardCursor(GLFW_ARROW_CURSOR));
    
    set_dark_mode(gc.window);
    set_window_icon("..\\Resources\\icon.png");

    gc.side_panel_x = 0.3;

    todo_list_new("Default list");
    todo_list_new("Hobbies");
    todo_list_new("Work stuff");
    todo_list_new("Programming");

    return true;
}

int main(void)
{   
    if(!init_all())
    {
        fprintf(stderr, "Error : Failed to initialize application");
        exit(EXIT_FAILURE);
    }

    while(!glfwWindowShouldClose((GLFWwindow*)gc.window))
    {
        PROFILE("Events")
        {
            poll_events();
        }

        PROFILE("Updating All")
        {
            update_all();
        }
        
        PROFILE("Rendering all")
        {
            render_all();
        }
        
        cleanup_all();

        PROFILE("Presenting The frame")
        {
            present_frame(&gc.draw_buffer);
        }
    }
    return 0;
}
