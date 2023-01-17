#include "wintype.hpp"

#if WIN_WIN32

#include "world.hpp"
#include "win.hpp"
#include "defer.hpp"
#include "win32.hpp"
#include "enums.hpp"

#include <limits.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <GL/wglew.h>

#ifndef WM_COPYGLOBALDATA
#define WM_COPYGLOBALDATA 0x0049
#endif

static int scan_to_key_table[0x200];
static int key_to_scan_table[__CP_KEY_COUNT];

static DWORD window_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_MINIMIZEBOX | WS_CAPTION | WS_MAXIMIZEBOX | WS_THICKFRAME;
static DWORD window_style_ex = WS_EX_APPWINDOW;

int get_key_mods() {
    int mods = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) mods |= CP_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= CP_MOD_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000) mods |= CP_MOD_ALT;
    if (GetKeyState(VK_LWIN) & 0x8000) mods |= CP_MOD_CMD;
    if (GetKeyState(VK_RWIN) & 0x8000) mods |= CP_MOD_CMD;
    return mods;
}

ccstr Window::get_clipboard_string() {
    if (!OpenClipboard(NULL)) return NULL;
    defer { CloseClipboard(); };

    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle == NULL) return NULL;

    auto wbuf = (wchar_t*)GlobalLock(handle);
    if (!wbuf) return NULL;
    defer { GlobalUnlock(handle); };

    return to_utf8(wbuf);
}

void Window::set_clipboard_string(ccstr text) {
    if (!OpenClipboard(NULL)) return;
    defer { CloseClipboard(); };

    auto wtext = to_wide(text);
    auto wlen = wcslen(wtext);

    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)wlen * sizeof(WCHAR));
    if (handle == NULL) return;

    WCHAR* wbuf = (WCHAR*)GlobalLock(handle);
    memcpy(wbuf, wtext, sizeof(WCHAR) * (wlen + 1));
    GlobalUnlock(handle);

    EmptyClipboard();
    if (SetClipboardData(CF_UNICODETEXT, handle) == NULL)
        GlobalFree(handle);
}

bool is_gte_win10_build(WORD build) {
    OSVERSIONINFOEXW osvi; ptr0(&osvi);
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = 10;
    osvi.dwMinorVersion = 0;
    osvi.dwBuildNumber = build;

    ULONGLONG cond = 0;
    cond = VerSetConditionMask(cond, VER_MAJORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_MINORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_BUILDNUMBER, VER_GREATER_EQUAL);

#ifndef RtlVerifyVersionInfo
    typedef LONG (WINAPI * pfRtlVerifyVersionInfo)(OSVERSIONINFOEXW*, ULONG, ULONGLONG);

    static pfRtlVerifyVersionInfo RtlVerifyVersionInfo;
    static HMODULE ntdll;

    if (RtlVerifyVersionInfo == NULL) {
        if (!ntdll) {
            ntdll = LoadLibraryA("ntdll.dll");
            if (!ntdll) return false;
        }
        RtlVerifyVersionInfo = (pfRtlVerifyVersionInfo)GetProcAddress(ntdll, "RtlVerifyVersionInfo");
        if (!RtlVerifyVersionInfo) return false;
    }
#endif

    return RtlVerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, cond) == 0;
}

#define is_gte_win10_ver1607() is_gte_win10_build(14393)
#define is_gte_win10_ver1703() is_gte_win10_build(15063)

/*
struct {
    static HWND helperwin;
    static ATOM helperwin_class;
} win32_globals;
*/

bool window_init_everything() {
    memset(scan_to_key_table, -1, sizeof(scan_to_key_table));
    memset(key_to_scan_table, -1, sizeof(key_to_scan_table));

    scan_to_key_table[0x00B] = CP_KEY_0;
    scan_to_key_table[0x002] = CP_KEY_1;
    scan_to_key_table[0x003] = CP_KEY_2;
    scan_to_key_table[0x004] = CP_KEY_3;
    scan_to_key_table[0x005] = CP_KEY_4;
    scan_to_key_table[0x006] = CP_KEY_5;
    scan_to_key_table[0x007] = CP_KEY_6;
    scan_to_key_table[0x008] = CP_KEY_7;
    scan_to_key_table[0x009] = CP_KEY_8;
    scan_to_key_table[0x00A] = CP_KEY_9;
    scan_to_key_table[0x01E] = CP_KEY_A;
    scan_to_key_table[0x030] = CP_KEY_B;
    scan_to_key_table[0x02E] = CP_KEY_C;
    scan_to_key_table[0x020] = CP_KEY_D;
    scan_to_key_table[0x012] = CP_KEY_E;
    scan_to_key_table[0x021] = CP_KEY_F;
    scan_to_key_table[0x022] = CP_KEY_G;
    scan_to_key_table[0x023] = CP_KEY_H;
    scan_to_key_table[0x017] = CP_KEY_I;
    scan_to_key_table[0x024] = CP_KEY_J;
    scan_to_key_table[0x025] = CP_KEY_K;
    scan_to_key_table[0x026] = CP_KEY_L;
    scan_to_key_table[0x032] = CP_KEY_M;
    scan_to_key_table[0x031] = CP_KEY_N;
    scan_to_key_table[0x018] = CP_KEY_O;
    scan_to_key_table[0x019] = CP_KEY_P;
    scan_to_key_table[0x010] = CP_KEY_Q;
    scan_to_key_table[0x013] = CP_KEY_R;
    scan_to_key_table[0x01F] = CP_KEY_S;
    scan_to_key_table[0x014] = CP_KEY_T;
    scan_to_key_table[0x016] = CP_KEY_U;
    scan_to_key_table[0x02F] = CP_KEY_V;
    scan_to_key_table[0x011] = CP_KEY_W;
    scan_to_key_table[0x02D] = CP_KEY_X;
    scan_to_key_table[0x015] = CP_KEY_Y;
    scan_to_key_table[0x02C] = CP_KEY_Z;
    scan_to_key_table[0x028] = CP_KEY_APOSTROPHE;
    scan_to_key_table[0x02B] = CP_KEY_BACKSLASH;
    scan_to_key_table[0x033] = CP_KEY_COMMA;
    scan_to_key_table[0x00D] = CP_KEY_EQUAL;
    scan_to_key_table[0x029] = CP_KEY_GRAVE_ACCENT;
    scan_to_key_table[0x01A] = CP_KEY_LEFT_BRACKET;
    scan_to_key_table[0x00C] = CP_KEY_MINUS;
    scan_to_key_table[0x034] = CP_KEY_PERIOD;
    scan_to_key_table[0x01B] = CP_KEY_RIGHT_BRACKET;
    scan_to_key_table[0x027] = CP_KEY_SEMICOLON;
    scan_to_key_table[0x035] = CP_KEY_SLASH;
    scan_to_key_table[0x056] = CP_KEY_WORLD_2;
    scan_to_key_table[0x00E] = CP_KEY_BACKSPACE;
    scan_to_key_table[0x153] = CP_KEY_DELETE;
    scan_to_key_table[0x14F] = CP_KEY_END;
    scan_to_key_table[0x01C] = CP_KEY_ENTER;
    scan_to_key_table[0x001] = CP_KEY_ESCAPE;
    scan_to_key_table[0x147] = CP_KEY_HOME;
    scan_to_key_table[0x152] = CP_KEY_INSERT;
    scan_to_key_table[0x15D] = CP_KEY_MENU;
    scan_to_key_table[0x151] = CP_KEY_PAGE_DOWN;
    scan_to_key_table[0x149] = CP_KEY_PAGE_UP;
    scan_to_key_table[0x045] = CP_KEY_PAUSE;
    scan_to_key_table[0x039] = CP_KEY_SPACE;
    scan_to_key_table[0x00F] = CP_KEY_TAB;
    scan_to_key_table[0x03A] = CP_KEY_CAPS_LOCK;
    scan_to_key_table[0x145] = CP_KEY_NUM_LOCK;
    scan_to_key_table[0x046] = CP_KEY_SCROLL_LOCK;
    scan_to_key_table[0x03B] = CP_KEY_F1;
    scan_to_key_table[0x03C] = CP_KEY_F2;
    scan_to_key_table[0x03D] = CP_KEY_F3;
    scan_to_key_table[0x03E] = CP_KEY_F4;
    scan_to_key_table[0x03F] = CP_KEY_F5;
    scan_to_key_table[0x040] = CP_KEY_F6;
    scan_to_key_table[0x041] = CP_KEY_F7;
    scan_to_key_table[0x042] = CP_KEY_F8;
    scan_to_key_table[0x043] = CP_KEY_F9;
    scan_to_key_table[0x044] = CP_KEY_F10;
    scan_to_key_table[0x057] = CP_KEY_F11;
    scan_to_key_table[0x058] = CP_KEY_F12;
    scan_to_key_table[0x064] = CP_KEY_F13;
    scan_to_key_table[0x065] = CP_KEY_F14;
    scan_to_key_table[0x066] = CP_KEY_F15;
    scan_to_key_table[0x067] = CP_KEY_F16;
    scan_to_key_table[0x068] = CP_KEY_F17;
    scan_to_key_table[0x069] = CP_KEY_F18;
    scan_to_key_table[0x06A] = CP_KEY_F19;
    scan_to_key_table[0x06B] = CP_KEY_F20;
    scan_to_key_table[0x06C] = CP_KEY_F21;
    scan_to_key_table[0x06D] = CP_KEY_F22;
    scan_to_key_table[0x06E] = CP_KEY_F23;
    scan_to_key_table[0x076] = CP_KEY_F24;
    scan_to_key_table[0x038] = CP_KEY_LEFT_ALT;
    scan_to_key_table[0x01D] = CP_KEY_LEFT_CONTROL;
    scan_to_key_table[0x02A] = CP_KEY_LEFT_SHIFT;
    scan_to_key_table[0x15B] = CP_KEY_LEFT_SUPER;
    scan_to_key_table[0x137] = CP_KEY_PRINT_SCREEN;
    scan_to_key_table[0x138] = CP_KEY_RIGHT_ALT;
    scan_to_key_table[0x11D] = CP_KEY_RIGHT_CONTROL;
    scan_to_key_table[0x036] = CP_KEY_RIGHT_SHIFT;
    scan_to_key_table[0x15C] = CP_KEY_RIGHT_SUPER;
    scan_to_key_table[0x150] = CP_KEY_DOWN;
    scan_to_key_table[0x14B] = CP_KEY_LEFT;
    scan_to_key_table[0x14D] = CP_KEY_RIGHT;
    scan_to_key_table[0x148] = CP_KEY_UP;
    scan_to_key_table[0x052] = CP_KEY_KP_0;
    scan_to_key_table[0x04F] = CP_KEY_KP_1;
    scan_to_key_table[0x050] = CP_KEY_KP_2;
    scan_to_key_table[0x051] = CP_KEY_KP_3;
    scan_to_key_table[0x04B] = CP_KEY_KP_4;
    scan_to_key_table[0x04C] = CP_KEY_KP_5;
    scan_to_key_table[0x04D] = CP_KEY_KP_6;
    scan_to_key_table[0x047] = CP_KEY_KP_7;
    scan_to_key_table[0x048] = CP_KEY_KP_8;
    scan_to_key_table[0x049] = CP_KEY_KP_9;
    scan_to_key_table[0x04E] = CP_KEY_KP_ADD;
    scan_to_key_table[0x053] = CP_KEY_KP_DECIMAL;
    scan_to_key_table[0x135] = CP_KEY_KP_DIVIDE;
    scan_to_key_table[0x11C] = CP_KEY_KP_ENTER;
    scan_to_key_table[0x059] = CP_KEY_KP_EQUAL;
    scan_to_key_table[0x037] = CP_KEY_KP_MULTIPLY;
    scan_to_key_table[0x04A] = CP_KEY_KP_SUBTRACT;

    for (int i = 0; i < _countof(scan_to_key_table); i++) {
        auto it = scan_to_key_table[i];
        if (it != CP_KEY_UNKNOWN)
            key_to_scan_table[it] = i;
    }

    if (is_gte_win10_ver1703())
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    SetProcessDPIAware();

    return true;
}

void* Window::get_native_handle() {
    return (void*)win32_window;
}

void Window::make_context_current() {
    wglMakeCurrent(wgl_dc, wgl_context);
}

void Window::swap_buffers() {
    SwapBuffers(wgl_dc);
}

void Window::swap_interval(int interval) {
    if (WGLEW_EXT_swap_control)
        wglSwapIntervalEXT(interval);
}

void Window::set_title(ccstr title) {
    auto wtitle = to_wide(title);
    if (!wtitle) return;

    SetWindowTextW(win32_window, wtitle);
}

void Window::get_size(int* w, int* h) {
    RECT area; ptr0(&area);
    GetClientRect(win32_window, &area);

    if (w) *w = area.right;
    if (h) *h = area.bottom;
}

void Window::adjust_rect_using_windows_gayness(RECT *rect) {
    if (is_gte_win10_ver1607())
        AdjustWindowRectExForDpi(rect, window_style, FALSE, window_style_ex, GetDpiForWindow(win32_window));
    else
        AdjustWindowRectEx(rect, window_style, FALSE, window_style_ex);
}

void Window::get_framebuffer_size(int* w, int* h) {
    // shouldn't this be adjusted for content scale?
    // TODO: test with a high dpi (known to 2x content scale) monitor?
    get_size(w, h);
}

void Window::get_content_scale(float* xscale, float* yscale) {
    auto h = MonitorFromWindow(win32_window, MONITOR_DEFAULTTONEAREST);

    UINT xdpi = USER_DEFAULT_SCREEN_DPI;
    UINT ydpi = USER_DEFAULT_SCREEN_DPI;
    GetDpiForMonitor(h, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);

    if (xscale) *xscale = xdpi / (float)USER_DEFAULT_SCREEN_DPI;
    if (yscale) *yscale = ydpi / (float)USER_DEFAULT_SCREEN_DPI;
}

bool Window::is_focused() {
    return GetActiveWindow() == win32_window;
}

void Window::get_cursor_pos(double* xpos, double* ypos) {
    POINT pos; ptr0(&pos);
    GetCursorPos(&pos);
    ScreenToClient(win32_window, &pos);

    if (xpos) *xpos = pos.x;
    if (ypos) *ypos = pos.y;
}

void Window::set_cursor(Cursor *_cursor) {
    RECT area;
    POINT pos;

    if (!GetCursorPos(&pos)) return;
    if (WindowFromPoint(pos) != win32_window) return;

    GetClientRect(win32_window, &area);
    ClientToScreen(win32_window, (POINT*) &area.left);
    ClientToScreen(win32_window, (POINT*) &area.right);

    if (!PtInRect(&area, pos)) return;

    cursor = _cursor;
    SetCursor(cursor->handle);
}

// might need to reuse for linux later too
struct Framebuffer_Config {
    int red_bits;
    int green_bits;
    int blue_bits;
    int alpha_bits;
    int depth_bits;
    int stencil_bits;
    int accum_red_bits;
    int accum_green_bits;
    int accum_blue_bits;
    int accum_alpha_bits;
    int aux_buffers;
    int samples;
    bool srgb;
    bool doublebuffer;
    bool transparent;
    int pixel_format_id; // used by windows
};

int get_pixel_format(HDC dc) {
    auto fbconfigs = alloc_list<Framebuffer_Config>();

    auto attribs = alloc_list<int>();
    int nformats;

    if (WGLEW_ARB_pixel_format) {
        int attrib = WGL_NUMBER_PIXEL_FORMATS_ARB;
        if (!wglGetPixelFormatAttribivARB(dc, 1, 0, 1, &attrib, &nformats))
            return 0;

        attribs->append(WGL_SUPPORT_OPENGL_ARB);
        attribs->append(WGL_DRAW_TO_WINDOW_ARB);
        attribs->append(WGL_PIXEL_TYPE_ARB);
        attribs->append(WGL_ACCELERATION_ARB);
        attribs->append(WGL_RED_BITS_ARB);
        attribs->append(WGL_RED_SHIFT_ARB);
        attribs->append(WGL_GREEN_BITS_ARB);
        attribs->append(WGL_GREEN_SHIFT_ARB);
        attribs->append(WGL_BLUE_BITS_ARB);
        attribs->append(WGL_BLUE_SHIFT_ARB);
        attribs->append(WGL_ALPHA_BITS_ARB);
        attribs->append(WGL_ALPHA_SHIFT_ARB);
        attribs->append(WGL_DEPTH_BITS_ARB);
        attribs->append(WGL_STENCIL_BITS_ARB);
        attribs->append(WGL_ACCUM_BITS_ARB);
        attribs->append(WGL_ACCUM_RED_BITS_ARB);
        attribs->append(WGL_ACCUM_GREEN_BITS_ARB);
        attribs->append(WGL_ACCUM_BLUE_BITS_ARB);
        attribs->append(WGL_ACCUM_ALPHA_BITS_ARB);
        attribs->append(WGL_AUX_BUFFERS_ARB);
        attribs->append(WGL_DOUBLE_BUFFER_ARB);

        if (WGLEW_ARB_multisample)
            attribs->append(WGL_SAMPLES_ARB);
        if (WGLEW_ARB_framebuffer_sRGB || WGLEW_EXT_framebuffer_sRGB)
            attribs->append(WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB);
    } else {
        nformats = DescribePixelFormat(dc, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);
    }

    for (int i = 0; i < nformats; i++) {
        auto out = fbconfigs->append();
        auto pixel_format_id = i + 1;

        if (WGLEW_ARB_pixel_format) {
            auto values = alloc_list<int>();
            if (!wglGetPixelFormatAttribivARB(dc, pixel_format_id, 0, attribs->len, attribs->items, values->items))
                return 0;

            auto find_attrib_value = [&](int a) -> int {
                Fori (attribs)
                    if (it == a)
                        return values->at(i);
                return 0;
            };

            if (!find_attrib_value(WGL_SUPPORT_OPENGL_ARB)) continue;
            if (!find_attrib_value(WGL_DRAW_TO_WINDOW_ARB)) continue;
            if (find_attrib_value(WGL_PIXEL_TYPE_ARB) != WGL_TYPE_RGBA_ARB) continue;
            if (find_attrib_value(WGL_ACCELERATION_ARB) == WGL_NO_ACCELERATION_ARB) continue;
            if (!find_attrib_value(WGL_DOUBLE_BUFFER_ARB)) continue;

            out->red_bits = find_attrib_value(WGL_RED_BITS_ARB);
            out->green_bits = find_attrib_value(WGL_GREEN_BITS_ARB);
            out->blue_bits = find_attrib_value(WGL_BLUE_BITS_ARB);
            out->alpha_bits = find_attrib_value(WGL_ALPHA_BITS_ARB);
            out->depth_bits = find_attrib_value(WGL_DEPTH_BITS_ARB);
            out->stencil_bits = find_attrib_value(WGL_STENCIL_BITS_ARB);
            out->accum_red_bits = find_attrib_value(WGL_ACCUM_RED_BITS_ARB);
            out->accum_green_bits = find_attrib_value(WGL_ACCUM_GREEN_BITS_ARB);
            out->accum_blue_bits = find_attrib_value(WGL_ACCUM_BLUE_BITS_ARB);
            out->accum_alpha_bits = find_attrib_value(WGL_ACCUM_ALPHA_BITS_ARB);
            out->aux_buffers = find_attrib_value(WGL_AUX_BUFFERS_ARB);

            if (WGLEW_ARB_multisample)
                out->samples = find_attrib_value(WGL_SAMPLES_ARB);
            if (WGLEW_ARB_framebuffer_sRGB || WGLEW_EXT_framebuffer_sRGB)
                if (find_attrib_value(WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB))
                    out->srgb = true;
        } else {
            // Get pixel format attributes through legacy PFDs
            PIXELFORMATDESCRIPTOR pfd;
            if (!DescribePixelFormat(dc, pixel_format_id, sizeof(PIXELFORMATDESCRIPTOR), &pfd))
                return 0;

            auto f = pfd.dwFlags;
            if (!(f & PFD_DRAW_TO_WINDOW)) continue;
            if (!(f & PFD_SUPPORT_OPENGL)) continue;
            if (!(f & PFD_GENERIC_ACCELERATED) && (f & PFD_GENERIC_FORMAT)) continue;
            if (!(f & PFD_DOUBLEBUFFER)) continue;
            if (pfd.iPixelType != PFD_TYPE_RGBA) continue;

            out->red_bits = pfd.cRedBits;
            out->green_bits = pfd.cGreenBits;
            out->blue_bits = pfd.cBlueBits;
            out->alpha_bits = pfd.cAlphaBits;
            out->depth_bits = pfd.cDepthBits;
            out->stencil_bits = pfd.cStencilBits;
            out->accum_red_bits = pfd.cAccumRedBits;
            out->accum_green_bits = pfd.cAccumGreenBits;
            out->accum_blue_bits = pfd.cAccumBlueBits;
            out->accum_alpha_bits = pfd.cAccumAlphaBits;
            out->aux_buffers = pfd.cAuxBuffers;
        }
        out->pixel_format_id = pixel_format_id;
    }

    if (!fbconfigs->len) return 0;

    Framebuffer_Config want; ptr0(&want);
    want.red_bits = 8;
    want.green_bits = 8;
    want.blue_bits = 8;
    want.alpha_bits = 8;
    want.depth_bits = 24;
    want.stencil_bits = 8;
    want.doublebuffer = true;

    struct Score {
        int missing;
        int colordiff;
        int extradiff;
    };

    auto scores = alloc_list<Score>();

    Fori (fbconfigs) {
        int missing = 0;
        if (!it.alpha_bits) missing++;
        if (!it.depth_bits) missing++;
        if (!it.stencil_bits) missing++;

        auto square = [&](int a) -> int { return a*a; };

        int colordiff = 0;
        colordiff += square(want.red_bits - it.red_bits);
        colordiff += square(want.green_bits - it.green_bits);
        colordiff += square(want.blue_bits - it.blue_bits);

        int extradiff = 0;
        extradiff += square((want.alpha_bits - it.alpha_bits));
        extradiff += square((want.depth_bits - it.depth_bits));
        extradiff += square((want.stencil_bits - it.stencil_bits));
        extradiff += square((want.accum_red_bits - it.accum_red_bits));
        extradiff += square((want.accum_green_bits - it.accum_green_bits));
        extradiff += square((want.accum_blue_bits - it.accum_blue_bits));
        extradiff += square((want.accum_alpha_bits - it.accum_alpha_bits));
        extradiff += square((want.samples - it.samples));
        if (want.srgb != it.srgb) extradiff++;

        auto score = scores->append();
        score->missing = missing;
        score->colordiff = colordiff;
        score->extradiff = extradiff;
    }

    int winner = 0;

    Fori (scores) {
        if (i == 0) continue;

        auto compare_scores = [&](auto &a, auto &b) -> int {
            if (a.missing != b.missing) return a.missing - b.missing;
            if (a.colordiff != b.colordiff) return a.colordiff - b.colordiff;
            if (a.extradiff != b.extradiff) return a.extradiff - b.extradiff;
            return 0;
        };

        if (compare_scores(it, scores->at(winner)) < 0) winner = i;
    }

    return fbconfigs->at(winner).pixel_format_id;
}

bool Window::init_os_specific(int width, int height, ccstr title) {
    if (!create_actual_window(width, height, title)) return false;
    if (!create_wgl_context()) return false;
    make_context_current();

    ShowWindow(win32_window, SW_SHOWNA);
    BringWindowToTop(win32_window);
    SetForegroundWindow(win32_window);
    SetFocus(win32_window);
    return true;
}

static LRESULT CALLBACK window_callback(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    auto window = (Window*)GetPropW(hwnd, L"window_object_pointer");
    if (!window) return DefWindowProcW(hwnd, msg, w, l);

    return window->callback(hwnd, msg, w, l);
}

LRESULT Window::callback(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_SETFOCUS: {
        dispatch_event(WINEV_FOCUS, [&](auto ev) {});
        return 0;
    }

    case WM_KILLFOCUS: {
        dispatch_event(WINEV_BLUR, [&](auto ev) {});
        return 0;
    }

    case WM_CLOSE: {
        should_close = true;
        return 0;
    }

    case WM_CHAR:
    case WM_SYSCHAR: {
        if (0xd800 <= w && w <= 0xdbff) {
            win32_high_surrogate = (WCHAR)w;
        } else {
            uint32_t codepoint = 0;
            if (0xdc00 <= w && w <= 0xdfff) {
                if (win32_high_surrogate) {
                    codepoint += (win32_high_surrogate - 0xd800) << 10;
                    codepoint += (WCHAR) w - 0xdc00;
                    codepoint += 0x10000;
                }
            } else {
                codepoint = (WCHAR)w;
            }
            win32_high_surrogate = 0;
            dispatch_event(WINEV_CHAR, [&](auto ev) {
                ev->character.ch = codepoint;
            });
        }
        if (msg == WM_SYSCHAR) break;
        return 0;
    }

    case WM_UNICHAR: {
        if (w == UNICODE_NOCHAR) return TRUE;
        dispatch_event(WINEV_CHAR, [&](auto ev) {
            ev->character.ch = w;
        });
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        bool press = HIWORD(l) & KF_UP;
        int mods = get_key_mods();

        int scan = (HIWORD(l) & (KF_EXTENDED | 0xff));
        if (!scan) scan = MapVirtualKeyW((UINT)w, MAPVK_VK_TO_VSC);
        // Alt+PrtSc has a different scan than just PrtSc
        // Ctrl+Pause has a different scan than just Pause
        if (scan == 0x54) scan = 0x137;
        if (scan == 0x146) scan = 0x45;

        auto key = (Key)scan_to_key_table[scan];
        // print("keydown: %s", key_str(key));

        // The Ctrl keys require special handling
        if (w == VK_CONTROL) {
            if (HIWORD(l) & KF_EXTENDED) {
                key = CP_KEY_RIGHT_CONTROL;
            } else {
                // NOTE: Alt Gr sends Left Ctrl followed by Right Alt
                // We only want one event for Alt Gr, so if we detect this
                // sequence we discard this Left Ctrl message now and later
                // report Right Alt normally
                MSG next;
                DWORD time = GetMessageTime();

                if (PeekMessageW(&next, NULL, 0, 0, PM_NOREMOVE)) {
                    auto m = next.message;
                    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN || m == WM_KEYUP || m == WM_SYSKEYUP)
                        if (next.wParam == VK_MENU && (HIWORD(next.lParam) & KF_EXTENDED) && next.time == time)
                            break; // discard if next message is Right Alt down
                }

                // This is a regular Left Ctrl message
                key = CP_KEY_LEFT_CONTROL;
            }
        } else if (w == VK_PROCESSKEY) {
            // IME notifies that keys have been filtered by setting the
            // virtual key-code to VK_PROCESSKEY
            break;
        }

        // TODO: glfw says keydown is not reported for print screen key
        // so we have to do it manually
        // if this ever becomes a concern we'll handle it lol

        if (!press && w == VK_SHIFT) {
            // Release both Shift keys on Shift up event, as when both
            // are pressed the first release does not emit any event
            // The other half of this is in poll_window_events()
            dispatch_event(WINEV_KEY, [&](auto ev) {
                ev->key.key = CP_KEY_LEFT_SHIFT;
                ev->key.press = press;
                ev->key.mods = mods;
            });
            dispatch_event(WINEV_KEY, [&](auto ev) {
                ev->key.key = CP_KEY_RIGHT_SHIFT;
                ev->key.press = press;
                ev->key.mods = mods;
            });
        } else {
            dispatch_event(WINEV_KEY, [&](auto ev) {
                ev->key.key = key;
                ev->key.press = press;
                ev->key.mods = mods;
            });
        }

        break;
    }

    case WM_LBUTTONDOWN: dispatch_mouse_event_win32(CP_MOUSE_LEFT, true); return 0;
    case WM_MBUTTONDOWN: dispatch_mouse_event_win32(CP_MOUSE_MIDDLE, true); return 0;
    case WM_RBUTTONDOWN: dispatch_mouse_event_win32(CP_MOUSE_RIGHT, true); return 0;

    case WM_LBUTTONUP: dispatch_mouse_event_win32(CP_MOUSE_LEFT, false); return 0;
    case WM_MBUTTONUP: dispatch_mouse_event_win32(CP_MOUSE_MIDDLE, false); return 0;
    case WM_RBUTTONUP: dispatch_mouse_event_win32(CP_MOUSE_RIGHT, false); return 0;

    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
        dispatch_mouse_event_win32(
            GET_XBUTTON_WPARAM(w) == XBUTTON1 ? CP_MOUSE_BUTTON_4 : CP_MOUSE_BUTTON_5,
            msg == WM_XBUTTONDOWN
        );
        return TRUE;

    case WM_MOUSEMOVE:
        dispatch_event(WINEV_MOUSE_MOVE, [&](auto ev) {
            ev->mouse_move.x = GET_X_LPARAM(l);
            ev->mouse_move.y = GET_Y_LPARAM(l);
        });
        return 0;

    case WM_MOUSEWHEEL:
        dispatch_event(WINEV_SCROLL, [&](auto ev) {
            ev->scroll.dx = 0;
            ev->scroll.dy = (SHORT)HIWORD(w) / (double)WHEEL_DELTA;
        });
        return 0;

    case WM_MOUSEHWHEEL:
        dispatch_event(WINEV_SCROLL, [&](auto ev) {
            ev->scroll.dx = 0;
            ev->scroll.dy = -(SHORT)HIWORD(w) / (double)WHEEL_DELTA;
        });
        return 0;

    case WM_SIZE: {
        int w = LOWORD(l);
        int h = HIWORD(l);

        if (w != frame_w || h != frame_h) {
            frame_w = w;
            frame_h = h;
            dispatch_event(WINEV_FRAME_SIZE, [&](auto ev) {
                ev->frame_size.w = frame_w;
                ev->frame_size.h = frame_h;
            });
        }

        if (w != window_w || h != window_h) {
            window_w = w;
            window_h = h;
            dispatch_event(WINEV_WINDOW_SIZE, [&](auto ev) {
                ev->window_size.w = window_w;
                ev->window_size.h = window_h;
            });
        }

        return 0;
    }

    case WM_GETDPISCALEDSIZE:
        if (is_gte_win10_ver1703()) {
            RECT source = {0}, target = {0};
            SIZE* out = (SIZE*)l;

            AdjustWindowRectExForDpi(&source, window_style, FALSE, window_style_ex, GetDpiForWindow(win32_window));
            AdjustWindowRectExForDpi(&target, window_style, FALSE, window_style_ex, LOWORD(w));

            out->cx += (target.right - target.left) - (source.right - source.left);
            out->cy += (target.bottom - target.top) - (source.bottom - source.top);
            return TRUE;
        }
        break;

    case WM_DPICHANGED: {
        float x = HIWORD(w) / (float)USER_DEFAULT_SCREEN_DPI;
        float y = LOWORD(w) / (float)USER_DEFAULT_SCREEN_DPI;

        // Resize windowed mode windows that either permit rescaling or that
        // need it to compensate for non-client area scaling
        if (is_gte_win10_ver1703()) {
            auto out = (RECT*)l;
            SetWindowPos(win32_window, HWND_TOP, out->left, out->top, out->right - out->left, out->bottom - out->top, SWP_NOACTIVATE | SWP_NOZORDER);
        }

        if (x != xscale || y != yscale) {
            xscale = x;
            yscale = y;

            dispatch_event(WINEV_WINDOW_SCALE, [&](auto ev) {
                ev->window_scale.xscale = xscale;
                ev->window_scale.yscale = yscale;
            });
        }
        break;
    }

    case WM_SETCURSOR: {
        if (LOWORD(l) == HTCLIENT) {
            if (cursor) SetCursor(cursor->handle);
            return TRUE;
        }
        break;
    }

    case WM_DROPFILES:
        // TODO: ENG-170
        break;
    }

    return DefWindowProcW(hwnd, msg, w, l);
}

void Window::dispatch_mouse_event_win32(Mouse_Button button, bool press) {
    dispatch_mouse_event(button, press, get_key_mods());
}

bool Window::create_actual_window(int width, int height, ccstr title) {
    DWORD style = window_style | WS_MAXIMIZE;
    DWORD style_ex = window_style_ex;

    if (!win32_window_class) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = window_callback;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW));
        wc.lpszClassName = L"codeperfect window class";
        wc.hIcon = (HICON)LoadImageW(NULL, MAKEINTRESOURCEW(IDI_APPLICATION), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

        win32_window_class = RegisterClassExW(&wc);
        if (!win32_window_class) return false;
    }

    int xpos = CW_USEDEFAULT;
    int ypos = CW_USEDEFAULT;
    int fullw = 0, fullh = 0;
    {
        RECT rect = { 0, 0, width, height };
        adjust_rect_using_windows_gayness(&rect);
        fullw = rect.right - rect.left;
        fullh = rect.bottom - rect.top;
    }

    auto wtitle = to_wide(title);
    if (!wtitle) return false;

    win32_window = CreateWindowExW(style_ex, (LPWSTR)((DWORD)((WORD)(win32_window_class))), wtitle, style, xpos, ypos, fullw, fullh, NULL, NULL, GetModuleHandleW(NULL), NULL); // pass anything?
    if (!win32_window) return false;

    SetPropW(win32_window, L"window_object_pointer", this);

    ChangeWindowMessageFilterEx(win32_window, WM_DROPFILES, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(win32_window, WM_COPYDATA, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(win32_window, WM_COPYGLOBALDATA, MSGFLT_ALLOW, NULL);

    {
        RECT rect = { 0, 0, width, height };
        adjust_rect_using_windows_gayness(&rect);

        WINDOWPLACEMENT wp; ptr0(&wp);
        wp.length = sizeof(wp);
        GetWindowPlacement(win32_window, &wp);
        OffsetRect(&rect, wp.rcNormalPosition.left - rect.left, wp.rcNormalPosition.top - rect.top);

        wp.rcNormalPosition = rect;
        wp.showCmd = SW_HIDE;
        SetWindowPlacement(win32_window, &wp);
    }

    DragAcceptFiles(win32_window, TRUE);

    get_size(&window_w, &window_h);
    get_framebuffer_size(&frame_w, &frame_h);
    get_content_scale(&xscale, &yscale);
    return true;
}

static HDC bootstrap_hdc = NULL;
static HGLRC bootstrap_ctx = NULL;

void make_bootstrap_context() {
    bootstrap_hdc = GetDC(NULL);
    if (!bootstrap_hdc)
        cp_panic("Unable to get device context to bootstrap OpenGL context.");

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        8, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };

    int pixel_format = ChoosePixelFormat(bootstrap_hdc, &pfd);
    if (!pixel_format)
        cp_panic("Unable to get pixel format to bootstrap OpenGL context.");
    if (!SetPixelFormat(bootstrap_hdc, pixel_format, &pfd))
        cp_panic("Unable to set pixel format to bootstrap OpenGL context.");

    bootstrap_ctx = wglCreateContext(bootstrap_hdc);
    if (!bootstrap_ctx)
        cp_panic("Unable to create bootstrap OpenGL context.");
    if (!wglMakeCurrent(bootstrap_hdc, bootstrap_ctx))
        cp_panic("Unable to make bootstrap OpenGL context current.");
}

void destroy_bootstrap_context() {
    if (bootstrap_ctx) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(bootstrap_ctx);
    }

    if (bootstrap_hdc != NULL) {
        ReleaseDC(NULL, bootstrap_hdc);
        bootstrap_hdc = NULL;
    }
}

bool Window::create_wgl_context() {
    wgl_dc = GetDC(win32_window);
    if (!wgl_dc) return false;

    auto pixel_format_id = get_pixel_format(wgl_dc);
    if (!pixel_format_id) return false;

    PIXELFORMATDESCRIPTOR pfd;
    if (!DescribePixelFormat(wgl_dc, pixel_format_id, sizeof(pfd), &pfd)) return false;
    if (!SetPixelFormat(wgl_dc, pixel_format_id, &pfd)) return false;

    if (!WGLEW_ARB_create_context) return false;
    if (!WGLEW_ARB_create_context_profile) return false;

    auto attribs = alloc_list<int>();

    auto set_attrib = [&](int key, int val) {
        attribs->append(key);
        attribs->append(val);
    };

    set_attrib(WGL_CONTEXT_MAJOR_VERSION_ARB, 4);
    set_attrib(WGL_CONTEXT_MINOR_VERSION_ARB, 1);
    set_attrib(WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB);
    set_attrib(WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB);
    set_attrib(0, 0);

    wgl_context = wglCreateContextAttribsARB(wgl_dc, NULL, attribs->items);
    if (!wgl_context) return false;

    return true;
}

bool Cursor::init(Cursor_Type _type) {
    ptr0(this);
    type = _type;

    int id = 0;

    switch (_type) {
    case CP_CUR_ARROW: id = OCR_NORMAL; break;
    case CP_CUR_IBEAM: id = OCR_IBEAM; break;
    case CP_CUR_CROSSHAIR: id = OCR_CROSS; break;
    case CP_CUR_POINTING_HAND: id = OCR_HAND; break;
    case CP_CUR_RESIZE_EW: id = OCR_SIZEWE; break;
    case CP_CUR_RESIZE_NS: id = OCR_SIZENS; break;
    case CP_CUR_RESIZE_NWSE: id = OCR_SIZENWSE; break;
    case CP_CUR_RESIZE_NESW: id = OCR_SIZENESW; break;
    case CP_CUR_RESIZE_ALL: id = OCR_SIZEALL; break;
    case CP_CUR_NOT_ALLOWED: id = OCR_NO; break;
    }

    if (!id) return false;

    handle = (HCURSOR)LoadImageW(NULL, MAKEINTRESOURCEW(id), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    return (bool)handle;
}

void poll_window_events() {
    MSG msg; ptr0(&msg);
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            world.window->should_close = true;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // release modifier keys that the system did not emit WM_KEYUP for

    auto hwnd = GetActiveWindow();
    if (!hwnd) return;

    auto window = (Window*)GetPropW(hwnd, L"window_object_pointer");
    if (!window) return;

    const int keys[4][2] = {
        { VK_LSHIFT, CP_KEY_LEFT_SHIFT },
        { VK_RSHIFT, CP_KEY_RIGHT_SHIFT },
        { VK_LWIN, CP_KEY_LEFT_SUPER },
        { VK_RWIN, CP_KEY_RIGHT_SUPER }
    };

    for (int i = 0; i < 4; i++) {
        const int vk = keys[i][0];
        const int key = keys[i][1];

        if ((GetKeyState(vk) & 0x8000)) continue;
        if (window->key_states[key]) continue;

        window->dispatch_event(WINEV_KEY, [&](auto ev) {
            ev->key.key = key;
            ev->key.press = false;
            ev->key.mods = get_key_mods();
        });
    }
}

#endif // WIN_WIN32
