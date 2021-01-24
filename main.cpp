/*
next task to do
---------------
Test new implementation of crawl_location inside Golang::build_index(),
make sure it works, start writing decls, make sure that works, continue trying
to parse Go files faster and with less memory.

notes
-----
- in visual mode, highlight spaces too
- close parameter hint when ')' is typed
- autocomplete doesn't work when prefix is a keyword, e.g. "default" (for gin.Default())
- had a weird bug where when we typed "fmt.Fprintf(", the hint showed up, we typed "w", the screen froze and started spazzing out
- handle exclude directives
- indent is fucked up when creating an untitled buffer (but it works after we save, quit, re-run, reopen file)
- commands with long output freeze; clear pipe
- permanent settings system so we can save things like "don't show again"
- configuration/settings? i mean, i'm against "customization", but some settings are still needed
- limit scrolling past bottom in sidebar
- support scrolloff (start by modifying Editor::move_cursor)
- destroy editor's nvim resources when closing a tab (can we even close tab rn?)
- Neovim Unicode integration
- investigate open source licenses for all the crap we're using
- how do we handle string length limits in stacktrace?
- "can't connect to debugger" error path is currently not handled
- indicator for "dirty" buffer
- we have a lot of ambiguity around:
    1) how to sync data with delve
    2) where the source of truth should be stored
- (mac) running processes from different folder (e.g. for ctrl+p) only works in tmux???
- make nvim use system clipboard (or at least have as option) (and also just get clipboard working)

observations about visual studio
--------------------------------
- ctrl+p lags like a bitch, not instant at all
- ctrl-tab to shift between editors lags
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

#include <GL/glew.h>
#include <GLFW/glfw3.h>

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

#include "imgui.h"
#include "veramono.hpp"

#define MAX_PATH 260
#define FONT_SIZE 14
#define FRAME_RATE_CAP 60

static const char WINDOW_TITLE[] = "i need to think of a name";

char vert_shader[] = R"(
#version 410

in vec2 pos;
in vec2 uv;
in vec4 color;
in int solid;
out vec2 _uv;
out vec4 _color;
flat out int _solid;
uniform mat4 projection;

void main(void) {
    _uv = uv;
    _color = color;
    _solid = solid;

    gl_Position = projection * vec4(pos, 0, 1);
}
)";

char frag_shader[] = R"(
#version 410

in vec2 _uv;
in vec4 _color;
flat in int _solid;
out vec4 outcolor;
uniform sampler2D tex;

void main(void) {
    outcolor = vec4(_color.rgb, (_solid == 1 ? _color.a : texture(tex, _uv).r));
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

void render_ast(Ast* ast, ccstr label = "Root") {
    int last_depth = -1;

    struct {
        bool buf[1000];
        s32 p;
    } node_stack = { 0 };

    walk_ast(ast, [&](Ast* node, ccstr name, int depth) -> Walk_Action {
        if (depth <= last_depth) {
            auto pops = last_depth - depth + 1;
            for (u32 i = 0; i < pops; i++)
                if (node_stack.buf[--node_stack.p])
                    ImGui::TreePop();
        }

        last_depth = depth;

        bool open = ImGui::TreeNodeEx(node, 0, "%s: %s", name, ast_type_str(node->type));

        node_stack.buf[node_stack.p++] = open;
        if (!open) return WALK_SKIP_CHILDREN;

        switch (node->type) {
            case AST_INC_DEC_STMT:
                ImGui::Text("op: %s", tok_type_str(node->inc_dec_stmt.op));
                break;
            case AST_SWITCH_STMT:
                ImGui::Text("is_type_switch: %s", node->switch_stmt.is_type_switch ? "true" : "false");
                break;
            case AST_BASIC_LIT:
                ImGui::Text("lit: %s", node->basic_lit.lit);
                break;
            case AST_ID:
                ImGui::Text("lit: %s", node->id.lit);
                break;
            case AST_UNARY_EXPR:
                ImGui::Text("op: %s", tok_type_str(node->unary_expr.op));
                break;
            case AST_BINARY_EXPR:
                ImGui::Text("op: %s", tok_type_str(node->binary_expr.op));
                break;
            case AST_CHAN_TYPE:
                ImGui::Text("direction: %s", ast_chan_direction_str(node->chan_type.direction));
                break;
            case AST_CALL_ARGS:
                ImGui::Text("ellip: %s", node->call_args.ellip ? "true" : "false");
                break;
            case AST_BRANCH_STMT:
                ImGui::Text("branch_type: %s", tok_type_str(node->branch_stmt.branch_type));
                break;
            case AST_DECL:
                ImGui::Text("type: %s", tok_type_str(node->decl.type));
                break;
            case AST_ASSIGN_STMT:
                ImGui::Text("op: %s", tok_type_str(node->assign_stmt.op));
                break;
        }

        return WALK_CONTINUE;
    });

    for (u32 i = 0; i < last_depth; i++)
        ImGui::TreePop();
}

#define MAX_RETRIES 5

void send_nvim_keys(ccstr s) {
    auto& nv = world.nvim;
    nv.start_request_message("nvim_input", 1);
    nv.writer.write_string(s);
    nv.end_message();
}

void run_proc_the_normal_way(Process* proc, ccstr cmd) {
    proc->cleanup();
    proc->init();
    proc->dir = world.wksp.path;

    print("running: %s", cmd);
    proc->run(cmd);
}

void list_files() {
    auto& wnd = world.wnd_open_file;

    wnd.searching = true;

    world.jobs.flag_list_files = true;

    run_proc_the_normal_way(
        &world.jobs.list_files.proc,
        our_sprintf("ag --ignore-dir vendor -l --nocolor -g \"\" | fzf -f \"%s\"", wnd.query)
    );
}

struct General_Parser {
    // This struct houses a few methods that parse random things -- `go build` error
    // messages, `ag --ackmate` search results, etc. Includes some (extremely)
    // basic utilities for parsing.

    Process* proc;
    char ch;
    String_Allocator salloc;

    void init(Process* _proc) {
        ptr0(this);
        proc = _proc;
    }

    // \r\n is actually the dumbest thing ever lol
    bool read_char() {
        do {
            if (!proc->read1(&ch)) return false;
        } while (ch == '\r');
        return true;
    }

    bool peek_char(char* out) {
        char tmp;

        while (true) {
            if (!proc->peek(&tmp)) return false;
            if (tmp != '\r') break;
            if (!proc->read1(&tmp)) return false;
        }

        *out = tmp;
        return true;
    }

#define ASSERT(x) if (!(x)) { goto done; }
#define READ_CHAR() ASSERT(read_char())

    bool read_int(i32* out, char* delim = NULL) {
        char tmp[32];
        u32 i = 0;

        while (true) {
            READ_CHAR();
            if (!isdigit(ch)) {
                if (delim != NULL)
                    *delim = ch;
                break;
            }
            if (i + 1 >= _countof(tmp)) {
                do { READ_CHAR(); } while (isdigit(ch));
                return false;
            }
            tmp[i++] = ch;
        }

        tmp[i] = '\0';
        *out = strtol(tmp, NULL, 10);
        return true;

    done:
        return false;
    }

    // assumes we're in a SCOPED_MEM
    void parse_find_results() {
        while (true) {
            READ_CHAR();
            ASSERT(ch == ':');

            // read filename
            auto filename = read_until(MAX_PATH, '\n');
            if (filename == NULL) {
                do { READ_CHAR(); } while (ch != '\n');
                continue;
            }

            u32 skipped_results = 0;

            // read each line
            while (true) {
                char tmp = 0;
                ASSERT(peek_char(&tmp));
                // proc->peek(&tmp));
                if (tmp == '\n') {
                    READ_CHAR();
                    break;
                }

                i32 row = 0, col = 0, match_len = 0;
                bool ok = true;
                char delim;

                ok = ok && read_int(&row);
                ok = ok && read_int(&col);
                ok = ok && read_int(&match_len, &delim);

                u32 results_in_row = 1;
                while (ok && delim != ':') {
                    i32 junk;
                    ok = ok && read_int(&junk);
                    ok = ok && read_int(&junk, &delim);
                    results_in_row++;
                }

                if (!ok) {
                    skipped_results++;
                    while (ch != '\n') READ_CHAR();
                    continue;
                }

                auto result = alloc_object(Search_Result);
                world.search_results.results.append(result);

                result->filename = filename;
                result->row = row;
                result->match_col = col;
                result->match_len = match_len;
                result->results_in_row = results_in_row;

                if (col > 20) {
                    for (u32 i = 0; i < col - 20; i++) {
                        READ_CHAR();
                    }
                    result->match_col_in_preview = 20;
                } else {
                    result->match_col_in_preview = col;
                }

                const int PREVIEW_LEN = 40;

                auto preview = salloc.start(PREVIEW_LEN + 1);
                for (u32 i = 0; i < 40; i++) {
                    READ_CHAR();
                    if (ch == '\n') break;
                    salloc.push(ch);
                }
                salloc.done();

                while (ch != '\n') {
                    READ_CHAR();
                }

                result->preview = preview;
            }
        }

    done:
        return;
    }

    ccstr read_until(u32 to_reserve, char until) {
        auto ret = salloc.start(MAX_PATH);
        while (ch != until) {
            READ_CHAR();
            bool success = (ch == until ? salloc.done() : salloc.push(ch));
            if (!success) {
                salloc.revert();
                return NULL;
            }
        }
        return ret;

    done:
        return NULL;
    }

    // assumes we're in a SCOPED_POOL
    void parse_build_errors() {
        while (true) {
            READ_CHAR();
            if (ch == '#') {
                while (ch != '\n') {
                    READ_CHAR();
                }
                continue;
            }

            auto filename = read_until(MAX_PATH, ':');
            if (filename == NULL) {
                do { READ_CHAR(); } while (ch != '\n');
                continue;
            }

            i32 row = 0, col = 0;
            char delim = 0;

            ASSERT(read_int(&row, &delim));
            ASSERT(delim == ':');
            ASSERT(read_int(&col, &delim));
            ASSERT(delim == ':');

            READ_CHAR();
            ASSERT(ch == ' ');

            auto message = read_until(120, '\n');
            if (message == NULL) {
                do { READ_CHAR(); } while (ch != '\n');
                continue;
            }

            auto build_error = alloc_object(Build_Error);
            world.build_errors.errors.append(build_error);

            build_error->col = col;
            build_error->row = row;
            build_error->file = filename;
            build_error->message = message;
        }

    done:
        // TODO: any kind of cleanup here
        return;
    }

#undef READ_CHAR
#undef ASSERT
};

enum {
    OUR_MOD_NONE = 0,
    OUR_MOD_CMD = 1 << 0,
    OUR_MOD_SHIFT = 1 << 1,
    OUR_MOD_ALT = 1 << 2,
    OUR_MOD_CTRL = 1 << 3,
};

void init_everything() {
    world.init(false);
    ui.init();
}

int main() {
    if (run_tests()) return EXIT_SUCCESS;

    init_everything();

    SCOPED_MEM(&world.frame_mem);

    if (!glfwInit())
        return error("glfwInit failed"), EXIT_FAILURE;
    defer { glfwTerminate(); };

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    world.window = glfwCreateWindow(420, 420, WINDOW_TITLE, NULL, NULL);
    if (world.window == NULL)
        return error("could not create window"), EXIT_FAILURE;

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
    ImGui::StyleColorsDark();

    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.ItemInnerSpacing.x = 8;
        style.ItemSpacing.x = 10;
        style.ItemSpacing.y = 10;
        style.WindowPadding.x = 12;
        style.WindowPadding.y = 12;
    }

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

    // window sizing crap
    glfwSetWindowSize(world.window, 1280, 720);
    glfwSetWindowPos(world.window, 50, 50);

    // grab window_size, display_size, and display_scale
    glfwGetWindowSize(world.window, (i32*)&world.window_size.x, (i32*)&world.window_size.y);
    glfwGetFramebufferSize(world.window, (i32*)&world.display_size.x, (i32*)&world.display_size.y);
    glfwGetWindowContentScale(world.window, &world.display_scale.x, &world.display_scale.y);

    // now that we have world.display_size, we can call wksp.activate_pane
    world.wksp.activate_pane(0);

    if (!world.font.init(vera_mono_ttf, FONT_SIZE))
        return error("could not initialize font"), EXIT_FAILURE;
    io.Fonts->AddFontFromMemoryTTF(vera_mono_ttf, vera_mono_ttf_len, FONT_SIZE);

    GLuint im_font_texture;
    {
        // init imgui texture
        u8* pixels;
        i32 width, height;

        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        glGenTextures(1, &im_font_texture);
        glBindTexture(GL_TEXTURE_2D, im_font_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        io.Fonts->TexID = (void*)(intptr_t)im_font_texture;
    }

    glfwSetWindowSizeCallback(world.window, [](GLFWwindow* wnd, i32 w, i32 h) {
        world.window_size.x = w;
        world.window_size.y = h;

        auto& font = world.font;

        ui.recalculate_view_sizes();
        print("resizing to (%d, %d)", w, h);

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

        if (world.wksp.resizing_pane != -1) {
            auto i = world.wksp.resizing_pane;

            auto& pane1 = world.wksp.panes[i];
            auto& pane2 = world.wksp.panes[i+1];
            auto delta = world.ui.mouse_delta.x;

            if (delta < (100 - pane1.width)) {
                delta = 100 - pane1.width;
            }
            if (delta > pane2.width - 100) {
                delta = pane2.width - 100;
            }

            pane1.width += delta;
            pane2.width -= delta;
        } else {
            boxf resize_area;
            if (ui.get_current_resize_area(&resize_area) != -1) {
                glfwSetCursor(wnd, world.ui.cursors[ImGuiMouseCursor_ResizeEW]);
            } else {
                glfwSetCursor(wnd, world.ui.cursors[ImGuiMouseCursor_Arrow]);
            }
        }
    });

    glfwSetMouseButtonCallback(world.window, [](GLFWwindow* wnd, int button, int action, int mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                boxf resize_area;
                auto resize_area_index = ui.get_current_resize_area(&resize_area);
                if (resize_area_index != -1)
                    world.wksp.resizing_pane = resize_area_index;
            } else if (action == GLFW_RELEASE) {
                if (world.wksp.resizing_pane != -1) {
                    ui.recalculate_view_sizes();
                    world.wksp.resizing_pane = -1;
                }
            }
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            // handle sidebar clicks
            do {
                if (world.sidebar.view == SIDEBAR_CLOSED) break;

                auto sidebar_area = ui.get_sidebar_area();
                if (!sidebar_area.contains(world.ui.mouse_pos)) break;

                switch (world.sidebar.view) {
                    case SIDEBAR_FILE_EXPLORER:
                        {
                            auto index = (int)((world.ui.mouse_pos.y - sidebar_area.y + world.file_explorer.scroll_offset) / ui.font->height);

                            auto& files = world.file_explorer.files;

                            u32 file_index = 0;
                            for (u32 i = 0; i < index; i++) {
                                auto it = files[file_index];
                                if (it->num_children != -1 && !it->open)
                                    file_index = advance_subtree_in_file_explorer(file_index);
                                else
                                    file_index++;
                            }

                            if (index >= world.file_explorer.files.len) break;

                            auto file = world.file_explorer.files[file_index];
                            if (file->num_children == -1) { // normal file
                                u32 num_nodes = 0;
                                for (auto curr = file; curr != NULL; curr = curr->parent)
                                    num_nodes++;

                                {
                                    SCOPED_FRAME();

                                    auto path = alloc_array(File_Explorer_Entry*, num_nodes);
                                    u32 i = 0;
                                    for (auto curr = file; curr != NULL; curr = curr->parent)
                                        path[i++] = curr;

                                    Text_Renderer rend;
                                    rend.init();

                                    for (i32 j = num_nodes - 1; j >= 0; j--) {
                                        rend.write("%s", path[j]->name);
                                        if (j != 0)
                                            rend.write("/");
                                    }

                                    auto s = path_join(world.wksp.path, rend.finish());
                                    world.get_current_pane()->focus_editor(s);
                                }

                                ui.recalculate_view_sizes();
                            } else {
                                file->open = !file->open;
                            }
                        }
                        break;
                    case SIDEBAR_SEARCH_RESULTS:
                        {
                            auto index = (int)((world.ui.mouse_pos.y - sidebar_area.y + world.search_results.scroll_offset) / ui.font->height);
                            auto& results = world.search_results.results;

                            world.get_current_pane()->focus_editor(results[index]->filename);
                            ui.recalculate_view_sizes();
                        }
                        break;
                }
            } while (0);
        }
    });

    glfwSetScrollCallback(world.window, [](GLFWwindow* wnd, double dx, double dy) {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheelH += (float)dx;
        io.MouseWheel += (float)dy;

        if (ui.get_sidebar_area().contains(world.ui.mouse_pos) && world.sidebar.view != SIDEBAR_CLOSED) {
            dy *= (world.font.height * 3);

            auto add_dy = [&](i32* offset) {
                if (dy > 0)
                    *offset += dy;
                else
                    *offset = relu_sub(*offset, -dy);
            };

            switch (world.sidebar.view) {
                case SIDEBAR_FILE_EXPLORER:
                    add_dy(&world.file_explorer.scroll_offset);
                    break;
                case SIDEBAR_SEARCH_RESULTS:
                    add_dy(&world.search_results.scroll_offset);
                    break;
            }

            return;
        }

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
        if (ev == GLFW_PRESS) {
            print("keycallback press, sending to imgui");
            io.KeysDown[key] = true;
        }
        if (ev == GLFW_RELEASE) {
            print("keycallback release, sending to imgui");
            io.KeysDown[key] = false;
        }

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

        if (world.windows_open.open_file) {
            auto &wnd = world.wnd_open_file;
            switch (ev) {
                case GLFW_PRESS:
                case GLFW_REPEAT:
                    switch (key) {
                        case GLFW_KEY_DOWN:
                            wnd.selection++;
                            wnd.selection %= wnd.files.len;
                            break;
                        case GLFW_KEY_UP:
                            if (wnd.selection == 0)
                                wnd.selection = wnd.files.len - 1;
                            else
                                wnd.selection--;
                            break;
                        case GLFW_KEY_BACKSPACE:
                            {
                                auto &wnd = world.wnd_open_file;
                                if (wnd.query[0] != '\0')
                                    wnd.query[strlen(wnd.query) - 1] = '\0';
                                if (wnd.query[0] != '\0')
                                    list_files();
                                else
                                    wnd.files.len = 0;
                            }
                            break;
                        case GLFW_KEY_ESCAPE:
                            world.jobs.list_files.proc.cleanup();
                            world.windows_open.open_file = false;
                            break;
                        case GLFW_KEY_ENTER:
                            world.jobs.list_files.proc.cleanup();
                            world.windows_open.open_file = false;

                            if (wnd.files.len == 0) break;

                            auto pane = world.get_current_pane();
                            auto query = path_join(world.wksp.path, wnd.files[wnd.selection]);

                            pane->focus_editor(query);
                            ui.recalculate_view_sizes();
                            break;
                    }
                    break;
            }
            return;
        }

        switch (ev) {
            case GLFW_PRESS:
            case GLFW_REPEAT:
                switch (nmod) {
                    case OUR_MOD_SHIFT:
                        if (world.use_nvim) {
                            switch (key) {
                                case GLFW_KEY_ENTER: send_nvim_keys("<S-Enter>"); break;
                                case GLFW_KEY_TAB: send_nvim_keys("<S-Tab>"); break;
                                case GLFW_KEY_UP: send_nvim_keys("<S-Up>"); break;
                                case GLFW_KEY_RIGHT: send_nvim_keys("<S-Right>"); break;
                                case GLFW_KEY_DOWN: send_nvim_keys("<S-Down>"); break;
                                case GLFW_KEY_LEFT: send_nvim_keys("<S-Left>"); break;
                                case GLFW_KEY_ESCAPE: send_nvim_keys("<S-Esc>"); break;
                                case GLFW_KEY_BACKSPACE: send_nvim_keys("<S-Backspace>"); break;
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
                                    case GLFW_KEY_ENTER: send_nvim_keys("<C-Enter>"); break;
                                    // case GLFW_KEY_TAB: send_nvim_keys("<C-Tab>"); break;
                                    case GLFW_KEY_UP: send_nvim_keys("<C-Up>"); break;
                                    case GLFW_KEY_RIGHT: send_nvim_keys("<C-Right>"); break;
                                    case GLFW_KEY_DOWN: send_nvim_keys("<C-Down>"); break;
                                    case GLFW_KEY_LEFT: send_nvim_keys("<C-Left>"); break;
                                    case GLFW_KEY_ESCAPE: send_nvim_keys("<C-Esc>"); break;
                                    case GLFW_KEY_BACKSPACE: send_nvim_keys("<C-Backspace>"); break;
                                    default: handled = false; break;
                                }
                            }

                            if (handled) break;

                            switch (key) {
                                case GLFW_KEY_1:
                                    world.wksp.activate_pane(0);
                                    break;
                                case GLFW_KEY_2:
                                    world.wksp.activate_pane(1);
                                    break;
                                case GLFW_KEY_3:
                                    world.wksp.activate_pane(2);
                                    break;
                                case GLFW_KEY_4:
                                    world.wksp.activate_pane(3);
                                    break;
                                case GLFW_KEY_P:
                                    world.windows_open.open_file ^= 1;
                                    if (world.windows_open.open_file) {
                                        auto &wnd = world.wnd_open_file;

                                        wnd.query[0] = '\0';
                                        wnd.selection = 0;
                                        wnd.searching = false;
                                        wnd.files.init(LIST_MALLOC, 128);
                                    }
                                    break;
                                case GLFW_KEY_S:
                                    {
                                        auto editor = world.get_current_editor();
                                        if (editor == NULL) break;

                                        if (editor->is_untitled) {
                                            Select_File_Opts opts;
                                            opts.buf = editor->filepath;
                                            opts.bufsize = _countof(editor->filepath);
                                            opts.folder = false;
                                            opts.save = true;
                                            let_user_select_file(&opts);
                                            editor->is_untitled = false;
                                        }

                                        FILE* f = fopen(editor->filepath, "w");
                                        if (f == NULL) break; // TODO: display error
                                        defer { fclose(f); };

                                        editor->buf.write(f);
                                        break;
                                    }
                                case GLFW_KEY_G:
                                    {
                                        auto editor = world.get_current_editor();
                                        if (editor == NULL) break;

                                        SCOPED_MEM(&world.index.main_thread_mem);
                                        defer { world.index.main_thread_mem.reset(); };

                                        auto result = world.index.jump_to_definition(editor->filepath, new_cur2(editor->cur_to_offset(editor->cur), 0));
                                        if (result == NULL) {
                                            error("unable to jump to definition");
                                            return;
                                        }

                                        auto target_ed = editor;

                                        if (!streq(editor->filepath, result->file)) {
                                            target_ed = world.get_current_pane()->focus_editor(result->file);
                                            ui.recalculate_view_sizes();
                                        }

                                        if (target_ed != NULL) {
                                            auto pos = result->pos;

                                            if (world.use_nvim) {
                                                target_ed->nvim_data.initial_pos = pos;
                                                target_ed->nvim_data.need_initial_pos_set = true;
                                            } else {
                                                target_ed->move_cursor(pos);
                                            }
                                        }
                                        break;
                                    }
                                case GLFW_KEY_SPACE:
                                    {
                                        auto ed = world.get_current_editor();
                                        if (ed == NULL) break;
                                        // TODO: set triggered_by_dot to true if the last character is a
                                        // period
                                        ed->trigger_autocomplete(false);
                                        break;
                                    }
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
                                    // TODO: Close tab.
                                    break;
                                case GLFW_KEY_N:
                                    world.get_current_pane()->open_empty_editor();
                                    ui.recalculate_view_sizes();
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
                                    case GLFW_KEY_ENTER: send_nvim_keys("<C-S-Enter>"); break;
                                    case GLFW_KEY_UP: send_nvim_keys("<C-S-Up>"); break;
                                    case GLFW_KEY_RIGHT: send_nvim_keys("<C-S-Right>"); break;
                                    case GLFW_KEY_DOWN: send_nvim_keys("<C-S-Down>"); break;
                                    case GLFW_KEY_LEFT: send_nvim_keys("<C-S-Left>"); break;
                                    case GLFW_KEY_ESCAPE: send_nvim_keys("<C-S-Esc>"); break;
                                    case GLFW_KEY_BACKSPACE: send_nvim_keys("<C-S-Backspace>"); break;
                                    default: handled = false; break;
                                }
                            }

                            if (handled) break;

                            switch (key) {
                                case GLFW_KEY_A:
                                    world.windows_open.ast_viewer ^= 1;
                                    break;
                                case GLFW_KEY_F:
                                    world.windows_open.search_and_replace ^= 1;
                                    break;
                                case GLFW_KEY_E:
                                    if (world.sidebar.view == SIDEBAR_FILE_EXPLORER)
                                        world.sidebar.view = SIDEBAR_CLOSED;
                                    else
                                        world.sidebar.view = SIDEBAR_FILE_EXPLORER;
                                    break;
                                case GLFW_KEY_LEFT_BRACKET:
                                    {
                                        auto pane = world.get_current_pane();
                                        u32 idx;
                                        if (pane->current_editor == 0)
                                            idx = pane->editors.len - 1;
                                        else
                                            idx = pane->current_editor - 1;
                                        pane->focus_editor_by_index(idx);
                                        break;
                                    }
                                case GLFW_KEY_RIGHT_BRACKET:
                                    {
                                        auto pane = world.get_current_pane();
                                        auto idx = (pane->current_editor + 1) % pane->editors.len;
                                        pane->focus_editor_by_index(idx);
                                        break;
                                    }
                            }
                            break;
                        }
                    case OUR_MOD_NONE:
                        {
                            bool done = true;

                            // first handle non-editor keys

                            switch (key) {
                                case GLFW_KEY_F12:
                                    world.windows_open.im_demo ^= 1;
                                    break;
                                case GLFW_KEY_F5:
                                    switch (world.dbg.state_flag) {
                                        case DBGSTATE_PAUSED:
                                            {
                                                Dbg_Call call;
                                                call.type = DBGCALL_CONTINUE_RUNNING;
                                                if (!world.dbg.call_queue.push(&call)) {
                                                    // TODO: surface error
                                                }
                                            }
                                            break;

                                        case DBGSTATE_INACTIVE:
                                            do {
                                                auto &&dbg = world.dbg;
                                                dbg.call_queue.init();

                                                // TODO: when do we stop debugging? do we kill this thread, wait for it to die, etc?
                                                dbg.thread = create_thread(debugger_loop_thread, NULL);
                                                if (dbg.thread == NULL) {
                                                    error("unable to create thread to start process: %s", get_last_error());
                                                    dbg.call_queue.cleanup();
                                                    break;
                                                }
                                            } while (0);
                                            break;
                                    }
                                    break;
                                case GLFW_KEY_F9:
                                    {
                                        auto editor = world.get_current_editor();
                                        ccstr file = editor->filepath;
                                        auto lineno = editor->cur.y + 1;

                                        auto &&ref = world.dbg;
                                        auto &&dbg = ref.debugger;

                                        auto idx = ref.breakpoints.find([&](Client_Breakpoint *it) -> bool {
                                            return are_breakpoints_same(file, lineno, it->file, it->line);
                                        });

                                        if (idx == -1) {
                                            auto it = ref.breakpoints.append();
                                            it->file = file;
                                            it->line = lineno;
                                            it->pending = true;

                                            if (ref.state_flag != DBGSTATE_INACTIVE) {
                                                Dbg_Call call;
                                                call.type = DBGCALL_SET_BREAKPOINT;
                                                call.set_breakpoint.filename = file;
                                                call.set_breakpoint.lineno = lineno;

                                                if (!ref.call_queue.push(&call)) {
                                                    // TODO: surface error
                                                }
                                            }
                                        } else {
                                            ref.breakpoints.remove(idx);
                                            if (ref.state_flag != DBGSTATE_INACTIVE) {
                                                ref.debugger.unset_breakpoint(file, lineno);

                                                Dbg_Call call;
                                                call.type = DBGCALL_UNSET_BREAKPOINT;
                                                call.unset_breakpoint.filename = file;
                                                call.unset_breakpoint.lineno = lineno;

                                                if (!ref.call_queue.push(&call)) {
                                                    // TODO: surface error
                                                }
                                            }
                                        }
                                    }
                                    break;
                                case GLFW_KEY_F10:
                                    {
                                        if (world.dbg.state_flag != DBGSTATE_PAUSED) break;

                                        Dbg_Call call;
                                        call.type = DBGCALL_STEP_OVER;
                                        if (!world.dbg.call_queue.push(&call)) {
                                            // TODO: surface error
                                        }
                                    }
                                    break;
                                case GLFW_KEY_F11:
                                    {
                                        if (world.dbg.state_flag != DBGSTATE_PAUSED) break;

                                        Dbg_Call call;
                                        call.type = DBGCALL_STEP_INTO;

                                        if (!world.dbg.call_queue.push(&call)) {
                                            // TODO: surface error
                                        }
                                    }
                                    break;
                                default:
                                    done = false;
                                    break;
                            }

                            if (done) break;
                            if (world.windows_open.is_any_open()) break;
                            if (world.popups_open.is_any_open()) break;

                            auto editor = world.get_current_editor();
                            if (editor == NULL) break;

                            auto &buf = editor->buf;
                            auto cur = editor->cur;

                            /*
                            Here we send the key to nvim. We need to handle the response when:
                            we need to do something that relies on the *effect* of the key being completed.
                            For example, when the user types a '.', we need the period to be in the buffer
                            before calling trigger_autocomplete, since it passes the buffer through a Go parser
                            and expects to find an (incomplete) AST_SELECTOR_EXPR.

                            So, the only keys we need to track are:
                             - backspace (for autocomplete, the character needs to be erased first)
                             - typed characters in insert mode
                                 - this includes '.' (autocomplete) and '(' (parameter hints)
                            */

                            switch (key) {
                                case GLFW_KEY_UP: send_nvim_keys("<Up>"); break;
                                case GLFW_KEY_RIGHT: send_nvim_keys("<Right>"); break;
                                case GLFW_KEY_DOWN: send_nvim_keys("<Down>"); break;
                                case GLFW_KEY_LEFT: send_nvim_keys("<Left>"); break;
                                case GLFW_KEY_BACKSPACE: send_nvim_keys("<Backspace>"); break;

                                case GLFW_KEY_TAB:
                                case GLFW_KEY_ENTER:
                                    {
                                        auto& ac = editor->autocomplete;
                                        if (ac.ac.results != NULL && ac.filtered_results->len > 0) {
                                            auto idx = ac.filtered_results->at(ac.selection);
                                            auto& result = ac.ac.results->at(idx);

                                            auto ac_start = editor->cur;
                                            ac_start.x -= strlen(ac.prefix);
                                            buf.remove(ac_start, editor->cur);

                                            auto len = strlen(result.name);
                                            auto name = alloc_array(uchar, len);
                                            for (u32 i = 0; i < len; i++)
                                                name[i] = (uchar)result.name[i];
                                            buf.insert(ac_start, name, len);

                                            ac.ac.results = NULL;
                                            ac_start.x += len;

                                            // update buffer
                                            {
                                                auto& nv = world.nvim;
                                                auto msgid = nv.start_request_message("nvim_buf_set_lines", 5);
                                                {
                                                    auto req = nv.save_request(NVIM_REQ_AUTOCOMPLETE_SETBUF, msgid, editor->id);
                                                    req->autocomplete_setbuf.target_cursor = ac_start;
                                                }
                                                nv.writer.write_int(editor->nvim_data.buf_id); // buffer
                                                nv.writer.write_int(ac_start.y); // start
                                                nv.writer.write_int(ac_start.y + 1); // end
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
                                                send_nvim_keys("<Tab>");
                                            else
                                                send_nvim_keys("<Enter>");
                                        }
                                        break;
                                    }
                                case GLFW_KEY_ESCAPE:
                                    {
                                        bool handled = false;

                                        if (editor->autocomplete.ac.results != NULL) {
                                            handled = true;
                                            editor->autocomplete.ac.results = NULL;
                                        }
                                        if (editor->parameter_hint.params != NULL) {
                                            handled = true;
                                            editor->parameter_hint.params = NULL;
                                        }

                                        // if (!handled) send_nvim_keys("<Esc>");
                                        send_nvim_keys("<Esc>");
                                    }
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
        if (ch > 0 && ch < 0x10000) {
            print("charcallback, sending to imgui");
            io.AddInputCharacter((u16)ch);
        }

        if (world.windows_open.open_file) {
            auto& ref = world.wnd_open_file;
            auto len = strlen(ref.query);
            if (len + 1 < _countof(ref.query)) {
                if (ch < 0xff && isprint(ch)) {
                    ref.query[len] = ch;
                    ref.query[len + 1] = '\0';
                    list_files();
                }
            }
            return;
        }

        if (world.popups_open.is_any_open()) return;
        if (world.windows_open.is_any_open()) return;

        auto ed = world.get_current_editor();
        if (ed == NULL) return;

        if (isprint(ch)) {
            char keys[2] = { (char)ch, '\0' };
            // TODO: i don't think we always need to track response, right?
            // only when autocomplete or parameter hints is open
            send_nvim_keys(keys);
        }
    });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);

    GLuint im_vao, im_vbo, im_vebo;
    glGenVertexArrays(1, &im_vao);
    glBindVertexArray(im_vao);
    glGenBuffers(1, &im_vbo);
    glGenBuffers(1, &im_vebo);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);

    {
        // set opengl program attributes
        glUseProgram(world.ui.program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

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

        loc = glGetAttribLocation(world.ui.program, "solid");
        glEnableVertexAttribArray(loc);
        glVertexAttribIPointer(loc, 1, GL_INT, sizeof(Vert), (void*)_offsetof(Vert, solid));

        loc = glGetUniformLocation(world.ui.program, "projection");
        mat4f ortho_projection;
        new_ortho_matrix(ortho_projection, 0, world.display_size.x, world.display_size.y, 0);
        glUniformMatrix4fv(loc, 1, GL_FALSE, (float*)ortho_projection);

        loc = glGetUniformLocation(world.ui.program, "tex");
        glUniform1i(loc, 0);
    }

    {
        // set imgui program attributes
        glUseProgram(world.ui.im_program);
        glBindVertexArray(im_vao);
        glBindBuffer(GL_ARRAY_BUFFER, im_vbo);

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
        glUniform1i(loc, 0);
    }

    double last_time = glfwGetTime();
    i64 last_frame_time = current_time_in_nanoseconds();

#if 1
    {
        SCOPED_FRAME();
        auto path = path_join(world.wksp.path, "sync/sync.go");
        world.get_current_pane()->focus_editor(path);
        ui.recalculate_view_sizes();

        auto editor = world.get_current_editor();
        while (!editor->is_nvim_ready()) continue;

        world.get_current_editor()->move_cursor(new_cur2(10, 11));
    }
#endif

    while (!glfwWindowShouldClose(world.window)) {
        world.frame_mem.reset();

        SCOPED_MEM(&world.frame_mem);

        {
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

                        General_Parser parser;
                        parser.init(&proc);
                        {
                            SCOPED_MEM(&search_results.pool);
                            parser.parse_find_results();
                        }

                        world.sidebar.view = SIDEBAR_SEARCH_RESULTS;
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

            if (world.jobs.flag_list_files) {
                auto& proc = world.jobs.list_files.proc;
                auto& wnd = world.wnd_open_file;

                switch (proc.status()) {
                    case PROCESS_ERROR:
                        wnd.files.len = 0;
                        wnd.searching = false;
                        world.jobs.flag_list_files = false;
                        proc.cleanup();
                        break;
                    case PROCESS_DONE:
                        world.jobs.flag_list_files = false;
                        wnd.searching = false;

                        {
                            SCOPED_MEM(&world.open_file_mem);
                            world.open_file_mem.reset();

                            wnd.files.len = 0;

                            char c;
                            char* s = alloc_array(char, 0);

                            while (proc.read1(&c)) {
                                if (c == '\n') {
                                    *alloc_object(char) = '\0';
                                    wnd.files.append(s);
                                    s = alloc_array(char, 0);
                                } else {
                                    *alloc_object(char) = c;
                                }
                            }
                        }

                        proc.cleanup();
                        break;
                }
            }

            if (world.jobs.flag_build) {
                auto& proc = world.jobs.build.proc;
                // auto& wnd = world.wnd_open_file;

                switch (proc.status()) {
                    case PROCESS_DONE:
                        world.search_results.pool.cleanup();

                        world.jobs.flag_build = false;
                        if (proc.exit_code != 0) {
                            auto& ref = world.build_errors;
                            ref.cleanup();
                            ref.init();

                            General_Parser parser;
                            parser.init(&proc);
                            {
                                SCOPED_MEM(&ref.pool);
                                parser.parse_build_errors();
                            }

                            world.error_list.show = true;
                        } else {
                            world.jobs.build.signal_done = true;
                        }

                        proc.cleanup();
                        break;

                    case PROCESS_ERROR:
                        world.jobs.flag_build = false;
                        world.search_results.cleanup();
                        proc.cleanup();
                        break;
                }
            }
        }

        glDisable(GL_SCISSOR_TEST);
        glClearColor(COLOR_WHITE.r, COLOR_WHITE.g, COLOR_WHITE.b, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        // TODO: do we need to do this every frame?
        io.DisplaySize = ImVec2((float)world.window_size.x, (float)world.window_size.y);
        io.DisplayFramebufferScale = ImVec2(world.display_scale.x, world.display_scale.y);

        double new_time = glfwGetTime();
        io.DeltaTime = last_time > 0.0 ? (float)(new_time - last_time) : (float)(1.0f / 60.0f);
        last_time = new_time;

        if (glfwGetWindowAttrib(world.window, GLFW_FOCUSED)) {
            if (io.WantSetMousePos) {
                glfwSetCursorPos(world.window, (double)io.MousePos.x, (double)io.MousePos.y);
            } else {
                double x, y;
                glfwGetCursorPos(world.window, &x, &y);
                io.MousePos = ImVec2((float)x, (float)y);
            }
        } else {
            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        }

        for (i32 i = 0; i < 3; i++) {
            io.MouseDown[i] = world.ui.mouse_buttons_pressed[i] || glfwGetMouseButton(world.window, i) != 0;
            world.ui.mouse_buttons_pressed[i] = false;
        }

        bool cur_changed = ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0);
        bool cur_enabled = (glfwGetInputMode(world.window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED);

        if (cur_changed && cur_enabled) {
            ImGuiMouseCursor cur = ImGui::GetMouseCursor();
            if (io.MouseDrawCursor || cur == ImGuiMouseCursor_None) {
                glfwSetInputMode(world.window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            } else {
                glfwSetInputMode(world.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                // glfwSetCursor(world.window, world.ui.cursors[cur] ? world.ui.cursors[cur] : world.ui.cursors[ImGuiMouseCursor_Arrow]);
            }
        }

        ui.draw_everything(vao, vbo, world.ui.program);

        {
            // render imgui stuff
            ImGui::NewFrame();

            if (world.windows_open.im_demo)
                ImGui::ShowDemoWindow(&world.windows_open.im_demo);

            if (world.windows_open.open_file) {
                auto& wnd = world.wnd_open_file;

                ImGui::Begin("Open File", &world.windows_open.open_file, ImGuiWindowFlags_AlwaysAutoResize);

                ImGui::Text("Search for file:");
                ImGui::Text("%-30s", wnd.query);
                ImGui::Separator();

                for (u32 i = 0; i < wnd.files.len; i++) {
                    auto& it = wnd.files[i];

                    if (i == wnd.selection)
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", it);
                    else
                        ImGui::Text("%s", it);
                }

                ImGui::End();
            }

            if (world.windows_open.ast_viewer) {
                ImGui::Begin("AST Viewer", &world.windows_open.ast_viewer);

                auto ed = world.get_current_editor();
                if (ed == NULL) {
                    ImGui::Text("Open a file to parse its AST.");
                } else {
                    if (ImGui::Button("Parse AST from current file")) {
                        world.ast_viewer_mem.reset();
                        SCOPED_MEM(&world.ast_viewer_mem);
                        world.wnd_ast_viewer.ast = parse_file_into_ast(ed->filepath);
                    }
                }

                auto ast = world.wnd_ast_viewer.ast;
                if (ast != NULL)
                    render_ast(ast);

                ImGui::End();
            }

            if (world.dbg.state_flag != DBGSTATE_INACTIVE) {
                ImGui::Begin("Debugger");
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                static float w = 200.0f;
                static float h = 300.0f;

                ImGui::BeginChild("child1", ImVec2(w, 0), true);
                {
                    if (ImGui::CollapsingHeader("Call Stack", ImGuiTreeNodeFlags_DefaultOpen)) {
                        u32 i = 0;
                        if (world.dbg.state.stackframe != NULL) {
                            For (*world.dbg.state.stackframe) {
                                // ImGuiTreeNodeFlags_Bullet
                                auto tree_flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                                if (world.wnd_debugger.current_location == i)
                                    tree_flags |= ImGuiTreeNodeFlags_Selected;

                                ImGui::TreeNodeEx((void*)(uptr)i, tree_flags, "%s (%s:%d)", it.func_name, our_basename(it.filepath), it.lineno);
                                if (ImGui::IsItemClicked()) {
                                    world.wnd_debugger.current_location = i;

                                    Dbg_Call call;
                                    call.type = DBGCALL_EVAL_WATCHES;
                                    call.eval_watches.frame_id = i;
                                    world.dbg.call_queue.push(&call);
                                }

                                i++;
                            }
                        }
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::InvisibleButton("vsplitter", ImVec2(8.0f, -1));
                if (ImGui::IsItemActive())
                    w += ImGui::GetIO().MouseDelta.x;

                ImGui::SameLine();

                ImGui::BeginGroup();
                {
                    ImGui::BeginChild("child2", ImVec2(0, h), true);
                    {
                        if (ImGui::CollapsingHeader("Local Variables", ImGuiTreeNodeFlags_DefaultOpen)) {
                            if (world.wnd_debugger.current_location != -1) {
                                auto& loc = world.dbg.state.stackframe->at(world.wnd_debugger.current_location);

                                fn<void(Dbg_Var*)> render_var;
                                int k = 0;

                                ImGuiTreeNodeFlags tree_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                                render_var = [&](Dbg_Var* var) {
                                    bool recurse = false;

                                    {
                                        SCOPED_FRAME();
                                        auto str_id = our_sprintf("%d-%d", world.wnd_debugger.current_location, k++);

                                        if (var->children == NULL || var->children->len == 0)
                                            ImGui::TreeNodeEx(str_id, tree_flags, "%s = %s", var->name, var->value);
                                        else
                                            recurse = ImGui::TreeNode(str_id, "%s = %s", var->name, var->value);
                                    }

                                    if (recurse) {
                                        For (*var->children)
                                            render_var(&it);
                                        ImGui::TreePop();
                                    }
                                };

                                if (loc.locals != NULL) {
                                    For (*loc.locals)
                                        render_var(&it);
                                } else {
                                    ImGui::Text("This location has no local variables.");
                                }
                            } else {
                                ImGui::Text("Select a location in the call stack to view the variables there.");
                            }
                        }
                    }
                    ImGui::EndChild();

                    ImGui::InvisibleButton("hsplitter", ImVec2(-1, 8.0f));
                    if (ImGui::IsItemActive())
                        h += ImGui::GetIO().MouseDelta.y;

                    ImGui::BeginChild("child3", ImVec2(0, 0), true);
                    {
                        if (ImGui::CollapsingHeader("Watches", ImGuiTreeNodeFlags_DefaultOpen)) {
                            For (world.dbg.watches) {
                                ImGui::Text("%s", it.expr);
                                switch (it.state) {
                                    case DBGWATCH_ERROR:
                                        ImGui::Text("<error reading>");
                                        break;
                                    case DBGWATCH_PENDING:
                                        ImGui::Text("Waiting...");
                                        break;
                                    case DBGWATCH_READY:
                                        ImGui::Text("%s", it.value.value);
                                        break;
                                }
                                ImGui::NewLine();
                            }

                            static char expr_buffer[256];

                            if (ImGui::Button("Add Watch...")) {
                                expr_buffer[0] = '\0';
                                world.popups_open.debugger_add_watch = true;
                                ImGui::OpenPopup("Add Watch");
                            }

                            if (ImGui::BeginPopupModal("Add Watch", &world.popups_open.debugger_add_watch, ImGuiWindowFlags_AlwaysAutoResize)) {
                                ImGui::InputText("Expression", expr_buffer, IM_ARRAYSIZE(expr_buffer));
                                if (ImGui::Button("OK", ImVec2(120, 0))) {
                                    auto watch = world.dbg.watches.append();
                                    strncpy(watch->expr, expr_buffer, _countof(watch->expr));
                                    watch->state = DBGWATCH_PENDING;

                                    if (world.dbg.state_flag == DBGSTATE_PAUSED)
                                        if (world.wnd_debugger.current_location != -1) {
                                            Dbg_Call call;
                                            call.type = DBGCALL_EVAL_SINGLE_WATCH;
                                            call.eval_single_watch.frame_id = world.wnd_debugger.current_location;
                                            call.eval_single_watch.watch_id = world.dbg.watches.len - 1;
                                            world.dbg.call_queue.push(&call);
                                        }

                                    ImGui::CloseCurrentPopup();
                                }
                                ImGui::SetItemDefaultFocus();
                                ImGui::SameLine();
                                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                                    ImGui::CloseCurrentPopup();
                                }

                                ImGui::EndPopup();
                            }
                        }
                    }
                    ImGui::EndChild();
                }
                ImGui::EndGroup();

                ImGui::PopStyleVar();
                ImGui::End();
            }

            if (world.windows_open.search_and_replace) {
                auto& wnd = world.wnd_search_and_replace;

                ImGui::Begin("Search and Replace", &world.windows_open.search_and_replace, ImGuiWindowFlags_AlwaysAutoResize);

                ImGui::Text("Search for:");
                ImGui::InputText("##find", wnd.find_str, _countof(wnd.find_str));
                ImGui::Text("Replace with:");
                ImGui::InputText("##replace", wnd.replace_str, _countof(wnd.replace_str));

                ImGui::Separator();

                ImGui::Checkbox("Case-sensitive", &wnd.case_sensitive);
                ImGui::SameLine();
                ImGui::Checkbox("Use regular expression", &wnd.use_regex);

                ImGui::Separator();

                if (ImGui::Button("Search for All")) {
                    world.search_results.pool.cleanup();
                    run_proc_the_normal_way(
                        &world.jobs.search.proc,
                        our_sprintf("ag --ackmate %s %s \"%s\"", wnd.use_regex ? "" : "-Q", wnd.case_sensitive ? "-s" : "", wnd.find_str)
                    );
                    world.jobs.flag_search = true;
                }

                ImGui::SameLine();

                if (ImGui::Button("Replace All")) {
                    // TODO: some kind of permanent settings system
                    // so we can save things like "don't show this popup again"
                    ImGui::OpenPopup("Really replace?");
                }

                if (world.jobs.search_and_replace.signal_done) {
                    world.jobs.search_and_replace.signal_done = false;
                    ImGui::OpenPopup("Done");
                }

                ImGui::SetNextWindowSize(ImVec2(450, -1));
                if (ImGui::BeginPopupModal("Really replace?", NULL, ImGuiWindowFlags_NoResize)) {
                    ImGui::TextWrapped("Pressing this button will automatically, irreversibly perform the text replacement you requested.");
                    ImGui::TextWrapped("If you make a mistake, we trust your version control system will save you.");
                    ImGui::TextWrapped("To preview your changes before replacing, click Find All first.");
                    ImGui::TextWrapped("We won't show this warning again.");
                    ImGui::NewLine();
                    if (ImGui::Button("Ok, replace all")) {
                        run_proc_the_normal_way(
                            &world.jobs.search_and_replace.proc,
                            our_sprintf("ag -g \"\" | xargs sed -i \"s/%s/%s/g\"", wnd.find_str, wnd.replace_str)
                        );
                        world.jobs.flag_search_and_replace = true;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) {
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }

                ImGui::SetNextWindowSize(ImVec2(450, -1));
                if (ImGui::BeginPopupModal("Done", NULL, ImGuiWindowFlags_NoResize)) {
                    ImGui::TextWrapped("Replace operation has completed.");
                    ImGui::NewLine();
                    if (ImGui::Button("Ok")) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::End();
            }

            if (world.windows_open.build_and_debug) {
                ImGui::Begin("Build and debug", &world.windows_open.build_and_debug, ImGuiWindowFlags_AlwaysAutoResize);

                ImGui::InputText("##build_command", world.settings.build_command, _countof(world.settings.build_command));
                if (ImGui::Button("Build")) {
                    run_proc_the_normal_way(&world.jobs.build.proc, world.settings.build_command);
                    world.jobs.flag_build = true;
                }

                if (world.jobs.build.signal_done) {
                    world.jobs.build.signal_done = false;
                    ImGui::OpenPopup("Done");
                }

                ImGui::SetNextWindowSize(ImVec2(450, -1));
                if (ImGui::BeginPopupModal("Done", NULL, ImGuiWindowFlags_NoResize)) {
                    ImGui::TextWrapped("The build has completed with no errors.");
                    ImGui::NewLine();
                    if (ImGui::Button("Ok")) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::End();
            }

            ImGui::Render();
        }

        {
            // draw imgui buffers
            ImDrawData* draw_data = ImGui::GetDrawData();
            draw_data->ScaleClipRects(ImVec2(world.display_scale.x, world.display_scale.y));

            glViewport(0, 0, world.display_size.x, world.display_size.y);
            glUseProgram(world.ui.im_program);
            glBindVertexArray(im_vao);
            glBindTexture(GL_TEXTURE_2D, im_font_texture);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glEnable(GL_SCISSOR_TEST);

            for (i32 i = 0; i < draw_data->CmdListsCount; i++) {
                const ImDrawList* cmd_list = draw_data->CmdLists[i];
                const ImDrawIdx* offset = 0;

                glBindBuffer(GL_ARRAY_BUFFER, im_vbo);
                glBufferData(GL_ARRAY_BUFFER, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, im_vebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

                i32 elem_size = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

                for (i32 j = 0; j < cmd_list->CmdBuffer.Size; j++) {
                    const ImDrawCmd* cmd = &cmd_list->CmdBuffer[j];

                    glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)cmd->TextureId);
                    glScissor(cmd->ClipRect.x, (world.display_size.y - cmd->ClipRect.w), (cmd->ClipRect.z - cmd->ClipRect.x), (cmd->ClipRect.w - cmd->ClipRect.y));
                    glDrawElements(GL_TRIANGLES, cmd->ElemCount, elem_size, offset);
                    offset += cmd->ElemCount;
                }
            }
        }

        glfwPollEvents();
        glfwSwapBuffers(world.window);

        {
            // wait until next frame
            auto curr = current_time_in_nanoseconds();
            auto rest = (1000000000.f / FRAME_RATE_CAP) - (curr - last_frame_time);
            if (rest > 0)
                sleep_milliseconds((u32)(rest / 1000.0f / 1000.0f));
            last_frame_time = curr;
        }
    }

    return EXIT_SUCCESS;
}
