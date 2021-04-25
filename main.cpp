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

void do_build(void*) {
    auto& indexer = world.indexer;
    auto& b = world.build;

    world.error_list.show = true;

    SCOPED_MEM(&b.mem);

    b.id = world.next_build_id++;

    {
        SCOPED_LOCK(&indexer.gohelper_lock);

        indexer.gohelper_run(GH_OP_START_BUILD, world.settings.build_command, NULL);
        if (indexer.gohelper_returned_error) {
            b.done = true;
            b.build_itself_had_error = true;
            return;
        }
    }

    for (;; sleep_milliseconds(100)) {
        SCOPED_LOCK(&indexer.gohelper_lock);

        auto resp = indexer.gohelper_run(GH_OP_GET_BUILD_STATUS, NULL);
        if (indexer.gohelper_returned_error) {
            b.done = true;
            b.build_itself_had_error = true;
            return;
        }

        if (!streq(resp, "done")) continue;

        auto len = indexer.gohelper_readint();

        for (u32 i = 0; i < len; i++) {
            auto err = b.errors.append();

            err->message = indexer.gohelper_readline();
            err->valid = (bool)indexer.gohelper_readint();

            if (err->valid) {
                err->file = indexer.gohelper_readline();
                err->row = indexer.gohelper_readint();
                err->col = indexer.gohelper_readint();
                auto is_vcol = indexer.gohelper_readint();
            }
        }

        b.current_error = -1;
        b.done = true;
        b.creating_extmarks = true;
        world.error_list.show = true;

        {
            auto &nv = world.nvim;
            auto msgid = nv.start_request_message("nvim_create_namespace", 1);
            nv.save_request(NVIM_REQ_CREATE_EXTMARKS_CREATE_NAMESPACE, msgid, 0);
            nv.writer.write_string(our_sprintf("build-%d", world.build.id));
            nv.end_message();
        }

        break;
    }
}

void kick_off_build() {
    world.build.cleanup();
    world.build.init();
    world.build.thread = create_thread(do_build, NULL);
}

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

void render_godecl(Godecl *decl);
void render_gotype(Gotype *gotype, ccstr field = NULL);

void render_godecl(Godecl *decl) {
    auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ImGui::TreeNodeEx(decl, flags, "%s", godecl_type_str(decl->type))) {
        ImGui::Text("decl_start: %s", format_pos(decl->decl_start));
        ImGui::Text("spec_start: %s", format_pos(decl->spec_start));
        ImGui::Text("name_start: %s", format_pos(decl->name_start));
        ImGui::Text("name: %s", decl->name);

        switch (decl->type) {
        case GODECL_IMPORT:
            ImGui::Text("import_path: %s", decl->import_path);
            break;
        case GODECL_VAR:
        case GODECL_CONST:
        case GODECL_TYPE:
        case GODECL_FUNC:
        case GODECL_FIELD:
        case GODECL_SHORTVAR:
            render_gotype(decl->gotype);
            break;
        }
        ImGui::TreePop();
    }
}

void render_gotype(Gotype *gotype, ccstr field) {
    if (gotype == NULL) return;

    auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    bool is_open = false;

    if (field == NULL)
        is_open = ImGui::TreeNodeEx(gotype, flags, "%s", gotype_type_str(gotype->type));
    else
        is_open = ImGui::TreeNodeEx(gotype, flags, "%s: %s", field, gotype_type_str(gotype->type));

    if (is_open) {
        switch (gotype->type) {
        case GOTYPE_ID:
            ImGui::Text("name: %s", gotype->id_name);
            ImGui::Text("pos: %s", format_pos(gotype->id_pos));
            break;
        case GOTYPE_SEL:
            ImGui::Text("package: %s", gotype->sel_name);
            ImGui::Text("sel: %s", gotype->sel_sel);
            break;
        case GOTYPE_MAP:
            render_gotype(gotype->map_key, "key");
            render_gotype(gotype->map_value, "value");
            break;
        case GOTYPE_STRUCT:
        case GOTYPE_INTERFACE:
            {
                auto specs = gotype->type == GOTYPE_STRUCT ? gotype->struct_specs : gotype->interface_specs;
                for (u32 i = 0; i < specs->len; i++) {
                    auto it = &specs->items[i];
                    if (ImGui::TreeNodeEx(it, flags, "spec %d", i)) {
                        ImGui::Text("is_embedded: %d", it->is_embedded);
                        ImGui::Text("tag: %s", it->tag);
                        if (it->is_embedded)
                            render_gotype(it->embedded_type);
                        else
                            render_godecl(it->field);
                        ImGui::TreePop();
                    }
                }
            }
            break;
        case GOTYPE_POINTER: render_gotype(gotype->pointer_base, "base"); break;
        case GOTYPE_SLICE: render_gotype(gotype->slice_base, "base"); break;
        case GOTYPE_ARRAY: render_gotype(gotype->array_base, "base"); break;
        case GOTYPE_LAZY_INDEX: render_gotype(gotype->lazy_index_base, "base"); break;
        case GOTYPE_LAZY_CALL: render_gotype(gotype->lazy_call_base, "base"); break;
        case GOTYPE_LAZY_DEREFERENCE: render_gotype(gotype->lazy_dereference_base, "base"); break;
        case GOTYPE_LAZY_REFERENCE: render_gotype(gotype->lazy_reference_base, "base"); break;
        case GOTYPE_LAZY_ARROW: render_gotype(gotype->lazy_arrow_base, "base"); break;
        case GOTYPE_VARIADIC: render_gotype(gotype->variadic_base, "base"); break;
        case GOTYPE_ASSERTION: render_gotype(gotype->assertion_base, "base"); break;

        case GOTYPE_CHAN:
            render_gotype(gotype->chan_base, "base"); break;
            ImGui::Text("direction: %d", gotype->chan_direction);
            break;

        case GOTYPE_FUNC:
            if (gotype->func_sig.params == NULL) {
                ImGui::Text("params: NULL");
            } else if (ImGui::TreeNodeEx(&gotype->func_sig.params, flags, "params:")) {
                For (*gotype->func_sig.params)
                    render_godecl(&it);
                ImGui::TreePop();
            }

            if (gotype->func_sig.result == NULL) {
                ImGui::Text("result: NULL");
            } else if (ImGui::TreeNodeEx(&gotype->func_sig.result, flags, "result:")) {
                For (*gotype->func_sig.result)
                    render_godecl(&it);
                ImGui::TreePop();
            }

            render_gotype(gotype->func_recv);
            break;

        case GOTYPE_MULTI:
            For (*gotype->multi_types) render_gotype(it);
            break;

        case GOTYPE_RANGE:
            render_gotype(gotype->range_base, "base");
            ImGui::Text("type: %d", gotype->range_type);
            break;

        case GOTYPE_LAZY_ID:
            ImGui::Text("name: %s", gotype->lazy_id_name);
            ImGui::Text("pos: %s", format_pos(gotype->lazy_id_pos));
            break;

        case GOTYPE_LAZY_SEL:
            render_gotype(gotype->lazy_sel_base, "base");
            ImGui::Text("sel: %s", gotype->lazy_sel_sel);
            break;

        case GOTYPE_LAZY_ONE_OF_MULTI:
            render_gotype(gotype->lazy_one_of_multi_base, "base");
            ImGui::Text("index: %d", gotype->lazy_one_of_multi_index);
            break;
        }
        ImGui::TreePop();
    }
}

void render_ts_cursor(TSTreeCursor *curr) {
    int last_depth = 0;
    bool last_open = false;

    auto pop = [&](int new_depth) {
        if (new_depth > last_depth) return;

        if (last_open)
            ImGui::TreePop();
        for (i32 i = 0; i < last_depth - new_depth; i++)
            ImGui::TreePop();
    };

    walk_ts_cursor(curr, false, [&](Ast_Node *node, Ts_Field_Type field_type, int depth) -> Walk_Action {
        if (node->anon && !world.wnd_ast_vis.show_anon_nodes)
            return WALK_SKIP_CHILDREN;

        // auto changed = ts_node_has_changes(node->node);

        pop(depth);
        last_depth = depth;

        auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

        auto type_str = ts_ast_type_str(node->type);
        if (type_str == NULL)
            type_str = "(unknown)";
        else
            type_str += strlen("TS_");

        auto field_type_str = ts_field_type_str(field_type);
        if (field_type_str == NULL)
            last_open = ImGui::TreeNodeEx(
                node->id,
                flags,
                "%s, start = %s, end = %s",
                type_str,
                format_pos(node->start),
                format_pos(node->end)
            );
        else
            last_open = ImGui::TreeNodeEx(
                node->id,
                flags,
                "(%s) %s, start = %s, end = %s",
                field_type_str + strlen("TSF_"),
                type_str,
                format_pos(node->start),
                format_pos(node->end)
            );

        if (ImGui::IsItemClicked()) {
            auto editor = world.get_current_editor();
            if (editor != NULL)
                editor->move_cursor(node->start);
        }

        return last_open ? WALK_CONTINUE : WALK_SKIP_CHILDREN;
    });

    pop(0);
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
    proc->dir = world.current_path;
    proc->run(cmd);
}

void filter_files() {
    auto wnd = &world.wnd_open_file;

    wnd->filtered_results->len = 0;

    u32 i = 0;
    For (*wnd->filepaths) {
        if (fzy_has_match(wnd->query, it))
            wnd->filtered_results->append(i);
        i++;
    }

    wnd->filtered_results->sort([&](int *ia, int *ib) {
        auto a = fzy_match(wnd->query, wnd->filepaths->at(*ia));
        auto b = fzy_match(wnd->query, wnd->filepaths->at(*ib));
        return a < b ? 1 : (a > b ? -1 : 0);  // reverse
    });
}

void init_open_file() {
    SCOPED_MEM(&world.open_file_mem);
    world.open_file_mem.reset();

    auto wnd = &world.wnd_open_file;
    ptr0(wnd);

    wnd->show = true;
    wnd->filepaths = alloc_list<ccstr>();
    wnd->filtered_results = alloc_list<int>();

    fn<void(ccstr)> fill_files = [&](ccstr path) {
        auto thispath = path_join(world.current_path, path);
        list_directory(thispath, [&](Dir_Entry *entry) {
            bool isdir = (entry->type == DIRENT_DIR);

            auto fullpath = path_join(thispath, entry->name);
            if (is_ignored_by_git(fullpath, isdir)) return;

            if (isdir && streq(path, "") && streq(entry->name, ".ide")) return;

            auto relpath = path[0] == '\0' ? our_strcpy(entry->name) : path_join(path, entry->name);
            if (isdir)
                fill_files(relpath);
            else
                wnd->filepaths->append(relpath);
        });
    };

    fill_files("");
}

enum {
    OUR_MOD_NONE = 0,
    OUR_MOD_CMD = 1 << 0,
    OUR_MOD_SHIFT = 1 << 1,
    OUR_MOD_ALT = 1 << 2,
    OUR_MOD_CTRL = 1 << 3,
};

void init_with_file_at_location(ccstr path, cur2 cur) {
    SCOPED_FRAME();

    world.get_current_pane()->focus_editor(path);

    auto editor = world.get_current_editor();
    while (!editor->is_nvim_ready()) continue;

    world.get_current_editor()->move_cursor(cur);
}

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

void* get_native_window_handle(GLFWwindow *window) {
#if OS_WIN
    return (void*)glfwGetWin32Window(window);
#endif
}

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

    t.log("window hints");

    world.window = glfwCreateWindow(420, 420, WINDOW_TITLE, NULL, NULL);
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

    // window sizing crap
    glfwSetWindowSize(world.window, 1280, 720);
    glfwSetWindowPos(world.window, 50, 50);

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

        world.ui.im_font_ui = io.Fonts->AddFontFromFileTTF("FiraSans-Regular.ttf", UI_FONT_SIZE);
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

        if (ui.get_sidebar_area().contains(world.ui.mouse_pos) && world.sidebar.view != SIDEBAR_CLOSED) {
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

        if (ui.get_build_results_area().contains(world.ui.mouse_pos)) {
            add_dy(&world.wnd_build_and_debug.scroll_offset);
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
                    auto pane = world.get_current_pane();

                    pane->focus_editor(filepath);
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

        // TODO: all these operations that add or remove a single character in
        // buf could definitely be optimized, I believe right now even for
        // single char changes, it destroys the whole line, creates a new line,
        // and copies it over.

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
                            editor->buf.insert(editor->cur, text, len);
                            editor->end_change();

                            auto cur = editor->cur;
                            for (u32 i = 0; i < len; i++)
                                cur = editor->buf.inc_cur(cur);

                            editor->raw_move_cursor(cur);
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
                                target = world.get_current_pane()->focus_editor(result->file);

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
                        if (world.sidebar.view == SIDEBAR_FILE_EXPLORER)
                            world.sidebar.view = SIDEBAR_CLOSED;
                        else
                            world.sidebar.view = SIDEBAR_FILE_EXPLORER;
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
                            {
                                Dbg_Call call;
                                call.type = DBGCALL_START;
                                if (!world.dbg.call_queue.push(&call)) {
                                    // TODO: surface error
                                }
                            }
                            break;
                        }
                        break;
                    case GLFW_KEY_F9:
                        {
                            if (editor == NULL) break;

                            ccstr file = editor->filepath;
                            auto lineno = editor->cur.y + 1;

                            auto &dbg = world.dbg;

                            auto bkpt = dbg.breakpoints.find([&](Client_Breakpoint *it) -> bool {
                                return are_breakpoints_same(file, lineno, it->file, it->line);
                            });

                            if (bkpt == NULL) {
                                auto it = dbg.breakpoints.append();
                                it->file = file;
                                it->line = lineno;
                                it->pending = true;

                                if (dbg.state_flag != DBGSTATE_INACTIVE) {
                                    Dbg_Call call;
                                    call.type = DBGCALL_SET_BREAKPOINT;
                                    call.set_breakpoint.filename = file;
                                    call.set_breakpoint.lineno = lineno;

                                    if (!dbg.call_queue.push(&call)) {
                                        // TODO: surface error
                                    }
                                }
                            } else {
                                dbg.breakpoints.remove(bkpt);
                                if (dbg.state_flag != DBGSTATE_INACTIVE) {
                                    dbg.unset_breakpoint(file, lineno);

                                    Dbg_Call call;
                                    call.type = DBGCALL_UNSET_BREAKPOINT;
                                    call.unset_breakpoint.filename = file;
                                    call.unset_breakpoint.lineno = lineno;

                                    if (!dbg.call_queue.push(&call)) {
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
                                ac.ac.results = NULL;

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

    // init_with_file_at_location(path_join(world.current_path, "sync/sync.go"), new_cur2(10, 11));

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
        }

        glDisable(GL_SCISSOR_TEST);
        glClearColor(COLOR_WHITE.r, COLOR_WHITE.g, COLOR_WHITE.b, 1.0);
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
                if (io.MouseDrawCursor || cur == ImGuiMouseCursor_None) {
                    glfwSetInputMode(world.window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
                } else {
                    glfwSetInputMode(world.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    glfwSetCursor(world.window, world.ui.cursors[cur] ? world.ui.cursors[cur] : world.ui.cursors[ImGuiMouseCursor_Arrow]);
                }
            }
        }

        // start rendering imgui
        ImGui::NewFrame();

        // draw menubar first, so we can get its height (which we need for UI)
        if (ImGui::BeginMainMenuBar()) {
            world.ui.menubar_height = ImGui::GetWindowSize().y;

            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open file...", "Ctrl+P")) {
                    if (!world.wnd_open_file.show) {
                        world.wnd_open_file.show = true;
                        init_open_file();
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit...", "Alt+F4")) {
                    glfwSetWindowShouldClose(world.window, true);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Project")) {
                if (ImGui::MenuItem("Build", "Ctrl+Shift+B")) {
                    kick_off_build();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Run", "F5")) {
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
                        {
                            Dbg_Call call;
                            call.type = DBGCALL_START;
                            if (!world.dbg.call_queue.push(&call)) {
                                // TODO: surface error
                            }
                        }
                        break;
                    }
                }

                if (ImGui::MenuItem("Break All")) {
                }

                if (ImGui::MenuItem("Stop Debugging", "Shift+F5")) {
                }

                if (ImGui::MenuItem("Step Over", "F10")) {
                }

                if (ImGui::MenuItem("Step Into", "F11")) {
                }

                if (ImGui::MenuItem("Step Out", "Shift+F11")) {
                }

                if (ImGui::MenuItem("Run to Cursor", "Shift+F10")) {
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Settings")) {
                    world.windows_open.settings = true;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Developer")) {
                ImGui::MenuItem("ImGui demo", NULL, &world.windows_open.im_demo);
                ImGui::MenuItem("ImGui metrics", NULL, &world.windows_open.im_metrics);
                ImGui::MenuItem("Editor AST viewer", NULL, &world.wnd_editor_tree.show);
                ImGui::MenuItem("Editor toplevels viewer", NULL, &world.wnd_editor_toplevels.show);
                ImGui::MenuItem("Roll Your Own IDE Construction Set", NULL, &world.wnd_style_editor.show);
                ImGui::MenuItem("Replace line numbers with bytecounts", NULL, &world.replace_line_numbers_with_bytecounts);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Go")) {
                if (ImGui::MenuItem("Reload go.mod")) {
                    atomic_set_flag(
                        &world.indexer.flag_lock,
                        &world.indexer.flag_handle_gomod_changed
                    );
                }

                if (ImGui::MenuItem("Re-index everything")) {
                    atomic_set_flag(
                        &world.indexer.flag_lock,
                        &world.indexer.flag_reindex_everything
                    );
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        {
            if (world.windows_open.settings) {
                ImGui::Begin("Project Settings", &world.windows_open.settings, ImGuiWindowFlags_AlwaysAutoResize);
                if (ImGui::IsWindowFocused)

                ImGui::InputText("Build command", world.settings.build_command, _countof(world.settings.build_command));
                ImGui::InputText("Debug binary path", world.settings.debug_binary_path, _countof(world.settings.debug_binary_path));

                ImGui::End();
            }

            if (world.windows_open.im_demo)
                ImGui::ShowDemoWindow(&world.windows_open.im_demo);

            if (world.windows_open.im_metrics)
                ImGui::ShowMetricsWindow(&world.windows_open.im_metrics);

            if (world.wnd_open_file.show) {
                auto& wnd = world.wnd_open_file;
                ImGui::Begin("Open File", &world.wnd_open_file.show, ImGuiWindowFlags_AlwaysAutoResize);

                wnd.focused = ImGui::IsWindowFocused();

                ImGui::Text("Search for file:");

                ImGui::PushFont(world.ui.im_font_mono);

                ImGui::InputText("##search_for_file", wnd.query, _countof(wnd.query));
                if (ImGui::IsWindowAppearing())
                    ImGui::SetKeyboardFocusHere();
                if (ImGui::IsItemEdited())
                    filter_files();

                for (u32 i = 0; i < wnd.filtered_results->len; i++) {
                    auto it = wnd.filepaths->at(wnd.filtered_results->at(i));
                    if (i == wnd.selection)
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", it);
                    else
                        ImGui::Text("%s", it);
                }

                ImGui::PopFont();

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

                        if (ImGui::CollapsingHeader("Local Variables", ImGuiTreeNodeFlags_DefaultOpen)) {
                            if (world.wnd_debugger.current_location != -1) {
                                auto& loc = world.dbg.state.stackframe->at(world.wnd_debugger.current_location);
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

                        if (ImGui::CollapsingHeader("Arguments", ImGuiTreeNodeFlags_DefaultOpen)) {
                            if (world.wnd_debugger.current_location != -1) {
                                auto& loc = world.dbg.state.stackframe->at(world.wnd_debugger.current_location);
                                if (loc.args != NULL) {
                                    For (*loc.args)
                                        render_var(&it);
                                } else {
                                    ImGui::Text("This location has no arguments.");
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
                                ImGui::OpenPopup("Add Watch");
                            }

                            if (ImGui::BeginPopupModal("Add Watch", &world.wnd_debugger.show_add_watch, ImGuiWindowFlags_AlwaysAutoResize)) {
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
                    world.search_results.mem.cleanup();
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

            if (world.wnd_add_file_or_folder.show) {
                auto &wnd = world.wnd_add_file_or_folder;

                auto label = our_sprintf(
                    "Add %s to %s",
                    wnd.folder ? "folder" : "file",
                    wnd.location_is_root ? "workspace root" : wnd.location
                );

                ImGui::Begin(label, &wnd.show, ImGuiWindowFlags_AlwaysAutoResize);

                ImGui::Text("Name:");
                ImGui::InputText("##add_file", wnd.name, IM_ARRAYSIZE(wnd.name));

                if (ImGui::Button("Add")) {
                    world.wnd_add_file_or_folder.show = false;

                    if (strlen(wnd.name) > 0) {
                        auto dest = wnd.location_is_root ? world.current_path : path_join(world.current_path, wnd.location);
                        auto path = path_join(dest, wnd.name);

                        if (wnd.folder) {
                            CreateDirectoryA(path, NULL);
                        } else {
                            // need to share, or else we have race condition
                            // with fsevent handler in
                            // Go_Indexer::background_thread() trying to read
                            auto h = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
                            if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
                        }

                        world.fill_file_tree();
                    }
                }
                ImGui::End();
            }

            if (world.wnd_style_editor.show) {
                ImGui::Begin("Style Editor", &world.wnd_style_editor.show, ImGuiWindowFlags_AlwaysAutoResize);

                ImGui::SliderFloat("status_padding_x", &settings.status_padding_x, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("status_padding_y", &settings.status_padding_y, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("line_number_margin_left", &settings.line_number_margin_left, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("line_number_margin_right", &settings.line_number_margin_right, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("sidebar_padding_x", &settings.sidebar_padding_x, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("sidebar_padding_y", &settings.sidebar_padding_y, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("sidebar_item_padding_x", &settings.sidebar_item_padding_x, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("sidebar_item_padding_y", &settings.sidebar_item_padding_y, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("filetree_item_margin", &settings.filetree_item_margin, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("filetree_item_padding_y", &settings.filetree_item_padding_y, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("filetree_space_between_items", &settings.filetree_space_between_items, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("filetree_button_size", &settings.filetree_button_size, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("filetree_buttons_area_padding_x", &settings.filetree_buttons_area_padding_x, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("filetree_buttons_area_padding_y", &settings.filetree_buttons_area_padding_y, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("filetree_button_margin_x", &settings.filetree_button_margin_x, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("filetree_button_padding", &settings.filetree_button_padding, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("autocomplete_menu_padding", &settings.autocomplete_menu_padding, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("autocomplete_item_padding_x", &settings.autocomplete_item_padding_x, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("autocomplete_item_padding_y", &settings.autocomplete_item_padding_y, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("error_list_item_padding_x", &settings.error_list_item_padding_x, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("error_list_item_padding_y", &settings.error_list_item_padding_y, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("tabs_offset", &settings.tabs_offset, 0.0, 20.0f, "%.0f");
                ImGui::SliderFloat("scrolloff", &settings.scrolloff, 0.0, 20.0f, "%.0f");

                ImGui::End();
            }

            do {
                auto editor = world.get_current_editor();
                if (editor == NULL) break;

                auto tree = editor->tree;
                if (tree == NULL) break;

                if (world.wnd_editor_tree.show) {
                    ImGui::Begin("AST", &world.wnd_editor_tree.show, 0);
                    ImGui::Checkbox("show anon?", &world.wnd_ast_vis.show_anon_nodes);
                    ts_tree_cursor_reset(&editor->cursor, ts_tree_root_node(tree));
                    render_ts_cursor(&editor->cursor);
                    ImGui::End();
                }

                if (world.wnd_editor_toplevels.show) {
                    ImGui::Begin("Toplevels", &world.wnd_editor_toplevels.show, 0);

                    List<Godecl> decls;
                    decls.init();

                    Parser_It it = {0};
                    it.init(&editor->buf);

                    Ast_Node node = {0};
                    node.init(ts_tree_root_node(tree), &it);

                    FOR_NODE_CHILDREN (&node) {
                        switch (it->type) {
                        case TS_VAR_DECLARATION:
                        case TS_CONST_DECLARATION:
                        case TS_FUNCTION_DECLARATION:
                        case TS_METHOD_DECLARATION:
                        case TS_TYPE_DECLARATION:
                        case TS_SHORT_VAR_DECLARATION:
                            decls.len = 0;
                            world.indexer.node_to_decls(it, &decls, NULL);
                            For (decls) render_godecl(&it);
                            break;
                        }
                    }

                    ImGui::End();
                }
            } while (0);

            world.ui.mouse_captured_by_imgui = io.WantCaptureMouse;
            world.ui.keyboard_captured_by_imgui = io.WantCaptureKeyboard || ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);

            ImGui::Render();
        }

        ui.draw_everything(vao, vbo, world.ui.program);

        {
            // draw imgui buffers
            ImDrawData* draw_data = ImGui::GetDrawData();
            draw_data->ScaleClipRects(ImVec2(world.display_scale.x, world.display_scale.y));

            glViewport(0, 0, world.display_size.x, world.display_size.y);
            glUseProgram(world.ui.im_program);
            glBindVertexArray(im_vao);
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
            auto remaining = (1000000000.f / FRAME_RATE_CAP) - (curr - last_frame_time);
            if (remaining > 0)
                sleep_milliseconds((u32)(remaining / 1000000.0f));
            last_frame_time = curr;
        }
    }

    return EXIT_SUCCESS;
}
