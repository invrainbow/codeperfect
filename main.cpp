/*
TODO:
- indent is fucked up when creating an untitled buffer (but it works after we save, quit, re-run, reopen file)
- settings system
- investigate open source licenses for all the crap we're using
- we have a lot of ambiguity around:
    1) how to sync data with delve
    2) where the source of truth should be stored
- (mac) running processes from different folder (e.g. for ctrl+p) only works in tmux???
*/

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

#include "os.hpp"
#include "common.hpp"
#include "debugger.hpp"
#include "editor.hpp"
#include "list.hpp"
#include "go.hpp"
#include "utils.hpp"
#include "world.hpp"
#include "nvim.hpp"
#include "ui.hpp"
#include "tests.hpp"
#include "fzy_match.h"
#include "settings.hpp"

#include "imgui.h"
#include "veramono.hpp"

#if OS_WIN
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define MAX_PATH 260
#define CODE_FONT_SIZE 14
#define UI_FONT_SIZE 15
#define FRAME_RATE_CAP 60

static const char WINDOW_TITLE[] = "i need to think of a name";

char vert_shader[] = R"(
#version 410

in vec2 pos;
in vec2 uv;
in vec4 color;
in int mode;
in int texture_id;
out vec2 _uv;
out vec4 _color;
flat out int _mode;
flat out int _texture_id;
uniform mat4 projection;

void main(void) {
    _uv = uv;
    _color = color;
    _mode = mode;
    _texture_id = texture_id;
    gl_Position = projection * vec4(pos, 0, 1);
}
)";

char frag_shader[] = R"(
#version 410

in vec2 _uv;
in vec4 _color;
flat in int _mode;
flat in int _texture_id;
out vec4 outcolor;
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D tex3;
uniform sampler2D tex4;
uniform sampler2D tex5;

vec4 our_texture(vec2 uv) {
    if (_texture_id == 0) return texture(tex0, uv);
    if (_texture_id == 1) return texture(tex1, uv);
    if (_texture_id == 2) return texture(tex2, uv);
    if (_texture_id == 3) return texture(tex3, uv);
    if (_texture_id == 4) return texture(tex4, uv);
    if (_texture_id == 5) return texture(tex5, uv);
    return vec4(0);
}

void main(void) {
    switch (_mode) {
    case 0: // DRAW_SOLID
        outcolor = _color;
        break;
    case 1: // DRAW_FONT_MASK
        outcolor = vec4(_color.rgb, our_texture(_uv).r * _color.a);
        break;
    case 2: // DRAW_IMAGE
        outcolor = our_texture(_uv);
        break;
    case 3: // DRAW_IMAGE_MASK
        // outcolor = vec4(_color.rgb, (0.5 + dot(vec3(0.33, 0.33, 0.33), our_texture(_uv).rgb) * 0.5) * our_texture(_uv).a);
        outcolor = vec4(_color.rgb, our_texture(_uv).a);
        break;
    }
}
)";

char im_vert_shader[] = R"(
#version 410

in vec2 pos;
in vec2 uv;
in vec4 color;
out vec2 _uv;
out vec4 _color;
uniform mat4 projection;

void main(void) {
    _uv = uv;
    _color = color;
    gl_Position = projection * vec4(pos, 0, 1);
}
)";

char im_frag_shader[] = R"(
#version 410

in vec2 _uv;
in vec4 _color;
out vec4 outcolor;
uniform sampler2D tex;

void main(void) {
    outcolor = _color * texture(tex, _uv);
}
)";

GLint compile_program(cstr vert_code, cstr frag_code) {
    auto compile_shader = [](cstr code, u32 type) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, (const GLchar**)&code, NULL);
        glCompileShader(shader);

        i32 status;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if ((GLboolean)status) return shader;

        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        fprintf(stderr, "failed to build shader, error: %s", log);
        return 0;
    };

    auto vert = compile_shader(vert_code, GL_VERTEX_SHADER);
    if (vert == 0) return -1;

    auto frag = compile_shader(frag_code, GL_FRAGMENT_SHADER);
    if (frag == 0) return -1;

    GLint id = glCreateProgram();
    glAttachShader(id, vert);
    glAttachShader(id, frag);
    glLinkProgram(id);
    glDeleteShader(vert);
    glDeleteShader(frag);

    i32 status = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &status);

    if (status == 0) {
        char log[512];
        glGetProgramInfoLog(id, 512, NULL, log);
        fprintf(stderr, "error linking program: %s", log);
        return -1;
    }

    return id;
}

void new_ortho_matrix(float* mat, float l, float r, float b, float t) {
    mat[0] = 2 / (r - l);
    mat[1] = 0;
    mat[2] = 0;
    mat[3] = 0;

    mat[4] = 0;
    mat[5] = 2 / (t - b);
    mat[6] = 0;
    mat[7] = 0;

    mat[8] = 0;
    mat[9] = 0;
    mat[10] = -1;
    mat[11] = 0;

    mat[12] = -(r + l) / (r - l);
    mat[13] = -(t + b) / (t - b);
    mat[14] = 0;
    mat[15] = 1;
}

#define MAX_RETRIES 5

void send_nvim_keys(ccstr s) {
    auto& nv = world.nvim;
    nv.start_request_message("nvim_input", 1);
    nv.writer.write_string(s);
    nv.end_message();
}

enum {
    OUR_MOD_NONE = 0,
    OUR_MOD_CMD = 1 << 0,
    OUR_MOD_SHIFT = 1 << 1,
    OUR_MOD_ALT = 1 << 2,
    OUR_MOD_CTRL = 1 << 3,
};

struct Timer {
    u64 time;

    void init() {
        time = current_time_in_nanoseconds();
    }

    void log(ccstr s) {
        auto curr = current_time_in_nanoseconds();
        print("%dms: %s", (curr - time) / 1000000, s);
        time = curr;
    }
};

int main() {
    Timer t;
    t.init();

    if (run_tests()) return EXIT_SUCCESS;

    t.log("run_tests");

    world.init();

    t.log("world.init");

    SCOPED_MEM(&world.frame_mem);

    if (!glfwInit())
        return error("glfwInit failed"), EXIT_FAILURE;
    defer { glfwTerminate(); };

    t.log("glfw init");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    t.log("window hints");

    world.window = glfwCreateWindow(1280, 720, WINDOW_TITLE, NULL, NULL);
    if (world.window == NULL)
        return error("could not create window"), EXIT_FAILURE;

    t.log("actually create the window");

    glfwMakeContextCurrent(world.window);

    {
        glewExperimental = GL_TRUE;
        auto err = glewInit();
        if (err != GLEW_OK)
            return error("unable to init GLEW: %s", glewGetErrorString(err)), EXIT_FAILURE;
    }

    glfwSwapInterval(0);

    t.log("random shit");

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // ImGuI::StyleColorsLight();

    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
    io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

    io.SetClipboardTextFn = [](void*, ccstr s) { glfwSetClipboardString(world.window, s); };
    io.GetClipboardTextFn = [](void*) { return glfwGetClipboardString(world.window); };

    world.ui.cursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    world.ui.cursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    world.ui.cursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    world.ui.cursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    world.ui.cursors[ImGuiMouseCursor_Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    world.ui.cursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    world.ui.cursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    world.ui.cursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    world.ui.cursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);

    world.ui.program = compile_program(vert_shader, frag_shader);
    if (world.ui.program == -1)
        return error("could not compile shaders"), EXIT_FAILURE;

    world.ui.im_program = compile_program(im_vert_shader, im_frag_shader);
    if (world.ui.im_program == -1)
        return EXIT_FAILURE;

    t.log("fill out a bunch of shit");

    // grab window_size, display_size, and display_scale
    glfwGetWindowSize(world.window, (i32*)&world.window_size.x, (i32*)&world.window_size.y);
    glfwGetFramebufferSize(world.window, (i32*)&world.display_size.x, (i32*)&world.display_size.y);
    glfwGetWindowContentScale(world.window, &world.display_scale.x, &world.display_scale.y);

    // now that we have world.display_size, we can call wksp.activate_pane
    world.activate_pane(0);

    t.log("more shit");

    // initialize & bind textures
    glGenTextures(__TEXTURE_COUNT__, world.ui.textures);
    for (u32 i = 0; i < __TEXTURE_COUNT__; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, world.ui.textures[i]);
    }
    // from here, if we want to modify a texture, just set the active texture
    // are we going to run out of texture units?

    ui.init_sprite_texture();

    t.log("textures");

    {
        SCOPED_FRAME();

        s32 len = 0;

        // auto ui_font = read_font_data_from_first_found(&len, "Segoe UI");
        // assert(ui_font != NULL, "unable to load UI font");
        // world.ui.im_font_ui = io.Fonts->AddFontFromMemoryTTF(ui_font, len, UI_FONT_SIZE);

        world.ui.im_font_ui = io.Fonts->AddFontFromFileTTF("fonts/FiraSans-Regular.ttf", UI_FONT_SIZE);
        assert(world.ui.im_font_ui != NULL, "unable to load UI font");

        // auto mono_font = read_font_data_from_first_found(&len, "Courier New", "Consolas", "Menlo", "Courier New");
        // assert(mono_font != NULL, "unable to load code font");

        if (!world.font.init((u8*)vera_mono_ttf, CODE_FONT_SIZE, TEXTURE_FONT))
            panic("unable to load code font");

        world.ui.im_font_mono = io.Fonts->AddFontFromMemoryTTF(vera_mono_ttf, vera_mono_ttf_len, CODE_FONT_SIZE);
        assert(world.ui.im_font_mono != NULL, "unable to load code font");

        // init imgui texture
        u8* pixels;
        i32 width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        glActiveTexture(GL_TEXTURE0 + TEXTURE_FONT_IMGUI);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        // i don't think this is needed
        // io.Fonts->TexID = (void*)TEXTURE_FONT_IMGUI;
    }

    t.log("init fonts");

    glfwSetWindowSizeCallback(world.window, [](GLFWwindow* wnd, i32 w, i32 h) {
        world.window_size.x = w;
        world.window_size.y = h;

        auto& font = world.font;

        mat4f projection;
        new_ortho_matrix(projection, 0, w, h, 0);
        glUseProgram(world.ui.im_program);
        glUniformMatrix4fv(glGetUniformLocation(world.ui.im_program, "projection"), 1, GL_FALSE, (float*)projection);
    });

    glfwSetFramebufferSizeCallback(world.window, [](GLFWwindow* wnd, i32 w, i32 h) {
        world.display_size.x = w;
        world.display_size.y = h;

        mat4f projection;
        new_ortho_matrix(projection, 0, w, h, 0);
        glUseProgram(world.ui.program);
        glUniformMatrix4fv(glGetUniformLocation(world.ui.program, "projection"), 1, GL_FALSE, (float*)projection);
    });

    glfwSetCursorPosCallback(world.window, [](GLFWwindow* wnd, double x, double y) {
        world.ui.mouse_delta.x = x - world.ui.mouse_pos.x;
        world.ui.mouse_delta.y = y - world.ui.mouse_pos.y;
        world.ui.mouse_pos.x = x;
        world.ui.mouse_pos.y = y;

        if (world.resizing_pane != -1) {
            auto i = world.resizing_pane;

            auto& pane1 = world.panes[i];
            auto& pane2 = world.panes[i+1];
            auto delta = world.ui.mouse_delta.x;

            if (delta < (100 - pane1.width))
                delta = 100 - pane1.width;
            if (delta > pane2.width - 100)
                delta = pane2.width - 100;

            pane1.width += delta;
            pane2.width -= delta;
        }
    });

    glfwSetMouseButtonCallback(world.window, [](GLFWwindow* wnd, int button, int action, int mods) {
        // Don't set world.ui.mouse_down here. We set it based on
        // world.ui.mouse_just_pressed and some additional logic below, while
        // we're setting io.MouseDown for ImGui.

        if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(world.ui.mouse_just_pressed))
            world.ui.mouse_just_pressed[button] = true;
    });

    glfwSetScrollCallback(world.window, [](GLFWwindow* wnd, double dx, double dy) {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheelH += (float)dx;
        io.MouseWheel += (float)dy;

        dy *= (world.font.height * 3);

        auto add_dy = [&](i32* offset) {
            *offset = (dy > 0 ? (*offset + dy) : relu_sub(*offset, -dy));
        };

        /*
        {
            // TODO: scroll in current editor
            auto &ed = world.editor;
            auto FACTOR = 0.5;
            world.ui.scroll_buffer += (-dy);
            if (world.ui.scroll_buffer <= -FACTOR || world.ui.scroll_buffer >= FACTOR) {
                auto y = ed.cur.y;
                if (world.ui.scroll_buffer > 0)
                    y = min(ed.buf.lines.len - 1, y + world.ui.scroll_buffer / FACTOR);
                else
                    y = relu_sub(y, -world.ui.scroll_buffer / FACTOR);
                ed.move_cursor({(i32)min(ed.cur.x, ed.buf.lines[y].len), y});
                world.ui.scroll_buffer = fmod(world.ui.scroll_buffer, FACTOR);
            }
        }
        */
    });

    glfwSetWindowContentScaleCallback(world.window, [](GLFWwindow* wnd, float xscale, float yscale) {
        world.display_scale = { xscale, yscale };
    });

    glfwSetKeyCallback(world.window, [](GLFWwindow* wnd, i32 key, i32 scan, i32 ev, i32 mod) {
        ImGuiIO& io = ImGui::GetIO();
        if (ev == GLFW_PRESS) io.KeysDown[key] = true;
        if (ev == GLFW_RELEASE) io.KeysDown[key] = false;

        io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
        io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
        io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
        io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];

        u32 nmod = 0; // normalized mod
        if (mod & GLFW_MOD_SUPER)
            nmod |= OUR_MOD_CMD;
        if (mod & GLFW_MOD_CONTROL)
            nmod |= OUR_MOD_CTRL;
        if (mod & GLFW_MOD_SHIFT)
            nmod |= OUR_MOD_SHIFT;
        if (mod & GLFW_MOD_ALT)
            nmod |= OUR_MOD_ALT;

        if (world.wnd_open_file.show && world.wnd_open_file.focused) {
            auto &wnd = world.wnd_open_file;
            switch (ev) {
            case GLFW_PRESS:
            case GLFW_REPEAT:
                switch (key) {
                case GLFW_KEY_DOWN:
                    wnd.selection++;
                    wnd.selection %= wnd.filtered_results->len;
                    break;
                case GLFW_KEY_UP:
                    if (wnd.selection == 0)
                        wnd.selection = wnd.filtered_results->len - 1;
                    else
                        wnd.selection--;
                    break;
                case GLFW_KEY_BACKSPACE:
                    {
                        auto &wnd = world.wnd_open_file;
                        if (wnd.query[0] != '\0')
                            wnd.query[strlen(wnd.query) - 1] = '\0';
                        if (strlen(wnd.query) >= 3)
                            filter_files();
                    }
                    break;
                case GLFW_KEY_ESCAPE:
                    world.wnd_open_file.show = false;
                    break;
                case GLFW_KEY_ENTER:
                    world.wnd_open_file.show = false;

                    if (wnd.filtered_results->len == 0) break;

                    auto relpath = wnd.filepaths->at(wnd.filtered_results->at(wnd.selection));
                    auto filepath = path_join(world.current_path, relpath);
                    world.focus_editor(filepath);
                    break;
                }
                break;
            }
            return;
        }

        auto editor = world.get_current_editor();

        auto handle_escape = [&]() -> bool {
            return editor->trigger_escape();
        };

        auto handle_enter = [&](ccstr nvim_string) {
            if (editor == NULL) return;
            if (world.nvim.mode != VI_INSERT) {
                send_nvim_keys(nvim_string);
                return;
            }

            editor->start_change();

            editor->type_char('\n');

            auto cur = editor->cur;
            auto y = max(0, cur.y-1);
            while (true) {
                auto& line = editor->buf.lines[y];
                for (u32 x = 0; x < line.len; x++)
                    if (!isspace(line[x]))
                        goto done;
                if (y == 0) break;
                y--;
            }
        done:

            auto& line = editor->buf.lines[y];
            u32 copy_spaces_until = 0;
            {
                u32 x = 0;
                for (; x < line.len; x++)
                    if (!isspace(line[x]))
                        break;
                if (x == line.len)  // all spaces
                    x = 0;
                copy_spaces_until = x;
            }

            // copy first `copy_spaces_until` chars of line y
            for (u32 x = 0; x < copy_spaces_until; x++)
                editor->type_char(line[x]);

            if (editor->is_go_file) {
                for (i32 x = line.len-1; x >= 0; x--) {
                    if (!isspace(line[x])) {
                        switch (line[x]) {
                        case '{':
                        case '(':
                        case '[':
                            editor->type_char('\t');
                            break;
                        }
                        break;
                    }
                }
            }

            editor->end_change();
        };

        auto handle_tab = [&](ccstr nvim_string) {
            if (editor == NULL) return;
            if (world.nvim.mode != VI_INSERT) {
                send_nvim_keys(nvim_string);
                return;
            }
            editor->type_char_in_insert_mode('\t');
        };

        auto handle_backspace = [&](ccstr nvim_string) {
            if (editor == NULL) return;
            if (world.nvim.mode != VI_INSERT) {
                send_nvim_keys(nvim_string);
                return;
            }

            editor->start_change();

            auto buf = &editor->buf;
            auto back1 = buf->dec_cur(editor->cur);
            buf->remove(back1, editor->cur);

            editor->raw_move_cursor(back1);

            if (editor->cur < editor->nvim_insert.backspaced_to)
                editor->nvim_insert.backspaced_to = editor->cur;

            editor->end_change();

            editor->update_autocomplete();
            editor->update_parameter_hint();
        };

        switch (ev) {
        case GLFW_PRESS:
        case GLFW_REPEAT:
            switch (nmod) {
            case OUR_MOD_SHIFT:
                if (world.use_nvim) {
                    switch (key) {
                    case GLFW_KEY_ENTER: handle_enter("<S-Enter>"); break;
                    case GLFW_KEY_TAB: handle_tab("<S-Tab>"); break;
                    case GLFW_KEY_BACKSPACE: handle_backspace("<S-Backspace>"); break;
                    case GLFW_KEY_ESCAPE: if (!handle_escape()) send_nvim_keys("<S-Esc>"); break;
                    case GLFW_KEY_F5:
                        if (world.dbg.state_flag == DLV_STATE_INACTIVE) break;
                        world.dbg.push_call(DLVC_STOP);
                        break;
                    case GLFW_KEY_F11:
                        if (world.dbg.state_flag != DLV_STATE_PAUSED) break;
                        world.dbg.push_call(DLVC_STEP_OUT);
                        break;
                    case GLFW_KEY_F10:
                        // TODO
                        break;
                    case GLFW_KEY_F9:
                        prompt_delete_all_breakpoints();
                    }
                    break;
                }
                break;

            case OUR_MOD_ALT | OUR_MOD_SHIFT:
                switch (key) {
                case GLFW_KEY_F:
                    editor->format_on_save();
                    break;
                }
                break;

            case OUR_MOD_ALT:
                switch (key) {
                case GLFW_KEY_LEFT_BRACKET:
                case GLFW_KEY_RIGHT_BRACKET:
                    {
                        auto &b = world.build;
                        if (!b.ready() || b.errors.len == 0) break;

                        auto old = b.current_error;
                        do {
                            b.current_error += (key == GLFW_KEY_LEFT_BRACKET ? -1 : 1);
                            if (b.current_error < 0)
                                b.current_error = b.errors.len - 1;
                            if (b.current_error >= b.errors.len)
                                b.current_error = 0;
                        } while (b.current_error != old && !b.errors[b.current_error].valid);

                        go_to_error(b.current_error);
                    }
                    break;
                }
                break;

            case OUR_MOD_CTRL:
                {
                    bool handled = false;

                    if (world.use_nvim) {
                        handled = true;
                        switch (key) {
                        case GLFW_KEY_ENTER: handle_enter("<C-Enter>"); break;
                        case GLFW_KEY_BACKSPACE: handle_backspace("<C-Backspace>"); break;
                        case GLFW_KEY_ESCAPE: if (!handle_escape()) send_nvim_keys("<C-Esc>"); break;
                        default: handled = false; break;
                        }
                    }

                    if (handled) break;

                    switch (key) {
                    case GLFW_KEY_1:
                        world.activate_pane(0);
                        break;
                    case GLFW_KEY_2:
                        world.activate_pane(1);
                        break;
                    case GLFW_KEY_3:
                        world.activate_pane(2);
                        break;
                    case GLFW_KEY_4:
                        world.activate_pane(3);
                        break;
                    case GLFW_KEY_R:
                    case GLFW_KEY_O:
                    case GLFW_KEY_I:
                        if (world.nvim.mode != VI_INSERT) {
                            SCOPED_FRAME();
                            send_nvim_keys(our_sprintf("<C-%c>", tolower((char)key)));
                        }
                        break;
                    case GLFW_KEY_Y:
                        if (editor == NULL) break;
                        if (editor->view.y > 0) {
                            editor->view.y--;
                            if (editor->cur.y + settings.scrolloff >= editor->view.y + editor->view.h)
                                editor->move_cursor(new_cur2(editor->cur.x, editor->view.y + editor->view.h - 1 - settings.scrolloff));
                        }
                        break;
                    case GLFW_KEY_E:
                        if (editor == NULL) break;
                        if (world.nvim.mode == VI_INSERT) break;
                        if (relu_sub(editor->cur.y, settings.scrolloff) < editor->view.y + 1) {
                            if (editor->view.y + 1 < editor->buf.lines.len) {
                                editor->view.y++;
                                editor->move_cursor(new_cur2(editor->cur.x, editor->view.y + settings.scrolloff));
                            }
                        } else {
                            editor->view.y++;
                        }
                        break;
                    case GLFW_KEY_V:
                        if (world.nvim.mode == VI_INSERT) {
                            auto clipboard_contents = glfwGetClipboardString(world.window);
                            if (clipboard_contents == NULL)
                                break;

                            auto len = strlen(clipboard_contents);
                            if (len == 0) break;

                            SCOPED_FRAME();

                            auto text = alloc_array(uchar, len);
                            for (u32 i = 0; i < len; i++)
                                text[i] = clipboard_contents[i];

                            editor->start_change();
                            {
                                editor->buf.insert(editor->cur, text, len);
                                auto cur = editor->cur;
                                for (u32 i = 0; i < len; i++)
                                    cur = editor->buf.inc_cur(cur);
                                editor->raw_move_cursor(cur);
                            }
                            editor->end_change();
                        } else {
                            send_nvim_keys("<C-v>");
                        }
                        break;
                    case GLFW_KEY_P:
                        world.wnd_open_file.show ^= 1;
                        if (world.wnd_open_file.show)
                            init_open_file();
                        break;
                    case GLFW_KEY_S:
                        if (editor != NULL)
                            editor->handle_save();
                        break;
                    case GLFW_KEY_G:
                        {
                            if (editor == NULL) break;

                            SCOPED_MEM(&world.indexer.ui_mem);
                            defer { world.indexer.ui_mem.reset(); };

                            Jump_To_Definition_Result *result = NULL;

                            {
                                if (!world.indexer.ready) return; // strictly we can just call try_enter(), but want consistency with UI, which is based on `ready`
                                if (!world.indexer.lock.try_enter()) return;
                                defer { world.indexer.lock.leave(); };

                                result = world.indexer.jump_to_definition(editor->filepath, new_cur2(editor->cur_to_offset(editor->cur), -1));
                                if (result == NULL) {
                                    error("unable to jump to definition");
                                    return;
                                }
                            }

                            auto target = editor;
                            if (!streq(editor->filepath, result->file))
                                target = world.focus_editor(result->file);

                            if (target == NULL) break;

                            auto pos = result->pos;
                            if (world.use_nvim) {
                                if (target->is_nvim_ready()) {
                                    if (pos.y == -1) pos = target->offset_to_cur(pos.x);
                                    target->move_cursor(pos);
                                } else {
                                    target->nvim_data.initial_pos = pos;
                                    target->nvim_data.need_initial_pos_set = true;
                                }
                            } else {
                                if (pos.y == -1) pos = target->offset_to_cur(pos.x);
                                target->move_cursor(pos);
                            }
                            break;
                        }
                    case GLFW_KEY_SLASH:
                        {
                            auto ed = world.get_current_editor();
                            if (ed == NULL) break;
                            ed->trigger_parameter_hint(false);
                        }
                        break;
                    case GLFW_KEY_SPACE:
                        {
                            auto ed = world.get_current_editor();
                            if (ed == NULL) break;
                            ed->trigger_autocomplete(false);
                        }
                        break;
                    case GLFW_KEY_J:
                    case GLFW_KEY_K:
                        {
                            auto ed = world.get_current_editor();
                            if (ed == NULL) return;

                            auto &ac = ed->autocomplete;

                            if (ac.ac.results != NULL) {
                                int delta = (key == GLFW_KEY_J ? 1 : -1);
                                if (ac.selection == 0 && delta == -1)
                                    ac.selection = ac.filtered_results->len - 1;
                                else
                                    ac.selection = (ac.selection + delta) % ac.filtered_results->len;

                                if (ac.selection >= ac.view + AUTOCOMPLETE_WINDOW_ITEMS)
                                    ac.view = ac.selection - AUTOCOMPLETE_WINDOW_ITEMS + 1;
                                if (ac.selection < ac.view)
                                    ac.view = ac.selection;
                            }

                            break;
                        }
                    case GLFW_KEY_W:
                        {
                            auto pane = world.get_current_pane();
                            if (pane == NULL) break;

                            auto editor = pane->get_current_editor();
                            if (editor == NULL) {
                                // can't close the last pane
                                if (world.panes.len <= 1) break;

                                pane->cleanup();
                                world.panes.remove(world.current_pane);
                                if (world.current_pane >= world.panes.len)
                                    world.activate_pane(world.panes.len - 1);
                            } else {
                                if (editor->buf.dirty) {
                                    auto result = ask_user_yes_no_cancel(
                                        get_native_window_handle(world.window),
                                        our_sprintf("Do you want to save your changes to %s?", our_basename(editor->filepath)),
                                        "Your changes will be lost if you don't."
                                    );
                                    if (result == ASKUSER_CANCEL)
                                        break;
                                    else if (result == ASKUSER_YES)
                                        editor->handle_save(true);
                                }

                                editor->cleanup();

                                pane->editors.remove(pane->current_editor);
                                if (pane->editors.len == 0)
                                    pane->current_editor = -1;
                                else {
                                    auto new_idx = pane->current_editor;
                                    if (new_idx >= pane->editors.len)
                                        new_idx = pane->editors.len - 1;
                                    pane->focus_editor_by_index(new_idx);
                                }

                                world.nvim.start_request_message("nvim_input", 1);
                                world.nvim.writer.write_string("<Esc>");
                                world.nvim.end_message();
                            }
                        }
                        break;
                    case GLFW_KEY_N:
                        world.get_current_pane()->open_empty_editor();
                        break;
                    case GLFW_KEY_TAB:
                        {
                            auto pane = world.get_current_pane();
                            if (pane->editors.len == 0) break;

                            auto idx = (pane->current_editor + 1) % pane->editors.len;
                            pane->focus_editor_by_index(idx);
                        }

                        break;
                    }
                }
                break;

            case OUR_MOD_CTRL | OUR_MOD_SHIFT:
                {
                    bool handled = false;
                    if (world.use_nvim) {
                        handled = true;
                        switch (key) {
                        case GLFW_KEY_ENTER: handle_enter("<C-S-Enter>"); break;
                        case GLFW_KEY_ESCAPE: if (!handle_escape()) send_nvim_keys("<C-S-Esc>"); break;
                        case GLFW_KEY_BACKSPACE: handle_backspace("<C-S-Backspace>"); break;
                        default: handled = false; break;
                        }
                    }

                    if (handled) break;

                    switch (key) {
                    case GLFW_KEY_B:
                        kick_off_build();
                        break;
                    case GLFW_KEY_F:
                        world.windows_open.search_and_replace ^= 1;
                        break;
                    case GLFW_KEY_E:
                        world.file_explorer.show ^= 1;
                        break;
                    case GLFW_KEY_TAB:
                        {
                            auto pane = world.get_current_pane();
                            if (pane->editors.len == 0) break;

                            u32 idx;
                            if (pane->current_editor == 0)
                                idx = pane->editors.len - 1;
                            else
                                idx = pane->current_editor - 1;
                            pane->focus_editor_by_index(idx);
                        }
                        break;
                    }
                    break;
                }
            case OUR_MOD_NONE:
                {
                    bool done = true;

                    // first handle global keys

                    switch (key) {
                    case GLFW_KEY_F12:
                        world.windows_open.im_demo ^= 1;
                        break;
                    case GLFW_KEY_F5:
                        switch (world.dbg.state_flag) {
                        case DLV_STATE_PAUSED:
                            world.dbg.push_call(DLVC_CONTINUE_RUNNING);
                            break;

                        case DLV_STATE_INACTIVE:
                            world.dbg.push_call(DLVC_START);
                            break;
                        }
                        break;
                    case GLFW_KEY_F9:
                        if (editor == NULL) break;
                        world.dbg.push_call(DLVC_TOGGLE_BREAKPOINT, [&](auto call) {
                            call->toggle_breakpoint.filename = our_strcpy(editor->filepath);
                            call->toggle_breakpoint.lineno = editor->cur.y + 1;
                        });
                        break;
                    case GLFW_KEY_F10:
                        if (world.dbg.state_flag != DLV_STATE_PAUSED) break;
                        world.dbg.push_call(DLVC_STEP_OVER);
                        break;
                    case GLFW_KEY_F11:
                        if (world.dbg.state_flag != DLV_STATE_PAUSED) break;
                        world.dbg.push_call(DLVC_STEP_INTO);
                        break;
                    default:
                        done = false;
                        break;
                    }

                    if (done) break;
                    if (world.ui.keyboard_captured_by_imgui) break;
                    if (editor == NULL) break;

                    auto &buf = editor->buf;
                    auto cur = editor->cur;

                    switch (key) {
                    case GLFW_KEY_BACKSPACE: handle_backspace("<Backspace>"); break;

                    case GLFW_KEY_TAB:
                    case GLFW_KEY_ENTER:
                        {
                            auto& ac = editor->autocomplete;
                            if (ac.ac.results != NULL && ac.filtered_results->len > 0) {
                                auto idx = ac.filtered_results->at(ac.selection);
                                auto& result = ac.ac.results->at(idx);

                                // grab len & save name
                                auto len = strlen(result.name);
                                auto name = alloc_array(uchar, len);
                                for (u32 i = 0; i < len; i++)
                                    name[i] = (uchar)result.name[i];

                                // figure out where insertion starts and ends
                                auto ac_start = editor->cur;
                                ac_start.x -= strlen(ac.ac.prefix);
                                auto ac_end = ac_start;
                                ac_end.x += len;

                                // perform the edit
                                buf.remove(ac_start, editor->cur);
                                buf.insert(ac_start, name, len);

                                // tell tree-sitter about the edit
                                if (editor->is_go_file) {
                                    TSInputEdit tsedit = {0};
                                    tsedit.start_byte = editor->cur_to_offset(ac_start);
                                    tsedit.start_point = cur_to_tspoint(ac_start);
                                    tsedit.old_end_byte = editor->cur_to_offset(editor->cur);
                                    tsedit.old_end_point = cur_to_tspoint(editor->cur);
                                    tsedit.new_end_byte = editor->cur_to_offset(ac_end);
                                    tsedit.new_end_point = cur_to_tspoint(ac_end);
                                    ts_tree_edit(editor->tree, &tsedit);
                                    editor->update_tree();
                                }

                                // move cursor forward
                                editor->raw_move_cursor(new_cur2(ac_start.x + len, ac_start.y));

                                // clear autocomplete
                                ptr0(&ac.ac);

                                // update buffer
                                if (world.nvim.mode != VI_INSERT) {
                                    auto& nv = world.nvim;
                                    auto msgid = nv.start_request_message("nvim_buf_set_lines", 5);
                                    {
                                        auto req = nv.save_request(NVIM_REQ_AUTOCOMPLETE_SETBUF, msgid, editor->id);
                                        req->autocomplete_setbuf.target_cursor = ac_end;
                                    }
                                    nv.writer.write_int(editor->nvim_data.buf_id); // buffer
                                    nv.writer.write_int(ac_end.y); // start
                                    nv.writer.write_int(ac_end.y + 1); // end
                                    nv.writer.write_bool(false); // strict_indexing
                                    {
                                        nv.writer.write_array(1); // replacement
                                        auto& line = buf.lines[ac_start.y];
                                        {
                                            // write a single string.
                                            nv.writer.write1(MP_OP_STRING);
                                            nv.writer.write4(line.len);
                                            for (u32 i = 0; i < line.len; i++)
                                                nv.writer.write1((char)line[i]);
                                        }
                                    }
                                    nv.end_message();
                                }
                                break;
                            } else {
                                if (key == GLFW_KEY_TAB)
                                    handle_tab("<Tab>");
                                else
                                    handle_enter("<Enter>");
                            }
                            break;
                        }
                    case GLFW_KEY_ESCAPE:
                        if (!handle_escape()) send_nvim_keys("<Esc>");
                        break;
                    }
                    break;
                }
            }
        }
    });

    glfwSetCharCallback(world.window, [](GLFWwindow* wnd, u32 ch) {
        auto pressed = [&](int key1, int key2) {
            return glfwGetKey(wnd, key1) || glfwGetKey(wnd, key2);
        };

        u32 nmod = 0; // normalized mod
        if (pressed(GLFW_KEY_LEFT_SUPER, GLFW_KEY_RIGHT_SUPER)) nmod |= OUR_MOD_CMD;
        if (pressed(GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_SUPER)) nmod |= OUR_MOD_CTRL;
        if (pressed(GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT)) nmod |= OUR_MOD_SHIFT;
        if (pressed(GLFW_KEY_LEFT_ALT, GLFW_KEY_RIGHT_ALT)) nmod |= OUR_MOD_ALT;

        if (nmod == OUR_MOD_CTRL) return;

        ImGuiIO& io = ImGui::GetIO();
        if (ch > 0 && ch < 0x10000)
            io.AddInputCharacter((u16)ch);

        if (world.ui.keyboard_captured_by_imgui) return;

        auto ed = world.get_current_editor();
        if (ed == NULL) return;

        if (isprint(ch)) {
            if (world.nvim.mode == VI_INSERT) {
                if (world.nvim.exiting_insert_mode) {
                    world.nvim.chars_after_exiting_insert_mode.append(ch);
                } else {
                    ed->type_char_in_insert_mode(ch);
                }
            } else {
                if (ch == '<') {
                    send_nvim_keys("<LT>");
                } else {
                    char keys[2] = { (char)ch, '\0' };
                    send_nvim_keys(keys);
                }
            }
        }
    });

    t.log("set random callbacks");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glGenVertexArrays(1, &world.ui.vao);
    glBindVertexArray(world.ui.vao);
    glGenBuffers(1, &world.ui.vbo);

    glGenVertexArrays(1, &world.ui.im_vao);
    glBindVertexArray(world.ui.im_vao);
    glGenBuffers(1, &world.ui.im_vbo);
    glGenBuffers(1, &world.ui.im_vebo);

    glGenFramebuffers(1, &world.ui.fbo);

    {
        // set opengl program attributes
        glUseProgram(world.ui.program);
        glBindVertexArray(world.ui.vao);
        glBindBuffer(GL_ARRAY_BUFFER, world.ui.vbo);

        i32 loc;

        loc = glGetAttribLocation(world.ui.program, "pos");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)_offsetof(Vert, x));

        loc = glGetAttribLocation(world.ui.program, "uv");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)_offsetof(Vert, u));

        loc = glGetAttribLocation(world.ui.program, "color");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)_offsetof(Vert, color));

        loc = glGetAttribLocation(world.ui.program, "mode");
        glEnableVertexAttribArray(loc);
        glVertexAttribIPointer(loc, 1, GL_INT, sizeof(Vert), (void*)_offsetof(Vert, mode));

        loc = glGetAttribLocation(world.ui.program, "texture_id");
        glEnableVertexAttribArray(loc);
        glVertexAttribIPointer(loc, 1, GL_INT, sizeof(Vert), (void*)_offsetof(Vert, texture_id));

        loc = glGetUniformLocation(world.ui.program, "projection");
        mat4f ortho_projection;
        new_ortho_matrix(ortho_projection, 0, world.display_size.x, world.display_size.y, 0);
        glUniformMatrix4fv(loc, 1, GL_FALSE, (float*)ortho_projection);

        for (u32 i = 0; i < __TEXTURE_COUNT__; i++) {
            char key[] = {'t', 'e', 'x', '0' + i, '\0'};
            loc = glGetUniformLocation(world.ui.program, key);
            glUniform1i(loc, i);
        }
    }

    {
        // set imgui program attributes
        glUseProgram(world.ui.im_program);
        glBindVertexArray(world.ui.im_vao);
        glBindBuffer(GL_ARRAY_BUFFER, world.ui.im_vbo);

        i32 loc;

        loc = glGetAttribLocation(world.ui.im_program, "pos");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));

        loc = glGetAttribLocation(world.ui.im_program, "uv");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));

        loc = glGetAttribLocation(world.ui.im_program, "color");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));

        loc = glGetUniformLocation(world.ui.im_program, "projection");
        mat4f ortho_projection;
        new_ortho_matrix(ortho_projection, 0, world.window_size.x, world.window_size.y, 0);
        glUniformMatrix4fv(loc, 1, GL_FALSE, (float*)ortho_projection);

        loc = glGetUniformLocation(world.ui.im_program, "tex");
        glUniform1i(loc, TEXTURE_FONT_IMGUI);
    }

    t.log("opengl shit");

    // Wait until all the OpenGL crap is initialized. I don't know why, but
    // creating background threads that run while glfwCreateWindow() is called
    // results in intermittent crashes. No fucking idea why. I hate
    // programming.
    world.start_background_threads();

    t.log("start background threads");

    double last_time = glfwGetTime();
    i64 last_frame_time = current_time_in_nanoseconds();

    // world.focus_editor(path_join(world.current_path, "main.go"), new_cur2(1, 8));

    while (!glfwWindowShouldClose(world.window)) {
        world.frame_mem.reset();

        SCOPED_MEM(&world.frame_mem);

        {
            // Process messages in nvim queue.
            SCOPED_LOCK(&world.message_queue_lock);

            For (world.message_queue) {
                switch (it.type) {
                case MTM_NVIM_MESSAGE:
                    {
                        auto &nv = world.nvim;
                        nv.handle_message_from_main_thread(&it.nvim_message);
                    }
                    break;
                case MTM_RELOAD_EDITOR:
                    {
                        auto editor = world.find_editor_by_id(it.reload_editor_id);
                        if (editor == NULL) break;
                        editor->reload_file(true);
                    }
                    break;
                case MTM_GOTO_FILEPOS:
                    {
                        auto &args = it.goto_filepos;
                        world.focus_editor(args.file, args.pos);
                    }
                    break;
                }
            }

            world.message_queue_mem.reset();
            world.message_queue.len = 0;
        }

        {
            // Check jobs.

            if (world.jobs.flag_search) {
                auto& job = world.jobs.search;
                auto& proc = job.proc;
                auto& search_results = world.search_results;

                switch (proc.status()) {
                case PROCESS_ERROR:
                    world.jobs.flag_search = false;
                    search_results.cleanup();
                    proc.cleanup();
                    break;
                case PROCESS_DONE:
                    world.jobs.flag_search = false;
                    search_results.cleanup();
                    search_results.init();

                    /*
                    General_Parser parser;
                    parser.init(&proc);
                    {
                        SCOPED_MEM(&search_results.pool);
                        parser.parse_find_results();
                    }
                    */

                    world.search_results.show = true;
                    proc.cleanup();
                    break;
                }
            }

            if (world.jobs.flag_search_and_replace) {
                auto& job = world.jobs.search_and_replace;
                auto& proc = job.proc;

                switch (proc.status()) {
                case PROCESS_DONE:
                    job.signal_done = true;
                    // fallthrough
                case PROCESS_ERROR:
                    world.jobs.flag_search_and_replace = false;
                    proc.cleanup();
                    break;
                }
            }
        }

        glDisable(GL_SCISSOR_TEST);
        glClearColor(COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        {
            // Send info to UI and ImGui.

            // TODO: Send info to UI.

            // TODO: do we need to do this every frame?
            io.DisplaySize = ImVec2((float)world.window_size.x, (float)world.window_size.y);
            io.DisplayFramebufferScale = ImVec2(world.display_scale.x, world.display_scale.y);

            double new_time = glfwGetTime();
            io.DeltaTime = last_time > 0.0 ? (float)(new_time - last_time) : (float)(1.0f / 60.0f);
            last_time = new_time;

            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
            if (glfwGetWindowAttrib(world.window, GLFW_FOCUSED)) {
                if (io.WantSetMousePos) {
                    glfwSetCursorPos(world.window, (double)io.MousePos.x, (double)io.MousePos.y);
                } else {
                    double x, y;
                    glfwGetCursorPos(world.window, &x, &y);
                    io.MousePos = ImVec2((float)x, (float)y);
                }
            }

            for (i32 i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++) {
                bool down = world.ui.mouse_just_pressed[i] || glfwGetMouseButton(world.window, i) != 0;
                io.MouseDown[i] = down;
                world.ui.mouse_down[i] = down && !world.ui.mouse_captured_by_imgui;
                world.ui.mouse_just_pressed[i] = false;
            }

            bool cur_changed = ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0);
            bool cur_enabled = (glfwGetInputMode(world.window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED);

            if (cur_changed && cur_enabled) {
                ImGuiMouseCursor cur = ImGui::GetMouseCursor();

                auto lookup_cursor = [&](ImGuiMouseCursor cur) {
                    auto ret = world.ui.cursors[cur];
                    if (ret == NULL)
                        ret = world.ui.cursors[ImGuiMouseCursor_Arrow];
                    return ret;
                };

                if (io.MouseDrawCursor || cur == ImGuiMouseCursor_None) {
                    glfwSetInputMode(world.window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
                } else if (cur != ImGuiMouseCursor_Arrow) {
                    glfwSetInputMode(world.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    glfwSetCursor(world.window, lookup_cursor(cur));
                } else if (ui.hover.ready && ui.hover.cursor != ImGuiMouseCursor_Arrow) {
                    glfwSetCursor(world.window, lookup_cursor(ui.hover.cursor));
                } else {
                    glfwSetCursor(world.window, lookup_cursor(ImGuiMouseCursor_Arrow));
                }
            }
        }

        ui.draw_everything();
        glfwPollEvents();
        ui.end_frame(); // end frame after polling events, so our event callbacks have access to imgui

        glfwSwapBuffers(world.window);

        {
            // wait until next frame
            auto curr = current_time_in_nanoseconds();
            auto remaining = (1000000000.f / FRAME_RATE_CAP) - (curr - last_frame_time);
            if (remaining > 0)
                sleep_milliseconds((u32)(remaining / 1000000.0f));
            last_frame_time = curr;
        }
    }

    return EXIT_SUCCESS;
}
