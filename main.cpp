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
#include "fzy_match.h"
#include "settings.hpp"
#include "IconsFontAwesome5.h"
#include "IconsMaterialDesign.h"

#include "imgui.h"
#include "fonts.hpp"

#if OS_WIN
#define GLFW_EXPOSE_NATIVE_WIN32
#elif OS_MAC
#define GLFW_EXPOSE_NATIVE_COCOA
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define MAX_PATH 260
#define CODE_FONT_SIZE 14
#define UI_FONT_SIZE 16
#define ICON_FONT_SIZE 15
#define FRAME_RATE_CAP 60

static const char WINDOW_TITLE[] = "CodePerfect 95";

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

void goto_next_tab() {
    auto pane = world.get_current_pane();
    if (pane->editors.len == 0) return;

    auto idx = (pane->current_editor + 1) % pane->editors.len;
    pane->focus_editor_by_index(idx);
}

void goto_previous_tab() {
    auto pane = world.get_current_pane();
    if (pane->editors.len == 0) return;

    u32 idx;
    if (pane->current_editor == 0)
        idx = pane->editors.len - 1;
    else
        idx = pane->current_editor - 1;
    pane->focus_editor_by_index(idx);
}

bool is_git_folder(ccstr path) {
    SCOPED_FRAME();
    auto pathlist = make_path(path);
    return pathlist->parts->find([&](auto it) { return streqi(*it, ".git"); }) != NULL;
}

int main(int argc, char **argv) {
    gargc = argc;
    gargv = argv;

    is_main_thread = true;

    Timer t;
    t.init();

    init_platform_specific_crap();

    if (!glfwInit())
        return error("glfwInit failed"), EXIT_FAILURE;
    defer { glfwTerminate(); };

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    // glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto window = glfwCreateWindow(1280, 720, WINDOW_TITLE, NULL, NULL);
    if (window == NULL)
        return error("could not create window"), EXIT_FAILURE;

    // on macos, this requires glfw crap to be done
    world.init(window);
    SCOPED_MEM(&world.frame_mem);

#ifdef DEBUG_MODE
    GHEnableDebugMode();
#endif

    GHAuthAndUpdate(); // kicks off auth/autoupdate shit in the background

    world.window = window;
    glfwSetWindowTitle(world.window, our_sprintf("%s - %s", WINDOW_TITLE, world.current_path));

    glfwMakeContextCurrent(world.window);

    {
        glewExperimental = GL_TRUE;
        auto err = glewInit();
        if (err != GLEW_OK)
            return error("unable to init GLEW: %s", glewGetErrorString(err)), EXIT_FAILURE;
    }

    glfwSwapInterval(0);

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // ImGui::StyleColorsLight();

    auto &style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(7, 7);
    style.FramePadding = ImVec2(7, 2);
    style.CellPadding = ImVec2(4, 2);
    style.ItemSpacing = ImVec2(7, 3);
    style.ItemInnerSpacing = ImVec2(3, 3);
    style.TouchExtraPadding = ImVec2(0, 0);
    style.IndentSpacing = 21;
    style.ScrollbarSize = 11;
    style.GrabMinSize = 8;

    style.WindowRounding = 3;
    style.ChildRounding = 0;
    style.FrameRounding = 2;
    style.PopupRounding = 0;
    style.ScrollbarRounding = 2;
    style.GrabRounding = 2;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 2;

    style.Colors[ImGuiCol_WindowBg] = ImColor(23, 23, 23);
    style.Colors[ImGuiCol_Text] = ImColor(227, 227, 227);

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

    // grab window_size, display_size, and display_scale
    glfwGetWindowSize(world.window, (i32*)&world.window_size.x, (i32*)&world.window_size.y);
    glfwGetFramebufferSize(world.window, (i32*)&world.display_size.x, (i32*)&world.display_size.y);
    glfwGetWindowContentScale(world.window, &world.display_scale.x, &world.display_scale.y);

    // now that we have world.display_size, we can call wksp.activate_pane
    world.activate_pane(0);

    // initialize & bind textures
    glGenTextures(__TEXTURE_COUNT__, world.ui.textures);
    for (u32 i = 0; i < __TEXTURE_COUNT__; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, world.ui.textures[i]);
    }
    // from here, if we want to modify a texture, just set the active texture
    // are we going to run out of texture units?

    {
        SCOPED_FRAME();

        s32 len = 0;

        world.ui.im_font_ui = io.Fonts->AddFontFromMemoryTTF(open_sans_ttf, open_sans_ttf_len, UI_FONT_SIZE);
        our_assert(world.ui.im_font_ui != NULL, "unable to load UI font");

        {
            // merge font awesome into main font
            ImFontConfig config;
            config.MergeMode = true;
            config.GlyphMinAdvanceX = ICON_FONT_SIZE;
            config.GlyphOffset.y = 3;

            /*
            ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
            io.Fonts->AddFontFromMemoryTTF(fa_regular_400_ttf, fa_regular_400_ttf_len, ICON_FONT_SIZE, &config, icon_ranges);
            io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, ICON_FONT_SIZE, &config, icon_ranges);
            */

            ImWchar icon_ranges[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
            io.Fonts->AddFontFromMemoryTTF(material_icons_regular_ttf, material_icons_regular_ttf_len, ICON_FONT_SIZE, &config, icon_ranges);
        }

        world.ui.im_font_mono = io.Fonts->AddFontFromMemoryTTF(vera_mono_ttf, vera_mono_ttf_len, CODE_FONT_SIZE);
        our_assert(world.ui.im_font_mono != NULL, "unable to load code font");

        if (!world.font.init((u8*)vera_mono_ttf, CODE_FONT_SIZE, TEXTURE_FONT))
            our_panic("unable to load code font");

        io.Fonts->Build();

        // init imgui texture
        u8* pixels;
        i32 width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        glActiveTexture(GL_TEXTURE0 + TEXTURE_FONT_IMGUI);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    glfwSetWindowSizeCallback(world.window, [](GLFWwindow*, i32 w, i32 h) {
        world.window_size.x = w;
        world.window_size.y = h;

        auto& font = world.font;

        mat4f projection;
        new_ortho_matrix(projection, 0, w, h, 0);
        glUseProgram(world.ui.im_program);
        glUniformMatrix4fv(glGetUniformLocation(world.ui.im_program, "projection"), 1, GL_FALSE, (float*)projection);

        // clear frame
        auto bgcolor = COLOR_JBLOW_BG;
        glClearColor(bgcolor.r, bgcolor.g, bgcolor.b, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(world.window);
    });

    glfwSetFramebufferSizeCallback(world.window, [](GLFWwindow*, i32 w, i32 h) {
        world.display_size.x = w;
        world.display_size.y = h;

        mat4f projection;
        new_ortho_matrix(projection, 0, w, h, 0);
        glUseProgram(world.ui.program);
        glUniformMatrix4fv(glGetUniformLocation(world.ui.program, "projection"), 1, GL_FALSE, (float*)projection);

        // clear frame
        auto bgcolor = COLOR_JBLOW_BG;
        glClearColor(bgcolor.r, bgcolor.g, bgcolor.b, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(world.window);
    });

    glfwSetCursorPosCallback(world.window, [](GLFWwindow*, double x, double y) {
        // world.ui.mouse_delta.x = x - world.ui.mouse_pos.x;
        // world.ui.mouse_delta.y = y - world.ui.mouse_pos.y;
        world.ui.mouse_pos.x = x;
        world.ui.mouse_pos.y = y;

        if (world.resizing_pane != -1) {
            auto i = world.resizing_pane;

            float leftoff = ui.panes_area.x;
            for (int j = 0; j < i; j++)
                leftoff += world.panes[j].width;

            auto& pane1 = world.panes[i];
            auto& pane2 = world.panes[i+1];

            // leftoff += pane1.width;
            auto desired_width = relu_sub(x, leftoff);
            auto delta = desired_width - pane1.width;

            if (delta < (100 - pane1.width))
                delta = 100 - pane1.width;
            if (delta > pane2.width - 100)
                delta = pane2.width - 100;

            pane1.width += delta;
            pane2.width -= delta;
        }
    });

    glfwSetMouseButtonCallback(world.window, [](GLFWwindow*, int button, int action, int mods) {
        // Don't set world.ui.mouse_down here. We set it based on
        // world.ui.mouse_just_pressed and some additional logic below, while
        // we're setting io.MouseDown for ImGui.

        if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(world.ui.mouse_just_pressed))
            world.ui.mouse_just_pressed[button] = true;
    });

    glfwSetScrollCallback(world.window, [](GLFWwindow*, double dx, double dy) {
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

    glfwSetWindowContentScaleCallback(world.window, [](GLFWwindow*, float xscale, float yscale) {
        world.display_scale = { xscale, yscale };
    });

    glfwSetKeyCallback(world.window, [](GLFWwindow*, i32 key, i32 scan, i32 ev, i32 mod) {
        ImGuiIO& io = ImGui::GetIO();
        if (ev == GLFW_PRESS) io.KeysDown[key] = true;
        if (ev == GLFW_RELEASE) io.KeysDown[key] = false;

        io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
        io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
        io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
        io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];

        if (ev != GLFW_PRESS && ev != GLFW_REPEAT) return;

        // handle global keys

        switch (ui.imgui_get_keymods()) {
        case KEYMOD_SHIFT:
            if (world.use_nvim) {
                switch (key) {
                case GLFW_KEY_F5:
                    if (world.dbg.state_flag == DLV_STATE_INACTIVE) break;
                    world.dbg.push_call(DLVC_STOP);
                    break;
                case GLFW_KEY_F11:
                    if (world.dbg.state_flag != DLV_STATE_PAUSED) break;
                    world.dbg.push_call(DLVC_STEP_OUT);
                    break;
                case GLFW_KEY_F10:
                    // TODO: run to cursor
                    break;
                case GLFW_KEY_F9:
                    prompt_delete_all_breakpoints();
                }
                break;
            }
            break;

        case KEYMOD_ALT:
            switch (key) {
            case GLFW_KEY_LEFT_BRACKET:
            case GLFW_KEY_RIGHT_BRACKET:
                goto_next_error(key == GLFW_KEY_LEFT_BRACKET ? -1 : 1);
                break;
            }
            break;

        case KEYMOD_PRIMARY:
            switch (key) {
            case GLFW_KEY_1:
            case GLFW_KEY_2:
            case GLFW_KEY_3:
            case GLFW_KEY_4:
                world.activate_pane(key - GLFW_KEY_1);
                ImGui::SetWindowFocus(NULL);
                break;
            case GLFW_KEY_T:
                if (world.wnd_goto_symbol.show)
                    world.wnd_goto_symbol.show = false;
                else
                    init_goto_symbol();
                break;
            case GLFW_KEY_P:
                if (world.wnd_goto_file.show)
                    world.wnd_goto_file.show = false;
                else
                    init_goto_file();
                break;
            case GLFW_KEY_N:
                world.get_current_pane()->open_empty_editor();
                break;
            }
            break;

        case KEYMOD_PRIMARY | KEYMOD_SHIFT:
            switch (key) {
            case GLFW_KEY_B:
                world.error_list.show = true;
                world.error_list.cmd_focus = true;
                save_all_unsaved_files();
                kick_off_build();
                break;
            case GLFW_KEY_F:
                {
                    auto &wnd = world.wnd_search_and_replace;
                    if (wnd.show) {
                        ImGui::SetWindowFocus("###search_and_replace");
                        wnd.focus_textbox = 1;
                    }
                    wnd.show = true;
                    wnd.replace = false;
                }
                break;
            case GLFW_KEY_H:
                if (world.wnd_search_and_replace.show)
                    ImGui::SetWindowFocus("###search_and_replace");
                world.wnd_search_and_replace.show = true;
                world.wnd_search_and_replace.replace = true;
                break;
            case GLFW_KEY_E:
                {
                    auto &wnd = world.file_explorer;
                    if (wnd.show) {
                        if (!wnd.focused) {
                            ImGui::SetWindowFocus("File Explorer");
                        } else {
                            wnd.show = false;
                        }
                    } else {
                        wnd.show = true;
                    }
                }
                break;
            }
            break;

        case KEYMOD_NONE:
            switch (key) {
            case GLFW_KEY_F12:
                world.windows_open.im_demo ^= 1;
                break;
            case GLFW_KEY_F6:
                if (world.dbg.state_flag == DLV_STATE_INACTIVE)
                    world.dbg.push_call(DLVC_DEBUG_TEST_UNDER_CURSOR);
                break;
            case GLFW_KEY_F5:
                switch (world.dbg.state_flag) {
                case DLV_STATE_PAUSED:
                    world.dbg.push_call(DLVC_CONTINUE_RUNNING);
                    break;
                case DLV_STATE_INACTIVE:
                    save_all_unsaved_files();
                    world.dbg.push_call(DLVC_START);
                    break;
                }
                break;
            case GLFW_KEY_F9:
                {
                    auto editor = world.get_current_editor();
                    if (editor == NULL) break;
                    world.dbg.push_call(DLVC_TOGGLE_BREAKPOINT, [&](auto call) {
                        call->toggle_breakpoint.filename = our_strcpy(editor->filepath);
                        call->toggle_breakpoint.lineno = editor->cur.y + 1;
                    });
                }
                break;
            case GLFW_KEY_F10:
                if (world.dbg.state_flag != DLV_STATE_PAUSED) break;
                world.dbg.push_call(DLVC_STEP_OVER);
                break;
            case GLFW_KEY_F11:
                if (world.dbg.state_flag != DLV_STATE_PAUSED) break;
                world.dbg.push_call(DLVC_STEP_INTO);
                break;
            case GLFW_KEY_ESCAPE:
                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
                    ImGui::SetWindowFocus(NULL);
                break;
            }
            break;
        }

        if (world.ui.keyboard_captured_by_imgui) return;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) return;

        // handle non-global keys

        auto editor = world.get_current_editor();

        auto handle_enter = [&](ccstr nvim_string) {
            if (editor == NULL) return;
            if (world.nvim.mode != VI_INSERT) {
                send_nvim_keys(nvim_string);
                return;
            }

            editor->type_char_in_insert_mode('\n');

            auto indent_chars = editor->get_autoindent(editor->cur.y);
            editor->insert_text_in_insert_mode(indent_chars);
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

            if (world.nvim.exiting_insert_mode) {
                world.nvim.chars_after_exiting_insert_mode.append('\b');
            } else {
                // if we're at beginning of line
                if (editor->cur.x == 0) {
                    auto back1 = editor->buf.dec_cur(editor->cur);
                    editor->buf.remove(back1, editor->cur);
                    if (back1 < editor->nvim_insert.start) {
                        editor->nvim_insert.start = back1;
                        editor->nvim_insert.deleted_graphemes++;
                    }
                    editor->raw_move_cursor(back1);
                } else {
                    editor->backspace_in_insert_mode(1, 0); // erase one grapheme
                }

                editor->update_autocomplete(false);
                editor->update_parameter_hint();
            }
        };

        switch (ui.imgui_get_keymods()) {
        case KEYMOD_SHIFT:
            if (world.use_nvim) {
                switch (key) {
                case GLFW_KEY_ENTER: handle_enter("<S-Enter>"); break;
                case GLFW_KEY_TAB: handle_tab("<S-Tab>"); break;
                case GLFW_KEY_BACKSPACE: handle_backspace("<S-Backspace>"); break;
                case GLFW_KEY_ESCAPE: if (!editor->trigger_escape()) send_nvim_keys("<S-Esc>"); break;
                }
                break;
            }
            break;

        case KEYMOD_ALT | KEYMOD_SHIFT:
            switch (key) {
            case GLFW_KEY_O:
                if (editor->optimize_imports())
                    editor->format_on_save(GH_FMT_GOIMPORTS);
                else
                    editor->format_on_save(GH_FMT_GOIMPORTS_WITH_AUTOIMPORT);
                break;
            case GLFW_KEY_F:
                editor->format_on_save(GH_FMT_GOIMPORTS);
                break;
            }
            break;

        case KEYMOD_CTRL:
            {
                bool handled = false;

                if (world.use_nvim) {
                    handled = true;
                    switch (key) {
                    case GLFW_KEY_ENTER:
                        /*
                        if (world.nvim.mode == VI_INSERT && editor->postfix_stack.len > 0) {
                            auto pf = editor->postfix_stack.last();
                            our_assert(pf->current_insert_position < pf->insert_positions.len, "went past last error position");

                            auto pos = pf->insert_positions[pf->current_insert_position++];
                            editor->trigger_escape(pos);

                            if (pf->current_insert_position == pf->insert_positions.len)
                                editor->postfix_stack.len--;
                        } else {
                            handle_enter("<C-Enter>");
                        }
                        */
                        handle_enter("<C-Enter>");
                        break;
                    case GLFW_KEY_BACKSPACE: handle_backspace("<C-Backspace>"); break;
                    case GLFW_KEY_ESCAPE: if (!editor->trigger_escape()) send_nvim_keys("<C-Esc>"); break;
                    default: handled = false; break;
                    }
                }

                if (handled) break;

                switch (key) {
#if OS_WIN
                case GLFW_KEY_TAB:
                    goto_next_tab();
                    break;
#endif
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
                    if (world.nvim.mode == VI_INSERT) break;
                    if (editor->view.y > 0) {
                        editor->view.y--;
                        editor->ensure_cursor_on_screen();
                    }
                    break;
                case GLFW_KEY_E:
                    if (editor == NULL) break;
                    if (world.nvim.mode == VI_INSERT) break;
                    if (editor->view.y + 1 < editor->buf.lines.len) {
                        editor->view.y++;
                        editor->ensure_cursor_on_screen();
                    }
                    break;
                case GLFW_KEY_V:
                    if (world.nvim.mode != VI_INSERT)
                        send_nvim_keys("<C-v>");
                    break;
                case GLFW_KEY_G:
                    handle_goto_definition();
                    break;
                case GLFW_KEY_SLASH:
                    {
                        auto &nv = world.nvim;
                        nv.start_request_message("nvim_exec", 2);
                        nv.writer.write_string("nohlsearch");
                        nv.writer.write_bool(false);
                        nv.end_message();
                    }
                    break;
                case GLFW_KEY_SPACE:
                    {
                        auto ed = world.get_current_editor();
                        if (ed == NULL) break;
                        ed->trigger_autocomplete(false, false);
                    }
                    break;
                }
            }
            break;

        case KEYMOD_CMD | KEYMOD_SHIFT:
            switch (key) {
#if OS_MAC
            case GLFW_KEY_LEFT_BRACKET:
                goto_previous_tab();
                break;
            case GLFW_KEY_RIGHT_BRACKET:
                goto_next_tab();
                break;
#endif
            }
            break;

        case KEYMOD_CTRL | KEYMOD_SHIFT:
            {
                bool handled = false;
                if (world.use_nvim) {
                    handled = true;
                    switch (key) {
                    case GLFW_KEY_ENTER: handle_enter("<C-S-Enter>"); break;
                    case GLFW_KEY_ESCAPE: if (!editor->trigger_escape()) send_nvim_keys("<C-S-Esc>"); break;
                    case GLFW_KEY_BACKSPACE: handle_backspace("<C-S-Backspace>"); break;
                    default: handled = false; break;
                    }
                }

                if (handled) break;

                switch (key) {
#if OS_WIN
                case GLFW_KEY_TAB:
                    goto_previous_tab();
                    break;
#endif
                case GLFW_KEY_SPACE:
                    {
                        auto ed = world.get_current_editor();
                        if (ed == NULL) break;
                        ed->trigger_parameter_hint();
                    }
                    break;
                }
                break;
            }
        case KEYMOD_NONE:
            {
                if (editor == NULL) break;

                auto &buf = editor->buf;
                auto cur = editor->cur;

                switch (key) {
                case GLFW_KEY_LEFT: send_nvim_keys("<Left>"); break;
                case GLFW_KEY_RIGHT: send_nvim_keys("<Right>"); break;

                case GLFW_KEY_DOWN:
                case GLFW_KEY_UP:
                    if (!move_autocomplete_cursor(editor, key == GLFW_KEY_DOWN ? 1 : -1)) {
                        if (key == GLFW_KEY_DOWN)
                            send_nvim_keys("<Down>");
                        else
                            send_nvim_keys("<Up>");
                    }
                    break;

                case GLFW_KEY_BACKSPACE: handle_backspace("<Backspace>"); break;

                case GLFW_KEY_ENTER:
                    handle_enter("<Enter>");
                    break;

                case GLFW_KEY_TAB:
                    {
                        auto& ac = editor->autocomplete;
                        if (ac.ac.results == NULL || ac.filtered_results->len == 0) {
                            handle_tab("<Tab>");
                            break;
                        }

                        auto idx = ac.filtered_results->at(ac.selection);
                        auto& result = ac.ac.results->at(idx);
                        editor->perform_autocomplete(&result);
                    }
                    break;

                case GLFW_KEY_ESCAPE:
                    if (!editor->trigger_escape()) send_nvim_keys("<Esc>");
                    break;
                }
                break;
            }
        }

        // separate switch for KEYMOD_PRIMARY

        switch (ui.imgui_get_keymods()) {
        case KEYMOD_PRIMARY:
            switch (key) {
            case GLFW_KEY_V:
                if (world.nvim.mode == VI_INSERT) {
                    auto clipboard_contents = glfwGetClipboardString(world.window);
                    if (clipboard_contents == NULL) break;
                    editor->insert_text_in_insert_mode(clipboard_contents);
                }
                break;

            case GLFW_KEY_S:
                if (editor != NULL)
                    editor->handle_save();
                break;
            case GLFW_KEY_J:
            case GLFW_KEY_K:
                {
                    auto ed = world.get_current_editor();
                    if (ed == NULL) return;
                    move_autocomplete_cursor(ed, key == GLFW_KEY_J ? 1 : -1);
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
                            auto title = "Your changes will be lost if you don't.";
                            auto filename  = editor->is_untitled ? "(untitled)" : our_basename(editor->filepath);
                            auto msg = our_sprintf("Do you want to save your changes to %s?", filename);

                            auto result = ask_user_yes_no_cancel(title, msg, "Save", "Don't Save");
                            if (result == ASKUSER_CANCEL)
                                break;
                            if (result == ASKUSER_YES)
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

                        send_nvim_keys("<Esc>");
                    }
                }
                break;
            }
        }
    });

    glfwSetCharCallback(world.window, [](GLFWwindow* wnd, u32 ch) {
        auto pressed = [&](int key1, int key2) {
            return glfwGetKey(wnd, key1) || glfwGetKey(wnd, key2);
        };

        u32 nmod = 0; // normalized mod
        if (pressed(GLFW_KEY_LEFT_SUPER, GLFW_KEY_RIGHT_SUPER)) nmod |= KEYMOD_CMD;
        if (pressed(GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_SUPER)) nmod |= KEYMOD_CTRL;
        if (pressed(GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT)) nmod |= KEYMOD_SHIFT;
        if (pressed(GLFW_KEY_LEFT_ALT, GLFW_KEY_RIGHT_ALT)) nmod |= KEYMOD_ALT;

        if (nmod == KEYMOD_CTRL) return;

        ImGuiIO& io = ImGui::GetIO();
        if (ch > 0 && ch < 0x10000)
            io.AddInputCharacter((u16)ch);

        if (world.ui.keyboard_captured_by_imgui) return;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) return;

        auto ed = world.get_current_editor();
        if (ed == NULL) return;

        if (ch > 127) return;

        if (isprint(ch)) {
            if (world.nvim.mode == VI_INSERT) {
                if (world.nvim.exiting_insert_mode) {
                    world.nvim.chars_after_exiting_insert_mode.append(ch);
                } else {
                    ed->type_char_in_insert_mode(ch);
                }
            } else { // if (world.nvim.mode == VI_REPLACE || ch != ':') {
                if (ch == '<') {
                    send_nvim_keys("<LT>");
                } else {
                    char keys[2] = { (char)ch, '\0' };
                    send_nvim_keys(keys);
                }
            }
        }
    });

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
            char key[] = {'t', 'e', 'x', (char)('0' + i), '\0'};
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

    // Wait until all the OpenGL crap is initialized. I don't know why, but
    // creating background threads that run while glfwCreateWindow() is called
    // results in intermittent crashes. No fucking idea why. I hate
    // programming.
    world.start_background_threads();

    t.log("initialize everything");

    auto last_frame_time = current_time_in_nanoseconds();

    // world.focus_editor("main.go");
    // world.focus_editor(path_join(world.indexer.goroot, "time/time.go"));
    // world.focus_editor(path_join(world.indexer.goroot, "database/sql/sql.go"));
    // world.focus_editor(path_join(world.indexer.gomodcache, "github.com/davecgh/go-spew@v1.1.1/spew/dump.go"));

    while (!glfwWindowShouldClose(world.window)) {
        {
            GH_Message msg; ptr0(&msg);
            if (GHGetMessage(&msg)) {
                tell_user(msg.text, msg.title);
                if (msg.is_panic)
                    return EXIT_FAILURE;
            }
        }

        auto frame_start_time = current_time_in_nanoseconds();

        world.frame_mem.reset();

        SCOPED_MEM(&world.frame_mem);

        {
            // Process message queue.
            auto messages = world.message_queue.start();
            defer { world.message_queue.end(); };

            For (*messages) {
                switch (it.type) {
                case MTM_ADD_DEBUGGER_STDOUT_LINE:
                    // it.debugger_stdout_line;

                case MTM_PANIC:
                    our_panic(it.panic_message);
                    break;

                case MTM_NVIM_MESSAGE:
                    {
                        auto &nv = world.nvim;
                        nv.handle_message_from_main_thread(&it.nvim_message);
                    }
                    break;
                case MTM_GOTO_FILEPOS:
                    {
                        auto &args = it.goto_filepos;
                        goto_file_and_pos(args.file, args.pos);
                    }
                    break;
                }
            }
        }

        // Process filesystem changes.

        {
            Fs_Event event;
            for (u32 items_processed = 0; items_processed < 10 && world.fswatch.next_event(&event); items_processed++) {
                if (is_git_folder(event.filepath)) continue;
                if (event.filepath[0] == '\0') continue;

                auto filepath = (ccstr)event.filepath;
                auto res = check_path(path_join(world.current_path, filepath));
                if (res != CPR_DIRECTORY)
                    filepath = our_dirname(filepath);

                reload_file_subtree(filepath);

                if (streq(filepath, "."))
                    filepath = "";

                auto editor = world.find_editor_by_filepath(filepath);
                if (editor != NULL)
                    editor->reload_file(true);

                if (res == CPR_DIRECTORY || str_ends_with(event.filepath, ".go")) {
                    world.indexer.message_queue.add([&](auto msg) {
                        msg->type = GOMSG_FSEVENT;
                        msg->fsevent_filepath = our_strcpy(filepath);
                    });
                }
            }
        }

        glDisable(GL_SCISSOR_TEST);
        auto bgcolor = COLOR_JBLOW_BG;
        glClearColor(bgcolor.r, bgcolor.g, bgcolor.b, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        {
            // Send info to UI and ImGui.
            io.DisplaySize = ImVec2((float)world.window_size.x, (float)world.window_size.y);
            io.DisplayFramebufferScale = ImVec2(world.display_scale.x, world.display_scale.y);

            auto now = current_time_in_nanoseconds();
            io.DeltaTime = (double)(now - last_frame_time) / (double)1000000000;
            last_frame_time = now;

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

        glfwPollEvents();

        ui.draw_everything();
        ui.end_frame(); // end frame after polling events, so our event callbacks have access to imgui

        glfwSwapBuffers(world.window);

        if (!world.turn_off_framerate_cap) {
            // wait until next frame
            auto budget = (1000.f / FRAME_RATE_CAP);
            auto spent = (current_time_in_nanoseconds() - frame_start_time) / 1000000.f;
            if (budget > spent) sleep_milliseconds((u32)(budget - spent));
        }
    }

    return EXIT_SUCCESS;
}
