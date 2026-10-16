/* borderless - see borderless.h
 *
 * Based on borderless-window, written in 2016 by James Ross-Gowan.
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "borderless.h"

#include <windowsx.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <versionhelpers.h>
#include <stdlib.h>

#ifndef WM_NCUAHDRAWCAPTION
#define WM_NCUAHDRAWCAPTION (0x00AE)
#endif
#ifndef WM_NCUAHDRAWFRAME
#define WM_NCUAHDRAWFRAME (0x00AF)
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED (0x02E0)
#endif
#ifndef ABM_GETAUTOHIDEBAREX
#define ABM_GETAUTOHIDEBAREX (0x0000000b)
#endif

static const wchar_t PROP_NAME[] = L"borderless";

/* --- DPI helpers ---------------------------------------------------------
   The per-DPI functions only exist on Windows 10 1607+, so load them at
   runtime to keep the binary working on older versions. */

static UINT get_window_dpi(HWND hwnd)
{
	typedef UINT (WINAPI *get_dpi_for_window_fn)(HWND);
	static get_dpi_for_window_fn get_dpi_for_window;
	static bool loaded;

	if (!loaded) {
		get_dpi_for_window = (get_dpi_for_window_fn)(void *)GetProcAddress(
			GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
		loaded = true;
	}
	if (get_dpi_for_window)
		return get_dpi_for_window(hwnd);

	HDC dc = GetDC(NULL);
	UINT dpi = GetDeviceCaps(dc, LOGPIXELSX);
	ReleaseDC(NULL, dc);
	return dpi;
}

static int get_metric(int index, UINT dpi)
{
	typedef int (WINAPI *get_system_metrics_for_dpi_fn)(int, UINT);
	static get_system_metrics_for_dpi_fn get_system_metrics_for_dpi;
	static bool loaded;

	if (!loaded) {
		get_system_metrics_for_dpi = (get_system_metrics_for_dpi_fn)(void *)
			GetProcAddress(GetModuleHandleW(L"user32.dll"),
			               "GetSystemMetricsForDpi");
		loaded = true;
	}
	if (get_system_metrics_for_dpi)
		return get_system_metrics_for_dpi(index, dpi);
	return GetSystemMetrics(index);
}

int borderless_scale(const struct borderless *bw, int px)
{
	return MulDiv(px, bw->dpi, USER_DEFAULT_SCREEN_DPI);
}

/* --- Message handlers ---------------------------------------------------- */

static LRESULT call_next(struct borderless *bw, UINT msg, WPARAM wparam,
                         LPARAM lparam)
{
	return CallWindowProcW(bw->next_proc, bw->hwnd, msg, wparam, lparam);
}

static void update_region(struct borderless *bw)
{
	RECT old_rgn = bw->rgn;

	if (bw->composition_enabled) {
		/* Never use a region when composition is enabled. A window with a
		   region loses its DWM frame: it has no shadow, and Windows paints a
		   classic caption instead, which flashes when a maximized window is
		   dragged off the top of the screen (it's restored before the region
		   is removed here). DWM already clips maximized windows to their
		   monitor, so the overhanging borders don't need cutting off. */
		bw->rgn = (RECT) { 0 };
	} else if (IsMaximized(bw->hwnd)) {
		WINDOWINFO wi = { .cbSize = sizeof wi };
		GetWindowInfo(bw->hwnd, &wi);

		/* Without composition, a region is needed to cut off the non-client
		   borders of maximized windows that hang over the edge of the screen */
		bw->rgn = (RECT) {
			.left = wi.rcClient.left - wi.rcWindow.left,
			.top = wi.rcClient.top - wi.rcWindow.top,
			.right = wi.rcClient.right - wi.rcWindow.left,
			.bottom = wi.rcClient.bottom - wi.rcWindow.top,
		};
	} else {
		/* For ordinary themed windows when composition is disabled, a region
		   is needed to remove the rounded top corners. Make it as large as
		   possible to avoid having to change it when the window is resized. */
		bw->rgn = (RECT) { 0, 0, 32767, 32767 };
	}

	/* Avoid unnecessarily updating the region to avoid unnecessary redraws */
	if (EqualRect(&bw->rgn, &old_rgn))
		return;
	/* Treat empty regions as NULL regions. SetWindowRgn takes ownership of
	   the region, so it must not be deleted here. */
	if (IsRectEmpty(&bw->rgn))
		SetWindowRgn(bw->hwnd, NULL, TRUE);
	else
		SetWindowRgn(bw->hwnd, CreateRectRgnIndirect(&bw->rgn), TRUE);
}

static void handle_compositionchanged(struct borderless *bw)
{
	BOOL enabled = FALSE;
	DwmIsCompositionEnabled(&enabled);
	bw->composition_enabled = enabled;

	if (enabled) {
		/* The window needs a frame to show a shadow, so give it the smallest
		   amount of frame possible. Extend it at the bottom, not the top: a
		   frame extended into the top of the client area makes Windows treat
		   the window as one that draws its caption in the DWM frame, and then
		   Windows 11 never shows the Snap Layouts flyout on the maximize
		   button of a restored window. (MARGINS is left, right, top, bottom.) */
		DwmExtendFrameIntoClientArea(bw->hwnd, &(MARGINS) { 0, 0, 0, 1 });
		DwmSetWindowAttribute(bw->hwnd, DWMWA_NCRENDERING_POLICY,
		                      &(DWORD) { DWMNCRP_ENABLED }, sizeof(DWORD));
	}

	update_region(bw);
}

static void handle_themechanged(struct borderless *bw)
{
	bw->theme_enabled = IsThemeActive();
}

static bool has_autohide_appbar(UINT edge, RECT mon)
{
	if (IsWindows8Point1OrGreater()) {
		return SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &(APPBARDATA) {
			.cbSize = sizeof(APPBARDATA),
			.uEdge = edge,
			.rc = mon,
		}) != 0;
	}

	/* Before Windows 8.1, it was not possible to specify a monitor when
	   checking for hidden appbars, so check only on the primary monitor */
	if (mon.left != 0 || mon.top != 0)
		return false;
	return SHAppBarMessage(ABM_GETAUTOHIDEBAR, &(APPBARDATA) {
		.cbSize = sizeof(APPBARDATA),
		.uEdge = edge,
	}) != 0;
}

static void handle_nccalcsize(struct borderless *bw, WPARAM wparam,
                              LPARAM lparam)
{
	/* lparam is either a RECT or an NCCALCSIZE_PARAMS, which starts with the
	   proposed window RECT, so both cases can be handled the same way */
	RECT *rect = (RECT *)lparam;

	/* The default handler must be called in both the maximized and
	   non-maximized cases, otherwise tile/cascade windows won't work */
	RECT nonclient = *rect;
	call_next(bw, WM_NCCALCSIZE, wparam, lparam);
	RECT client = *rect;

	if (!IsMaximized(bw->hwnd)) {
		/* For the non-maximized case, set the output RECT to what it was
		   before WM_NCCALCSIZE modified it. This will make the client size the
		   same as the non-client size. */
		*rect = nonclient;
		return;
	}

	/* Maximized windows always have a non-client border that hangs over the
	   edge of the screen, so the size proposed by WM_NCCALCSIZE is fine. Just
	   adjust the top border to remove the window title. The top border is as
	   thick as the bottom one. Don't use WINDOWINFO.cyWindowBorders for this:
	   with per-monitor DPI it can be off by a pixel from the frame that
	   DefWindowProc actually used, which leaves a see-through row at the top
	   of the screen (the window region clips it). */
	*rect = (RECT) {
		.left = client.left,
		.top = nonclient.top + (nonclient.bottom - client.bottom),
		.right = client.right,
		.bottom = client.bottom,
	};

	HMONITOR mon = MonitorFromWindow(bw->hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi = { .cbSize = sizeof mi };
	GetMonitorInfoW(mon, &mi);

	/* If the client rectangle is the same as the monitor's rectangle, the
	   shell assumes that the window has gone fullscreen, so it removes the
	   topmost attribute from any auto-hide appbars, making them inaccessible.
	   To avoid this, reduce the size of the client area by one pixel on a
	   certain edge. The edge is chosen based on which side of the monitor is
	   likely to contain an auto-hide appbar, so the missing client area is
	   covered by it. */
	if (EqualRect(rect, &mi.rcMonitor)) {
		if (has_autohide_appbar(ABE_BOTTOM, mi.rcMonitor))
			rect->bottom--;
		else if (has_autohide_appbar(ABE_LEFT, mi.rcMonitor))
			rect->left++;
		else if (has_autohide_appbar(ABE_TOP, mi.rcMonitor))
			rect->top++;
		else if (has_autohide_appbar(ABE_RIGHT, mi.rcMonitor))
			rect->right--;
	}
}

LRESULT borderless_hit_test(struct borderless *bw, int x, int y)
{
	POINT mouse = { x, y };
	ScreenToClient(bw->hwnd, &mouse);

	/* Resize borders take priority, except when maximized */
	if (!IsMaximized(bw->hwnd)) {
		/* The horizontal frame should be the same size as the vertical frame,
		   since the NONCLIENTMETRICS structure does not distinguish between
		   them */
		int frame = get_metric(SM_CXFRAME, bw->dpi) +
		            get_metric(SM_CXPADDEDBORDER, bw->dpi);
		/* The diagonal size handles are wider than the frame */
		int diagonal = frame * 2 + get_metric(SM_CXBORDER, bw->dpi);

		if (mouse.y < frame) {
			if (mouse.x < diagonal)
				return HTTOPLEFT;
			if (mouse.x >= bw->width - diagonal)
				return HTTOPRIGHT;
			return HTTOP;
		}
		if (mouse.y >= bw->height - frame) {
			if (mouse.x < diagonal)
				return HTBOTTOMLEFT;
			if (mouse.x >= bw->width - diagonal)
				return HTBOTTOMRIGHT;
			return HTBOTTOM;
		}
		if (mouse.x < frame)
			return HTLEFT;
		if (mouse.x >= bw->width - frame)
			return HTRIGHT;
	}

	if (bw->hit_test) {
		LRESULT ht = bw->hit_test(bw->hit_test_user, mouse.x, mouse.y);
		if (ht != HTNOWHERE)
			return ht;
	}

	if (mouse.y < borderless_scale(bw, bw->caption_height))
		return HTCAPTION;
	return HTCLIENT;
}

static void handle_windowposchanged(struct borderless *bw,
                                    const WINDOWPOS *pos)
{
	RECT client;
	GetClientRect(bw->hwnd, &client);
	bool client_changed = client.right != bw->width ||
	                      client.bottom != bw->height;
	bw->width = client.right;
	bw->height = client.bottom;

	if (client_changed || (pos->flags & SWP_FRAMECHANGED))
		update_region(bw);
}

static LRESULT handle_message_invisible(struct borderless *bw, UINT msg,
                                        WPARAM wparam, LPARAM lparam)
{
	LONG_PTR old_style = GetWindowLongPtrW(bw->hwnd, GWL_STYLE);

	/* Prevent Windows from drawing the default title bar by temporarily
	   toggling the WS_VISIBLE style. This is recommended in:
	   https://blogs.msdn.microsoft.com/wpfsdk/2008/09/08/custom-window-chrome-in-wpf/ */
	SetWindowLongPtrW(bw->hwnd, GWL_STYLE, old_style & ~WS_VISIBLE);
	LRESULT result = call_next(bw, msg, wparam, lparam);
	SetWindowLongPtrW(bw->hwnd, GWL_STYLE, old_style);

	return result;
}

/* Returns true and sets *result if the message was fully handled */
static bool handle_message(struct borderless *bw, UINT msg, WPARAM wparam,
                           LPARAM lparam, LRESULT *result)
{
	*result = 0;

	switch (msg) {
	case WM_DWMCOMPOSITIONCHANGED:
		handle_compositionchanged(bw);
		return true;
	case WM_DPICHANGED:
		bw->dpi = HIWORD(wparam);
		return false;
	case WM_NCACTIVATE:
		/* DefWindowProc won't repaint the window border if lParam (normally a
		   HRGN) is -1. This is recommended in:
		   https://blogs.msdn.microsoft.com/wpfsdk/2008/09/08/custom-window-chrome-in-wpf/ */
		*result = call_next(bw, msg, wparam, -1);
		return true;
	case WM_NCCALCSIZE:
		handle_nccalcsize(bw, wparam, lparam);
		return true;
	case WM_NCHITTEST:
		*result = borderless_hit_test(bw, GET_X_LPARAM(lparam),
		                              GET_Y_LPARAM(lparam));
		return true;
	case WM_NCPAINT:
		/* Only block WM_NCPAINT when composition is disabled. If it's blocked
		   when composition is enabled, the window shadow won't be drawn. */
		return !bw->composition_enabled;
	case WM_NCUAHDRAWCAPTION:
	case WM_NCUAHDRAWFRAME:
		/* These undocumented messages are sent to draw themed window borders.
		   Block them to prevent drawing borders over the client area. */
		return true;
	case WM_SETICON:
	case WM_SETTEXT:
		/* Disable painting while these messages are handled to prevent them
		   from drawing a window caption over the client area, but only when
		   composition and theming are disabled. These messages don't paint
		   when composition is enabled and blocking WM_NCUAHDRAWCAPTION should
		   be enough to prevent painting when theming is enabled. */
		if (bw->composition_enabled || bw->theme_enabled)
			return false;
		*result = handle_message_invisible(bw, msg, wparam, lparam);
		return true;
	case WM_THEMECHANGED:
		handle_themechanged(bw);
		return false;
	case WM_WINDOWPOSCHANGED:
		handle_windowposchanged(bw, (const WINDOWPOS *)lparam);
		/* Pass it on, so WM_SIZE and WM_MOVE are still sent */
		return false;
	}

	return false;
}

static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                      LPARAM lparam)
{
	struct borderless *bw = borderless_from_hwnd(hwnd);
	if (!bw)
		return DefWindowProcW(hwnd, msg, wparam, lparam);

	if (msg == WM_NCDESTROY) {
		/* Last message the window receives. Free the state before forwarding
		   it, but keep the next procedure so it can still be called. */
		WNDPROC next = bw->next_proc;
		RemovePropW(hwnd, PROP_NAME);
		if ((WNDPROC)GetWindowLongPtrW(hwnd, GWLP_WNDPROC) == subclass_proc)
			SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)next);
		free(bw);
		return CallWindowProcW(next, hwnd, msg, wparam, lparam);
	}

	LRESULT result;
	if (handle_message(bw, msg, wparam, lparam, &result))
		return result;
	if (bw->message_hook &&
	    bw->message_hook(bw->message_hook_user, msg, wparam, lparam, &result))
		return result;
	return call_next(bw, msg, wparam, lparam);
}

/* Makes Windows recalculate the frame. The client area changes size while the
   window rectangle stays the same, which OpenGL drivers (and possibly other
   renderers) don't notice: they keep the old surface size until the window is
   really resized. So briefly resize it by one pixel to make them catch up. */
static void frame_changed(HWND hwnd)
{
	const UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
	SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE | flags);

	if (IsZoomed(hwnd) || IsIconic(hwnd))
		return;
	RECT r;
	GetWindowRect(hwnd, &r);
	SetWindowPos(hwnd, NULL, 0, 0, r.right - r.left + 1, r.bottom - r.top,
	             flags);
	SetWindowPos(hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, flags);
}

/* --- Public API ---------------------------------------------------------- */

struct borderless *borderless_from_hwnd(HWND hwnd)
{
	return GetPropW(hwnd, PROP_NAME);
}

struct borderless *borderless_attach(HWND hwnd)
{
	if (!hwnd || borderless_from_hwnd(hwnd))
		return NULL;

	struct borderless *bw = calloc(1, sizeof *bw);
	if (!bw)
		return NULL;

	bw->hwnd = hwnd;
	bw->dpi = get_window_dpi(hwnd);

	if (!SetPropW(hwnd, PROP_NAME, bw)) {
		free(bw);
		return NULL;
	}
	bw->next_proc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
	                                           (LONG_PTR)subclass_proc);

	handle_themechanged(bw);
	handle_compositionchanged(bw);

	/* Make Windows recalculate the frame now that WM_NCCALCSIZE is handled */
	frame_changed(hwnd);

	return bw;
}

bool borderless_detach(struct borderless *bw)
{
	if (!bw)
		return false;

	HWND hwnd = bw->hwnd;
	/* Only unhook if nobody subclassed the window after us, otherwise their
	   subclass would be removed too */
	if ((WNDPROC)GetWindowLongPtrW(hwnd, GWLP_WNDPROC) != subclass_proc)
		return false;
	SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)bw->next_proc);
	RemovePropW(hwnd, PROP_NAME);
	free(bw);

	SetWindowRgn(hwnd, NULL, TRUE);
	DwmExtendFrameIntoClientArea(hwnd, &(MARGINS) { 0 });
	frame_changed(hwnd);
	return true;
}
