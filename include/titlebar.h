/* titlebar - a custom title bar for borderless windows
 *
 * Caption buttons: layout, hit testing, hover and pressed states, and the
 * minimize, maximize/restore and close actions. The maximize button reports
 * HTMAXBUTTON, so Windows 11 Snap Layouts work. caption_buttons_init() hooks
 * itself into the borderless window, so no changes to the window procedure
 * are needed.
 *
 * Title bar: draws the background and the button glyphs into an
 * image_view_t using the base_graphics primitives.
 *
 * Usage:
 *
 *     titlebar_init(&tb, bw, buttons, count, &style);  // after attaching
 *     ...
 *     titlebar_draw(&draw_buffer, &tb);  // each frame, after the content
 *
 * All sizes are in 96-DPI pixels and scaled to the window's DPI.
 */

#ifndef TITLEBAR_H
#define TITLEBAR_H

#include "base_graphics.h"
#include "font.h"
#include "borderless.h"

/* --- Caption buttons ----------------------------------------------------- */

enum caption_kind {
	CAPTION_CUSTOM,          /* calls on_click */
	CAPTION_MINIMIZE,
	CAPTION_MAXIMIZE,        /* maximize / restore */
	CAPTION_CLOSE,
};

enum caption_anchor {
	CAPTION_LEFT,            /* laid out left to right from the left edge */
	CAPTION_RIGHT,           /* laid out so the last one touches the right edge */
};

enum caption_state {
	CAPTION_NORMAL,
	CAPTION_HOVER,
	CAPTION_PRESSED,
};

struct caption_button {
	int id;                  /* passed to on_click for CAPTION_CUSTOM */
	enum caption_kind kind;
	enum caption_anchor anchor;
	int width;
	const char *label;       /* optional, drawn centred in the button (UTF-8) */
};

struct caption_buttons {
	struct borderless *bw;
	const struct caption_button *buttons;
	int count;
	int height;

	void (*on_click)(void *user, int id);
	void *user;

	/* Internal state */
	int hovered;             /* index into buttons, or -1 */
	int pressed;
	bool tracking_client;
	bool tracking_nonclient;
};

/* Sets up cb and installs its hooks and caption height into bw. The buttons
   array must outlive cb, and cb must not move after this call. */
void caption_buttons_init(struct caption_buttons *cb, struct borderless *bw,
                          const struct caption_button *buttons, int count,
                          int height);

/* Height in pixels at the current DPI */
int caption_buttons_height(const struct caption_buttons *cb);
/* Button rectangle in client coordinates */
RECT caption_buttons_rect(const struct caption_buttons *cb, int index);
/* Returns the index of the button at x, y (client coordinates), or -1 */
int caption_buttons_at(const struct caption_buttons *cb, int x, int y);
enum caption_state caption_buttons_state(const struct caption_buttons *cb,
                                         int index);

/* --- Title bar ----------------------------------------------------------- */

struct titlebar_colors {
	color4_t normal;
	color4_t hover;
	color4_t pressed;
};

struct titlebar_style {
	int height;              /* in 96-DPI pixels */
	color4_t background;
	color4_t glyph;          /* also the label text colour */
	font_tt *font;           /* for button labels, NULL = no labels */
	struct titlebar_colors button;
	struct titlebar_colors close;
};

struct titlebar {
	struct caption_buttons caption;
	struct titlebar_style style;
};

/* The buttons array must outlive tb, and tb must not move after this call */
void titlebar_init(struct titlebar *tb, struct borderless *bw,
                   const struct caption_button *buttons, int count,
                   const struct titlebar_style *style);

/* Draws the title bar over the whole width of img, which must be the size of
   the window's client area */
void titlebar_draw(image_view_t *img, const struct titlebar *tb);

#endif
