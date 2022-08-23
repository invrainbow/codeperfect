#include "ostype.hpp"

#if OS_LINUX

#include "world.hpp"

#include "window.hpp"
#include "utils.hpp"
#include "glcrap.hpp"

bool window_init_everything() {
    return glfwInit();
}

void* Window::get_native_handle() {
    return get_native_window_handle(window);
}

void Window::set_clipboard_string(ccstr string) {
    glfwSetClipboardString(window, string);
}

ccstr Window::get_clipboard_string() {
    auto ret = glfwGetClipboardString(window);
    if (!ret) return NULL;
    return cp_strdup(ret);
}

void poll_window_events() {
    glfwPollEvents();
}

bool Cursor::init(Cursor_Type _type) {
    type = _type;
    switch (type) {
    case CP_CUR_RESIZE_EW: cursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR); break;
    case CP_CUR_RESIZE_NS: cursor = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR); break;
    case CP_CUR_ARROW: cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR); break;
    case CP_CUR_IBEAM: cursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR); break;
    case CP_CUR_CROSSHAIR: cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR); break;
    case CP_CUR_POINTING_HAND: cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR); break;
    default: return false;
    }
    return (bool)cursor;
}

Key glfw_to_cp_key(int key) {
    switch (key) {
    case GLFW_KEY_UNKNOWN: return CP_KEY_UNKNOWN;
    case GLFW_KEY_SPACE: return CP_KEY_SPACE;
    case GLFW_KEY_APOSTROPHE: return CP_KEY_APOSTROPHE;
    case GLFW_KEY_COMMA: return CP_KEY_COMMA;
    case GLFW_KEY_MINUS: return CP_KEY_MINUS;
    case GLFW_KEY_PERIOD: return CP_KEY_PERIOD;
    case GLFW_KEY_SLASH: return CP_KEY_SLASH;
    case GLFW_KEY_0: return CP_KEY_0;
    case GLFW_KEY_1: return CP_KEY_1;
    case GLFW_KEY_2: return CP_KEY_2;
    case GLFW_KEY_3: return CP_KEY_3;
    case GLFW_KEY_4: return CP_KEY_4;
    case GLFW_KEY_5: return CP_KEY_5;
    case GLFW_KEY_6: return CP_KEY_6;
    case GLFW_KEY_7: return CP_KEY_7;
    case GLFW_KEY_8: return CP_KEY_8;
    case GLFW_KEY_9: return CP_KEY_9;
    case GLFW_KEY_SEMICOLON: return CP_KEY_SEMICOLON;
    case GLFW_KEY_EQUAL: return CP_KEY_EQUAL;
    case GLFW_KEY_A: return CP_KEY_A;
    case GLFW_KEY_B: return CP_KEY_B;
    case GLFW_KEY_C: return CP_KEY_C;
    case GLFW_KEY_D: return CP_KEY_D;
    case GLFW_KEY_E: return CP_KEY_E;
    case GLFW_KEY_F: return CP_KEY_F;
    case GLFW_KEY_G: return CP_KEY_G;
    case GLFW_KEY_H: return CP_KEY_H;
    case GLFW_KEY_I: return CP_KEY_I;
    case GLFW_KEY_J: return CP_KEY_J;
    case GLFW_KEY_K: return CP_KEY_K;
    case GLFW_KEY_L: return CP_KEY_L;
    case GLFW_KEY_M: return CP_KEY_M;
    case GLFW_KEY_N: return CP_KEY_N;
    case GLFW_KEY_O: return CP_KEY_O;
    case GLFW_KEY_P: return CP_KEY_P;
    case GLFW_KEY_Q: return CP_KEY_Q;
    case GLFW_KEY_R: return CP_KEY_R;
    case GLFW_KEY_S: return CP_KEY_S;
    case GLFW_KEY_T: return CP_KEY_T;
    case GLFW_KEY_U: return CP_KEY_U;
    case GLFW_KEY_V: return CP_KEY_V;
    case GLFW_KEY_W: return CP_KEY_W;
    case GLFW_KEY_X: return CP_KEY_X;
    case GLFW_KEY_Y: return CP_KEY_Y;
    case GLFW_KEY_Z: return CP_KEY_Z;
    case GLFW_KEY_LEFT_BRACKET: return CP_KEY_LEFT_BRACKET;
    case GLFW_KEY_BACKSLASH: return CP_KEY_BACKSLASH;
    case GLFW_KEY_RIGHT_BRACKET: return CP_KEY_RIGHT_BRACKET;
    case GLFW_KEY_GRAVE_ACCENT: return CP_KEY_GRAVE_ACCENT;
    case GLFW_KEY_WORLD_1: return CP_KEY_WORLD_1;
    case GLFW_KEY_WORLD_2: return CP_KEY_WORLD_2;
    case GLFW_KEY_ESCAPE: return CP_KEY_ESCAPE;
    case GLFW_KEY_ENTER: return CP_KEY_ENTER;
    case GLFW_KEY_TAB: return CP_KEY_TAB;
    case GLFW_KEY_BACKSPACE: return CP_KEY_BACKSPACE;
    case GLFW_KEY_INSERT: return CP_KEY_INSERT;
    case GLFW_KEY_DELETE: return CP_KEY_DELETE;
    case GLFW_KEY_RIGHT: return CP_KEY_RIGHT;
    case GLFW_KEY_LEFT: return CP_KEY_LEFT;
    case GLFW_KEY_DOWN: return CP_KEY_DOWN;
    case GLFW_KEY_UP: return CP_KEY_UP;
    case GLFW_KEY_PAGE_UP: return CP_KEY_PAGE_UP;
    case GLFW_KEY_PAGE_DOWN: return CP_KEY_PAGE_DOWN;
    case GLFW_KEY_HOME: return CP_KEY_HOME;
    case GLFW_KEY_END: return CP_KEY_END;
    case GLFW_KEY_CAPS_LOCK: return CP_KEY_CAPS_LOCK;
    case GLFW_KEY_SCROLL_LOCK: return CP_KEY_SCROLL_LOCK;
    case GLFW_KEY_NUM_LOCK: return CP_KEY_NUM_LOCK;
    case GLFW_KEY_PRINT_SCREEN: return CP_KEY_PRINT_SCREEN;
    case GLFW_KEY_PAUSE: return CP_KEY_PAUSE;
    case GLFW_KEY_F1: return CP_KEY_F1;
    case GLFW_KEY_F2: return CP_KEY_F2;
    case GLFW_KEY_F3: return CP_KEY_F3;
    case GLFW_KEY_F4: return CP_KEY_F4;
    case GLFW_KEY_F5: return CP_KEY_F5;
    case GLFW_KEY_F6: return CP_KEY_F6;
    case GLFW_KEY_F7: return CP_KEY_F7;
    case GLFW_KEY_F8: return CP_KEY_F8;
    case GLFW_KEY_F9: return CP_KEY_F9;
    case GLFW_KEY_F10: return CP_KEY_F10;
    case GLFW_KEY_F11: return CP_KEY_F11;
    case GLFW_KEY_F12: return CP_KEY_F12;
    case GLFW_KEY_F13: return CP_KEY_F13;
    case GLFW_KEY_F14: return CP_KEY_F14;
    case GLFW_KEY_F15: return CP_KEY_F15;
    case GLFW_KEY_F16: return CP_KEY_F16;
    case GLFW_KEY_F17: return CP_KEY_F17;
    case GLFW_KEY_F18: return CP_KEY_F18;
    case GLFW_KEY_F19: return CP_KEY_F19;
    case GLFW_KEY_F20: return CP_KEY_F20;
    case GLFW_KEY_F21: return CP_KEY_F21;
    case GLFW_KEY_F22: return CP_KEY_F22;
    case GLFW_KEY_F23: return CP_KEY_F23;
    case GLFW_KEY_F24: return CP_KEY_F24;
    case GLFW_KEY_F25: return CP_KEY_F25;
    case GLFW_KEY_KP_0: return CP_KEY_KP_0;
    case GLFW_KEY_KP_1: return CP_KEY_KP_1;
    case GLFW_KEY_KP_2: return CP_KEY_KP_2;
    case GLFW_KEY_KP_3: return CP_KEY_KP_3;
    case GLFW_KEY_KP_4: return CP_KEY_KP_4;
    case GLFW_KEY_KP_5: return CP_KEY_KP_5;
    case GLFW_KEY_KP_6: return CP_KEY_KP_6;
    case GLFW_KEY_KP_7: return CP_KEY_KP_7;
    case GLFW_KEY_KP_8: return CP_KEY_KP_8;
    case GLFW_KEY_KP_9: return CP_KEY_KP_9;
    case GLFW_KEY_KP_DECIMAL: return CP_KEY_KP_DECIMAL;
    case GLFW_KEY_KP_DIVIDE: return CP_KEY_KP_DIVIDE;
    case GLFW_KEY_KP_MULTIPLY: return CP_KEY_KP_MULTIPLY;
    case GLFW_KEY_KP_SUBTRACT: return CP_KEY_KP_SUBTRACT;
    case GLFW_KEY_KP_ADD: return CP_KEY_KP_ADD;
    case GLFW_KEY_KP_ENTER: return CP_KEY_KP_ENTER;
    case GLFW_KEY_KP_EQUAL: return CP_KEY_KP_EQUAL;
    case GLFW_KEY_LEFT_SHIFT: return CP_KEY_LEFT_SHIFT;
    case GLFW_KEY_LEFT_CONTROL: return CP_KEY_LEFT_CONTROL;
    case GLFW_KEY_LEFT_ALT: return CP_KEY_LEFT_ALT;
    case GLFW_KEY_LEFT_SUPER: return CP_KEY_LEFT_SUPER;
    case GLFW_KEY_RIGHT_SHIFT: return CP_KEY_RIGHT_SHIFT;
    case GLFW_KEY_RIGHT_CONTROL: return CP_KEY_RIGHT_CONTROL;
    case GLFW_KEY_RIGHT_ALT: return CP_KEY_RIGHT_ALT;
    case GLFW_KEY_RIGHT_SUPER: return CP_KEY_RIGHT_SUPER;
    case GLFW_KEY_MENU: return CP_KEY_MENU;
    }
    return CP_KEY_UNKNOWN;
}

bool Window::init_os_specific(int width, int height, ccstr title) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE);

    window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window) return false;

    glfwSetWindowCloseCallback(window, [](GLFWwindow*) {
        world.window->should_close = true;
    });

    glfwSetWindowSizeCallback(window, [](GLFWwindow*, int w, int h) {
        world.window->dispatch_event(WINEV_WINDOW_SIZE, [&](auto ev) {
            ev->window_size.w = w;
            ev->window_size.h = h;
        });
    });

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) {
        world.window->dispatch_event(WINEV_FRAME_SIZE, [&](auto ev) {
            ev->frame_size.w = w;
            ev->frame_size.h = h;
        });
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow*, double x, double y) {
        world.window->dispatch_event(WINEV_MOUSE_MOVE, [&](auto ev) {
            ev->mouse_move.x = x;
            ev->mouse_move.y = y;
        });
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow*, int button, int action, int mod) {
        Mouse_Button cp_button;
        switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT: cp_button = CP_MOUSE_LEFT; break;
        case GLFW_MOUSE_BUTTON_RIGHT: cp_button = CP_MOUSE_RIGHT; break;
        case GLFW_MOUSE_BUTTON_MIDDLE: cp_button = CP_MOUSE_MIDDLE; break;
        case GLFW_MOUSE_BUTTON_4: cp_button = CP_MOUSE_BUTTON_4; break;
        case GLFW_MOUSE_BUTTON_5: cp_button = CP_MOUSE_BUTTON_5; break;
        default: return;
        }

        bool press = false;
        switch (action) {
        case GLFW_PRESS: press = true; break;
        case GLFW_REPEAT: press = true; break;
        case GLFW_RELEASE: press = false; break;
        default: return;
        }

        int cp_mods = 0;
        if (mod & GLFW_MOD_SHIFT) cp_mods |= CP_MOD_SHIFT;
        if (mod & GLFW_MOD_ALT) cp_mods |= CP_MOD_ALT;
        if (mod & GLFW_MOD_CONTROL) cp_mods |= CP_MOD_CTRL;
        if (mod & GLFW_MOD_SUPER) cp_mods |= CP_MOD_CMD;

        world.window->dispatch_mouse_event(cp_button, press, cp_mods);
    });

    glfwSetScrollCallback(window, [](GLFWwindow*, double dx, double dy) {
        world.window->dispatch_event(WINEV_SCROLL, [&](auto ev) {
            ev->scroll.dx = dx;
            ev->scroll.dy = dy;
        });
    });

    glfwSetWindowContentScaleCallback(window, [](GLFWwindow*, float xscale, float yscale) {
        world.window->dispatch_event(WINEV_WINDOW_SCALE, [&](auto ev) {
            ev->window_scale.xscale = xscale;
            ev->window_scale.yscale = yscale;
        });
    });

    glfwSetKeyCallback(window, [](GLFWwindow*, int key, int, int action, int mod) {
        bool press;
        switch (action) {
        case GLFW_PRESS: press = true; break;
        case GLFW_REPEAT: press = true; break;
        case GLFW_RELEASE: press = false; break;
        default: return;
        }

        int cp_mods = 0;
        if (mod & GLFW_MOD_SHIFT) cp_mods |= CP_MOD_SHIFT;
        if (mod & GLFW_MOD_ALT) cp_mods |= CP_MOD_ALT;
        if (mod & GLFW_MOD_CONTROL) cp_mods |= CP_MOD_CTRL;
        if (mod & GLFW_MOD_SUPER) cp_mods |= CP_MOD_CMD;

        Key cp_key = glfw_to_cp_key(key);
        if (cp_key == CP_KEY_UNKNOWN) return;

        world.window->dispatch_event(WINEV_KEY, [&](auto ev) {
            ev->key.key = (Key)cp_key;
            ev->key.press = press;
            ev->key.mods = cp_mods;
        });
    });

    glfwSetCharCallback(window, [](GLFWwindow* wnd, u32 ch) {
        world.window->dispatch_event(WINEV_CHAR, [&](auto ev) {
            ev->character.ch = ch;
        });
    });

    return true;
}

void Window::make_context_current() {
    glfwMakeContextCurrent(window);
}

void Window::swap_buffers() {
    glfwSwapBuffers(window);
}

void Window::swap_interval(int interval) {
    glfwSwapInterval(interval);
}

// void Window::cleanup() {}

void Window::set_title(ccstr title) {
    glfwSetWindowTitle(window, title);
}

void Window::get_size(int* width, int* height) {
    glfwGetWindowSize(window, width, height);
}

void Window::get_framebuffer_size(int* width, int* height) {
    glfwGetFramebufferSize(window, width, height);
}

void Window::get_content_scale(float* xscale, float* yscale) {
    glfwGetWindowContentScale(window, xscale, yscale);
}

bool Window::is_focused() {
    return glfwGetWindowAttrib(window, GLFW_FOCUSED);
}

void Window::get_cursor_pos(double* xpos, double* ypos) {
    glfwGetCursorPos(window, xpos, ypos);
}

void Window::set_cursor(Cursor *_cursor) {
    cursor = _cursor;
    glfwSetCursor(window, cursor->cursor);
}

#endif // OS_LINUX
