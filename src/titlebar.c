#include "titlebar.h"

#include <windowsx.h>

/* --- Layout -------------------------------------------------------------- */

static int scale(const struct caption_buttons *cb, int px)
{
	return borderless_scale(cb->bw, px);
}

int caption_buttons_height(const struct caption_buttons *cb)
{
	return scale(cb, cb->height);
}

RECT caption_buttons_rect(const struct caption_buttons *cb, int index)
{
	const struct caption_button *button = &cb->buttons[index];
	int left = 0;

	if (button->anchor == CAPTION_LEFT) 
    {
		for (int i = 0; i < index; i++){
			if (cb->buttons[i].anchor == CAPTION_LEFT){
				left += scale(cb, cb->buttons[i].width);
            }
        }
	} 
    else 
    {
		/* Right-anchored buttons are packed against the right edge */
		left = cb->bw->width;
		for (int i = index; i < cb->count; i++){
			if (cb->buttons[i].anchor == CAPTION_RIGHT){
				left -= scale(cb, cb->buttons[i].width);
            }
        }
	}

	return (RECT) {
		left, 0, left + scale(cb, button->width), caption_buttons_height(cb)
	};
}

int caption_buttons_at(const struct caption_buttons *cb, int x, int y)
{
	POINT pt = { x, y };
	for (int i = 0; i < cb->count; i++) {
		RECT r = caption_buttons_rect(cb, i);
		if (PtInRect(&r, pt)){
			return i;
        }
	}
	return -1;
}

enum caption_state caption_buttons_state(const struct caption_buttons *cb,
                                         int index)
{
	if (index != cb->hovered){
		return CAPTION_NORMAL;
    }
	return index == cb->pressed ? CAPTION_PRESSED : CAPTION_HOVER;
}

static int min_width(const struct caption_buttons *cb)
{
	int width = 0;
	for (int i = 0; i < cb->count; i++){
		width += scale(cb, cb->buttons[i].width);
    }
	return width;
}

/* --- State --------------------------------------------------------------- */

static void invalidate_button(struct caption_buttons *cb, int index)
{
	if (index < 0){
		return;
    }
	RECT r = caption_buttons_rect(cb, index);
	InvalidateRect(cb->bw->hwnd, &r, FALSE);
}

static void set_hovered(struct caption_buttons *cb, int index)
{
	if (index == cb->hovered){
		return;
    }
	invalidate_button(cb, cb->hovered);
	cb->hovered = index;
	invalidate_button(cb, index);
}

static void set_pressed(struct caption_buttons *cb, int index)
{
	if (index == cb->pressed){
		return;
    }
	invalidate_button(cb, cb->pressed);
	cb->pressed = index;
	invalidate_button(cb, index);
}

/* Recomputes the hovered button from the real cursor position. Used when the
   mouse leaves the client or non-client area, because moving between the two
   (eg. onto the HTMAXBUTTON) also generates a leave message. */
static void refresh_hovered(struct caption_buttons *cb)
{
	POINT pt;
	if (!GetCursorPos(&pt) || WindowFromPoint(pt) != cb->bw->hwnd) {
		set_hovered(cb, -1);
		return;
	}
	ScreenToClient(cb->bw->hwnd, &pt);
	set_hovered(cb, caption_buttons_at(cb, pt.x, pt.y));
}

static bool is_max_button(const struct caption_buttons *cb, int index)
{
	return index >= 0 && cb->buttons[index].kind == CAPTION_MAXIMIZE;
}

static void track_mouse(struct caption_buttons *cb, bool nonclient)
{
	bool *tracking = nonclient ? &cb->tracking_nonclient : &cb->tracking_client;
	if (*tracking){
		return;
    }

	TRACKMOUSEEVENT tme = {
		.cbSize = sizeof tme,
		.dwFlags = TME_LEAVE | (nonclient ? TME_NONCLIENT : 0),
		.hwndTrack = cb->bw->hwnd,
	};
	*tracking = TrackMouseEvent(&tme) != 0;
}

static void activate(struct caption_buttons *cb, int index)
{
	HWND hwnd = cb->bw->hwnd;

	switch (cb->buttons[index].kind) 
    {
        case CAPTION_MINIMIZE:
            ShowWindow(hwnd, SW_MINIMIZE);
            break;
        case CAPTION_MAXIMIZE:
            ShowWindow(hwnd, IsMaximized(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
            break;
        case CAPTION_CLOSE:
            /* WM_CLOSE rather than DestroyWindow, so the app can intercept it */
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            break;
        case CAPTION_CUSTOM:
            if (cb->on_click)
                cb->on_click(cb->user, cb->buttons[index].id);
            break;
	}
}

/* --- Hooks --------------------------------------------------------------- */

static LRESULT hit_test(void *user, int x, int y)
{
	struct caption_buttons *cb = user;
	int index = caption_buttons_at(cb, x, y);
	if (index < 0)
		return HTNOWHERE;
	/* The maximize button must be HTMAXBUTTON for Snap Layouts. The other
	   buttons are HTCLIENT so they receive ordinary mouse messages. */
	return is_max_button(cb, index) ? HTMAXBUTTON : HTCLIENT;
}

static bool message_hook(void *user, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT *result)
{
	struct caption_buttons *cb = user;
	*result = 0;

	switch (msg) 
    {
        case WM_SIZE:
        {
            /* Right-anchored buttons move when the width changes and the maximize
            glyph changes when the window is maximized or restored */
            InvalidateRect(cb->bw->hwnd, &(RECT) {
                0, 0, cb->bw->width, caption_buttons_height(cb)
            }, FALSE);
            return false;
        }
        case WM_GETMINMAXINFO: 
        {
            /* Don't let the window get narrow enough for the buttons to overlap */
            MINMAXINFO *mmi = (MINMAXINFO *)lparam;
            int width = min_width(cb);
            if (mmi->ptMinTrackSize.x < width)
                mmi->ptMinTrackSize.x = width;
            return false;
        }
        /* Client-area buttons: act on release, like normal buttons */
        case WM_MOUSEMOVE:
        {
            set_hovered(cb, caption_buttons_at(cb, GET_X_LPARAM(lparam),
            GET_Y_LPARAM(lparam)));
            track_mouse(cb, false);
            return false;
        }
        case WM_MOUSELEAVE:
        {
            cb->tracking_client = false;
            refresh_hovered(cb);
            return false;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: 
        {
            int index = caption_buttons_at(cb, GET_X_LPARAM(lparam),
                                        GET_Y_LPARAM(lparam));
            if (index < 0)
                return false;
            set_pressed(cb, index);
            SetCapture(cb->bw->hwnd);
            return true;
        }
        case WM_LBUTTONUP: 
        {
            int index = cb->pressed;
            if (index < 0 || is_max_button(cb, index))
                return false;
            set_pressed(cb, -1);
            ReleaseCapture();
            if (caption_buttons_at(cb, GET_X_LPARAM(lparam),
                                GET_Y_LPARAM(lparam)) == index)
            {
                activate(cb, index);
            }
            return true;
        }
        case WM_CAPTURECHANGED:
        {
            if (!is_max_button(cb, cb->pressed)){
                set_pressed(cb, -1);
            }
            return false;
        }

        /* The maximize button is HTMAXBUTTON, so it gets non-client messages */
        case WM_NCMOUSEMOVE: 
        {
            POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            ScreenToClient(cb->bw->hwnd, &pt);
            set_hovered(cb, caption_buttons_at(cb, pt.x, pt.y));
            track_mouse(cb, true);
            /* Don't let DefWindowProc try to hot-track a system button */
            return wparam == HTMAXBUTTON;
        }
        case WM_NCMOUSELEAVE:
        {
            cb->tracking_nonclient = false;
            if (is_max_button(cb, cb->pressed)){
                set_pressed(cb, -1);
            }
            refresh_hovered(cb);
            return false;
        }
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
            if (wparam != HTMAXBUTTON){
                return false;
            }
            /* Swallow it so the default caption button tracking doesn't run */
            for (int i = 0; i < cb->count; i++){
                if (is_max_button(cb, i)){
                    set_pressed(cb, i);
                }
            }
            return true;
        case WM_NCLBUTTONUP:
            if (wparam != HTMAXBUTTON){
                return false;
            }
            if (is_max_button(cb, cb->pressed)) {
                int index = cb->pressed;
                set_pressed(cb, -1);
                activate(cb, index);
            }
            return true;
	}
	return false;
}

/* --- Public API ---------------------------------------------------------- */

void caption_buttons_init(struct caption_buttons *cb, struct borderless *bw,
                          const struct caption_button *buttons, int count,
                          int height)
{
	*cb = (struct caption_buttons) {
		.bw = bw,
		.buttons = buttons,
		.count = count,
		.height = height,
		.hovered = -1,
		.pressed = -1,
	};

	bw->caption_height = height;
	bw->hit_test = hit_test;
	bw->hit_test_user = cb;
	bw->message_hook = message_hook;
	bw->message_hook_user = cb;
}

/* --- Title bar ----------------------------------------------------------- */

void titlebar_init(struct titlebar *tb, struct borderless *bw,
                   const struct caption_button *buttons, int count,
                   const struct titlebar_style *style)
{
	*tb = (struct titlebar) { .style = *style };
	caption_buttons_init(&tb->caption, bw, buttons, count, style->height);
}

static void draw_glyph(image_view_t *img, const struct borderless *bw, RECT r,
                       enum caption_kind kind, color4_t color)
{
	i32 size = borderless_scale(bw, 10);
	i32 t = MAX(1, borderless_scale(bw, 1));
	i32 x = (r.left + r.right - size) / 2;
	i32 y = (r.top + r.bottom - size) / 2;

	switch (kind) {
	case CAPTION_MINIMIZE:
		draw_rect_solid_wh(img, x, y + size / 2, size, t, color);
		break;
	case CAPTION_MAXIMIZE:
		if (IsMaximized(bw->hwnd)) {
			/* Restore glyph: a front square and the visible edges of a
			   second square behind it, offset up and to the right */
			i32 o = borderless_scale(bw, 2);
			for (i32 i = 0; i < t; i++)
				draw_rect_outline_wh(img, x + i, y + o + i,
				                     size - o - 2 * i, size - o - 2 * i, color);
			draw_rect_solid_wh(img, x + o, y, size - o, t, color);        /* top */
			draw_rect_solid_wh(img, x + size - t, y, t, size - o, color); /* right */
		} else {
			for (i32 i = 0; i < t; i++)
				draw_rect_outline_wh(img, x + i, y + i,
				                     size - 2 * i, size - 2 * i, color);
		}
		break;
	case CAPTION_CLOSE:
		for (i32 i = 0; i < t; i++) {
			draw_aaline(img, x + i, y, x + size - 1 + i, y + size - 1, color);
			draw_aaline(img, x + size - 1 + i, y, x + i, y + size - 1, color);
		}
		break;
	case CAPTION_CUSTOM:
		break;
	}
}

/* Draws text centred in r */
static void draw_label(image_view_t *img, font_tt *font, RECT r,
                       const char *label, color4_t color)
{
	float w = get_string_width(font, label);
	float h = (float)(font->ascent - font->descent);
	float x = floorf((r.left + r.right - w) / 2);
	float y = floorf((r.top + r.bottom - h) / 2);
	render_text_simple(img, font, label, x, y, color);
}

static color4_t state_color(const struct titlebar_colors *colors,
                            enum caption_state state)
{
	switch (state) {
	case CAPTION_HOVER:   return colors->hover;
	case CAPTION_PRESSED: return colors->pressed;
	default:              return colors->normal;
	}
}

void titlebar_draw(image_view_t *img, const struct titlebar *tb)
{
	const struct caption_buttons *cb = &tb->caption;

	draw_rect_solid_wh(img, 0, 0, (i32)img->width, caption_buttons_height(cb),
	                   tb->style.background);

	for (int i = 0; i < cb->count; i++) {
		const struct caption_button *button = &cb->buttons[i];
		const struct titlebar_colors *colors =
			button->kind == CAPTION_CLOSE ? &tb->style.close
			                              : &tb->style.button;
		RECT r = caption_buttons_rect(cb, i);

		draw_rect_solid(img, r.left, r.top, r.right - 1, r.bottom - 1,
		                state_color(colors, caption_buttons_state(cb, i)));
		draw_glyph(img, cb->bw, r, button->kind, tb->style.glyph);
		if (button->label && tb->style.font)
			draw_label(img, tb->style.font, r, button->label, tb->style.glyph);
	}
}
