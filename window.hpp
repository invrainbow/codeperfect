#pragma once

#include "os.hpp"
#include "common.hpp"
#include <Carbon/Carbon.h>
#include "objc_id_shim.hpp"

enum Window_Event_Type {
    WINEV_WINDOW_SIZE,
    WINEV_FRAME_SIZE,
    WINEV_MOUSE_MOVE,
    WINEV_MOUSE,
    WINEV_SCROLL,
    WINEV_WINDOW_SCALE,
    WINEV_KEY,
    WINEV_CHAR,
};

enum Mouse_Button {
    CP_MOUSE_LEFT = 0,
    CP_MOUSE_RIGHT = 1,
    CP_MOUSE_MIDDLE = 2,
};

enum Press_Action {
    CP_ACTION_RELEASE = 0,
    CP_ACTION_PRESS = 1,
    CP_ACTION_REPEAT = 2,
};

struct Window_Event {
    Window_Event_Type type;

    union {
        struct {
            i32 w;
            i32 h;
        } window_size;

        struct {
            i32 w;
            i32 h;
        } frame_size;

        struct {
            double x;
            double y;
        } mouse_move;

        struct {
            Mouse_Button button;
            Press_Action action;
            int mods;
        } mouse;

        struct {
            double dx;
            double dy;
        } scroll;

        struct {
            float xscale;
            float yscale;
        } window_scale;

        struct {
            int key;
            int scan;
            Press_Action action;
            int mods;
        } key;

        struct {
            u32 ch;
        } character;
    };
};

enum Keymod {
    CP_MOD_NONE = 0,
    CP_MOD_CMD = 1 << 0,
    CP_MOD_SHIFT = 1 << 1,
    CP_MOD_ALT = 1 << 2,
    CP_MOD_CTRL = 1 << 3,
};

enum Key {
    CP_KEY_UNKNOWN = -1,
    CP_KEY_SPACE = 32,
    CP_KEY_APOSTROPHE = 39,  /* ' */
    CP_KEY_COMMA = 44,  /* , */
    CP_KEY_MINUS = 45,  /* - */
    CP_KEY_PERIOD = 46,  /* . */
    CP_KEY_SLASH = 47,  /* / */
    CP_KEY_0 = 48,
    CP_KEY_1 = 49,
    CP_KEY_2 = 50,
    CP_KEY_3 = 51,
    CP_KEY_4 = 52,
    CP_KEY_5 = 53,
    CP_KEY_6 = 54,
    CP_KEY_7 = 55,
    CP_KEY_8 = 56,
    CP_KEY_9 = 57,
    CP_KEY_SEMICOLON = 59,  /* ; */
    CP_KEY_EQUAL = 61,  /* = */
    CP_KEY_A = 65,
    CP_KEY_B = 66,
    CP_KEY_C = 67,
    CP_KEY_D = 68,
    CP_KEY_E = 69,
    CP_KEY_F = 70,
    CP_KEY_G = 71,
    CP_KEY_H = 72,
    CP_KEY_I = 73,
    CP_KEY_J = 74,
    CP_KEY_K = 75,
    CP_KEY_L = 76,
    CP_KEY_M = 77,
    CP_KEY_N = 78,
    CP_KEY_O = 79,
    CP_KEY_P = 80,
    CP_KEY_Q = 81,
    CP_KEY_R = 82,
    CP_KEY_S = 83,
    CP_KEY_T = 84,
    CP_KEY_U = 85,
    CP_KEY_V = 86,
    CP_KEY_W = 87,
    CP_KEY_X = 88,
    CP_KEY_Y = 89,
    CP_KEY_Z = 90,
    CP_KEY_LEFT_BRACKET = 91,  /* [ */
    CP_KEY_BACKSLASH = 92,  /* \ */
    CP_KEY_RIGHT_BRACKET = 93,  /* ] */
    CP_KEY_GRAVE_ACCENT = 96,  /* ` */
    CP_KEY_WORLD_1 = 161, /* non-US #1 */
    CP_KEY_WORLD_2 = 162, /* non-US #2 */

    CP_KEY_ESCAPE = 256,
    CP_KEY_ENTER,
    CP_KEY_TAB,
    CP_KEY_BACKSPACE,
    CP_KEY_INSERT,
    CP_KEY_DELETE,
    CP_KEY_RIGHT,
    CP_KEY_LEFT,
    CP_KEY_DOWN,
    CP_KEY_UP,
    CP_KEY_PAGE_UP,
    CP_KEY_PAGE_DOWN,
    CP_KEY_HOME,
    CP_KEY_END,
    CP_KEY_CAPS_LOCK,
    CP_KEY_SCROLL_LOCK,
    CP_KEY_NUM_LOCK,
    CP_KEY_PRINT_SCREEN,
    CP_KEY_PAUSE,
    CP_KEY_F1,
    CP_KEY_F2,
    CP_KEY_F3,
    CP_KEY_F4,
    CP_KEY_F5,
    CP_KEY_F6,
    CP_KEY_F7,
    CP_KEY_F8,
    CP_KEY_F9,
    CP_KEY_F10,
    CP_KEY_F11,
    CP_KEY_F12,
    CP_KEY_F13,
    CP_KEY_F14,
    CP_KEY_F15,
    CP_KEY_F16,
    CP_KEY_F17,
    CP_KEY_F18,
    CP_KEY_F19,
    CP_KEY_F20,
    CP_KEY_F21,
    CP_KEY_F22,
    CP_KEY_F23,
    CP_KEY_F24,
    CP_KEY_F25,
    CP_KEY_KP_0,
    CP_KEY_KP_1,
    CP_KEY_KP_2,
    CP_KEY_KP_3,
    CP_KEY_KP_4,
    CP_KEY_KP_5,
    CP_KEY_KP_6,
    CP_KEY_KP_7,
    CP_KEY_KP_8,
    CP_KEY_KP_9,
    CP_KEY_KP_DECIMAL,
    CP_KEY_KP_DIVIDE,
    CP_KEY_KP_MULTIPLY,
    CP_KEY_KP_SUBTRACT,
    CP_KEY_KP_ADD,
    CP_KEY_KP_ENTER,
    CP_KEY_KP_EQUAL,
    CP_KEY_LEFT_SHIFT,
    CP_KEY_LEFT_CONTROL,
    CP_KEY_LEFT_ALT,
    CP_KEY_LEFT_SUPER,
    CP_KEY_RIGHT_SHIFT,
    CP_KEY_RIGHT_CONTROL,
    CP_KEY_RIGHT_ALT,
    CP_KEY_RIGHT_SUPER,
    CP_KEY_MENU,
    CP_KEY_LAST,
};

#if OS_MAC
#   define CP_MOD_PRIMARY CP_MOD_CMD
#   define CP_MOD_TEXT CP_MOD_ALT
#else
#   define CP_MOD_PRIMARY CP_MOD_CTRL
#   define CP_MOD_TEXT CP_MOD_CTRL
#endif

enum Cursor_Type {
    CP_CUR_RESIZE_EW,
    CP_CUR_RESIZE_NS,
    CP_CUR_RESIZE_NWSE,
    CP_CUR_RESIZE_NESW,
    CP_CUR_ARROW,
    CP_CUR_IBEAM,
    CP_CUR_CROSSHAIR,
    CP_CUR_POINTING_HAND,
    CP_CUR_RESIZE_ALL,
    CP_CUR_NOT_ALLOWED,
};

struct Cursor {
    id object;
    Cursor_Type type;

    bool init(Cursor_Type _type);
    void cleanup();
};

struct Window {
    bool should_close;
    bool maximized;
    Cursor *cursor;

    int window_w;
    int window_h;
    int frame_w;
    int frame_h;

    Pool mem;
    List<Window_Event> events;

    id ns_window;
    id ns_view;
    id ns_delegate;
    id ns_layer;
    int ns_width;
    int ns_height;
    int ns_framewidth;
    int ns_frameheight;
    double ns_cursor_warp_dx;
    double ns_cursor_warp_dy;
    double ns_xscale;
    double ns_yscale;

    bool mouse_states[42];
    bool key_states[CP_KEY_LAST + 1];

    id nsgl_object;
    id nsgl_pixel_format;

    void dispatch_event(Window_Event_Type type, fn<void(Window_Event*)> cb) {
        Window_Event ev; ptr0(&ev);
        ev.type = type;
        cb(&ev);
        events.append(&ev);
    }

    void dispatch_mouse_event(Mouse_Button button, Press_Action action, int mods) {
        mouse_states[button] = (action != CP_ACTION_RELEASE);
        dispatch_event(WINEV_MOUSE, [&](auto ev) {
            ev->mouse.button = button;
            ev->mouse.action = action;
            ev->mouse.mods = mods;
        });
    }

    void update_cursor_image();
    void update_cursor_mode();
    bool is_cursor_in_content_area();
    void make_context_current();
    void swap_buffers();
    void swap_interval(int interval);
    bool create_nsgl_context();
    bool init(int width, int height, ccstr title);
    void cleanup();
    void set_title(ccstr title);
    void get_pos(int* xpos, int* ypos);
    void set_pos(int x, int y);
    void get_size(int* width, int* height);
    void set_size(int width, int height);
    void get_framebuffer_size(int* width, int* height);
    void get_content_scale(float* xscale, float* yscale);
    void restore();
    void maximize();
    void show();
    void hide();
    void request_attention();
    void focus();
    bool is_focused();
    void get_cursor_pos(double* xpos, double* ypos);
    void set_cursor(Cursor *_cursor);
    bool create_actual_window(int width, int height, ccstr title);
};

bool window_init_everything();
void set_clipboard_string(ccstr string);
ccstr get_clipboard_string();
void poll_window_events();
