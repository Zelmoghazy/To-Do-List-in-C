/* borderless - make any top-level Win32 window borderless while keeping
 * snapping, shadows, animations and the rest of the standard window behaviour.
 *
 * Based on borderless-window, written in 2016 by James Ross-Gowan.
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * Usage:
 *
 *     struct borderless *bw = borderless_attach(hwnd);
 *     bw->caption_height = 32;   // optional: drag area at the top
 *     bw->hit_test = my_hit_test; // optional: custom buttons etc.
 *
 * borderless_attach() subclasses the window, so it works with windows created
 * by your own code as well as by libraries such as GLFW or SDL. The window
 * should be a normal overlapped window (WS_OVERLAPPEDWINDOW), not WS_POPUP.
 * The state is freed automatically when the window is destroyed.
 */

#ifndef BORDERLESS_H
#define BORDERLESS_H

#include <windows.h>
#include <stdbool.h>

/* Custom hit-test hook. x and y are in client coordinates. Return a HT* code
   (eg. HTCLIENT for a button, HTMAXBUTTON for a maximize button so Windows 11
   shows Snap Layouts) or HTNOWHERE to fall through to the default logic. */
typedef LRESULT (*borderless_hit_test_fn)(void *user, int x, int y);

/* Message hook, called before a message reaches the original window
   procedure. Return true and set *result to swallow the message. */
typedef bool (*borderless_message_hook_fn)(void *user, UINT msg, WPARAM wparam,
                                           LPARAM lparam, LRESULT *result);

struct borderless {
	HWND hwnd;
	WNDPROC next_proc;       /* window procedure that was replaced */

	int width;               /* client size in pixels */
	int height;
	UINT dpi;

	RECT rgn;
	bool theme_enabled;
	bool composition_enabled;

	/* Settings, may be changed at any time */
	int caption_height;      /* drag area height in 96-DPI pixels, 0 = none */
	borderless_hit_test_fn hit_test;
	void *hit_test_user;
	borderless_message_hook_fn message_hook;
	void *message_hook_user;
};

/* Makes hwnd borderless. Returns NULL on failure or if already attached. */
struct borderless *borderless_attach(HWND hwnd);

/* Restores the original window procedure and frame and frees bw. Fails (and
   does nothing) if the window was subclassed again after borderless_attach. */
bool borderless_detach(struct borderless *bw);

/* Returns the state attached to hwnd, or NULL */
struct borderless *borderless_from_hwnd(HWND hwnd);

/* Scales a 96-DPI pixel value to the window's current DPI */
int borderless_scale(const struct borderless *bw, int px);

/* The hit-test logic used for WM_NCHITTEST. x and y are screen coordinates. */
LRESULT borderless_hit_test(struct borderless *bw, int x, int y);

#endif
