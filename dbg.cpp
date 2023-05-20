/*
when we get a new state
    state contains the "current goroutine" if there is one
    list all goroutines
    if there is a "current goroutine," open it

when a goroutine is opened
    call stacktrace and save frames

when a frame is opened
    list local vars
    list function arguments

3: let's users specify a --log-output parameter
9. displays a warning when stepping through a stale executable
10. setting a breakpoint on a function should result in the breakpoint being after the prologue
20. the debugger should run fast even in presence of very deep stacks, a large number of goroutines or very large variables
*/

#include "defer.hpp"
#include "dbg.hpp"

#include "world.hpp"
#include "os.hpp"
#include "utils.hpp"
#include "go.hpp"

#include <stdio.h>
#include <stdlib.h>

#if OS_WINBLOWS
#include "win32.hpp"
#elif OS_MAC || OS_LINUX
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <inttypes.h>

#define DBG_DEBUG 0

#if DBG_DEBUG
#define dbg_print(fmt, ...) print("[dbg] " fmt, ##__VA_ARGS__)
#else
#define dbg_print(fmt, ...)
#endif

bool Dlv_Var::incomplete() {
    auto children_len = [&]() -> int { return !children ? 0 : children->len; };

    switch (kind) {
    case GO_KIND_STRING:
        return len > strlen(value);
    case GO_KIND_ARRAY:
    case GO_KIND_SLICE:
    case GO_KIND_STRUCT:
        return len > children_len();
    case GO_KIND_MAP:
        return len > children_len() / 2;
    case GO_KIND_INTERFACE:
        return children_len() > 0 && children->at(0)->only_addr;
    }

    return false;
}

Json_Navigator Packet::js() {
    Json_Navigator ret;
    ret.string = string;
    ret.tokens = tokens;
    return ret;
}

void Debugger::push_call(Dlv_Call_Type type) {
    push_call(type, [&](Dlv_Call*) {});
}

void Debugger::push_call(Dlv_Call_Type type, fn<void(Dlv_Call *call)> f) {
    SCOPED_LOCK(&calls_lock);
    SCOPED_MEM(&calls_mem);

    auto call = calls.append();
    call->type = type;
    f(call);
}

bool Debugger::find_breakpoint(ccstr filename, u32 line, Breakpoint* out) {
    SCOPED_FRAME();

    auto breakpoints = list_breakpoints();
    if (!breakpoints)
        return false;

    For (breakpoints)
        if (are_filepaths_equal(it.file, filename))
            if (it.line == line)
                return memcpy(out, &it, sizeof(Breakpoint)), true;

    return false;
}

void Debugger::save_list_of_vars(Json_Navigator js, i32 idx, List<Dlv_Var*> *out) {
    auto len = js.array_length(idx);
    for (u32 i = 0; i < len; i++) {
        auto ptr = new_object(Dlv_Var);
        save_single_var(js, js.get(idx, i), ptr);
        out->append(ptr);
    }
}

ccstr parse_json_string(ccstr s) {
    Frame frame;

    u32 i = 0;
    u32 len = strlen(s);

    List<char> chars;
    chars.init(LIST_POOL, len);

    while (i < len) {
        if (s[i] != '\\') {
            chars.append(s[i++]);
            continue;
        }

        if (i+2 > len) goto fail;

        int seqlen = 2;

        switch (s[i+1]) {
        case 'b': chars.append('\b'); break;
        case 'f': chars.append('\f'); break;
        case 'n': chars.append('\n'); break;
        case 'r': chars.append('\r'); break;
        case 't': chars.append('\t'); break;
        case '\"': chars.append('\"'); break;
        case '\\': chars.append('\\'); break;
        case '/': chars.append('/'); break;
        case 'u': {
            if (i+6 > len) goto fail;

            auto parse_hex4 = [&](ccstr s, bool *ok) -> u32 {
                u32 h = 0;
                for (u32 i = 0; i < 4; i++) {
                    if (i) h = h << 4;

                    if ((s[i] >= '0') && (s[i] <= '9'))
                        h += (u32)s[i] - '0';
                    else if ((s[i] >= 'A') && (s[i] <= 'F'))
                        h += (u32)10 + s[i] - 'A';
                    else if ((s[i] >= 'a') && (s[i] <= 'f'))
                        h += (u32)10 + s[i] - 'a';
                    else {
                        *ok = false;
                        return 0;
                    }
                }

                *ok = true;
                return h;
            };

            bool ok = false;

            auto codepoint = parse_hex4(&s[i+2], &ok);
            if (!ok) goto fail;
            if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) goto fail;

            seqlen = 6;
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                auto j = i+6;
                if (j+6 > len) goto fail;
                if (s[j] != '\\' || s[j+1] != 'u') goto fail;

                auto second = parse_hex4(&s[j+2], &ok);
                if (!ok) goto fail;
                if (second < 0xDC00 || second > 0xDFFF) goto fail;

                codepoint = 0x10000 + (((codepoint & 0x3FF) << 10) | (second & 0x3FF));
                seqlen = 12;
            }

            char utf8_chars[4];
            auto utf8_len = uchar_to_cstr(codepoint, utf8_chars);
            if (!utf8_len) goto fail;
            for (u32 j = 0; j < utf8_len; j++)
                chars.append(utf8_chars[j]);
            break;
        }
        default:
            goto fail;
        }

        i += seqlen;
    }

    chars.append('\0');
    return chars.items;

fail:
    frame.restore();
    return NULL;
}

void Debugger::save_single_var(Json_Navigator js, i32 idx, Dlv_Var* out, Save_Var_Mode save_mode) {
    if (save_mode == SAVE_VAR_NORMAL) {
        out->name = js.str(js.get(idx, ".name"));
        out->value = parse_json_string(js.str(js.get(idx, ".value")));
        out->kind = (Go_Reflect_Kind)js.num(js.get(idx, ".kind"));
        out->len = js.num(js.get(idx, ".len"));
        out->cap = js.num(js.get(idx, ".cap"));
        out->address = js.num_u64(js.get(idx, ".addr"));
        out->type = js.str(js.get(idx, ".type"));
        out->type = str_replace(out->type, "\\u003c", "<");
        out->real_type = js.str(js.get(idx, ".realType"));
        out->flags = js.num(js.get(idx, ".flags"));
        out->only_addr = js.boolean(js.get(idx, ".onlyAddr"));

        auto unreadable = js.str(js.get(idx, ".unreadable"));
        if (unreadable && unreadable[0] != '\0')
            out->unreadable_description = unreadable;
    }

    if (save_mode == SAVE_VAR_VALUE_APPEND) {
        out->value = cp_sprintf("%s%s", out->value, js.str(js.get(idx, ".value")));
    } else {
        if (save_mode == SAVE_VAR_NORMAL || save_mode == SAVE_VAR_CHILDREN_OVERWRITE)
            out->children = new_list(Dlv_Var*);
        save_list_of_vars(js, js.get(idx, ".children"), out->children);
    }
}

void Debugger::eval_watches() {
    For (&watches) eval_watch(&it);
}

void Debugger::eval_watch(Dlv_Watch *watch) {
    // TODO: is there any difference between these two flags?
    watch->fresh = false;
    watch->state = DBGWATCH_PENDING;

    int goroutine_id = state->current_goroutine_id;
    int frame_id = state->current_frame;

    Dlv_Var value; ptr0(&value);
    if (eval_expression(watch->expr, goroutine_id, frame_id, &value, SAVE_VAR_NORMAL)) {
        SCOPED_MEM(&watches_mem);
        watch->value = new_object(Dlv_Var);
        memcpy(watch->value, value.copy(), sizeof(Dlv_Var));
        watch->state = DBGWATCH_READY;
    } else {
        SCOPED_MEM(&watches_mem);
        watch->value = new_object(Dlv_Var);
        watch->value->flags = DLV_VAR_CANTREAD;
        watch->state = DBGWATCH_ERROR;
    }

    watch->fresh = true;
}

bool Debugger::eval_expression(ccstr expression, i32 goroutine_id, i32 frame_id, Dlv_Var* out, Save_Var_Mode save_mode) {
    if (goroutine_id == -1) {
        auto resp = send_packet("State", [&]() {});
        auto js = resp->js();

        auto idx = js.get(0, ".result.State.currentThread.goroutineID");
        if (idx == -1) return false;

        idx = js.num(idx);
        if (idx == -1) return false;
    }

    auto resp = send_packet("Eval", [&]() {
        rend->field("Expr", expression);
        rend->field("Scope", [&]() {
            rend->obj([&]() {
                rend->field("goroutineID", goroutine_id);
                rend->field("frame", frame_id);
            });
        });
        rend->field("Cfg", [&]() {
            rend->obj([&]() {
                rend->field("followPointers", true);
                rend->field("maxVariableRecurse", 1);
                rend->field("maxStringLen", 64);
                rend->field("maxArrayValues", 64);
                rend->field("maxStructFields", -1);
            });
        });
    });

    auto js = resp->js();

    auto variable_idx = js.get(0, ".result.Variable");
    if (variable_idx == -1) return false;

    save_single_var(js, variable_idx, out, save_mode);
    return true;
}

bool Debugger::can_read() {
    if (read_buffer_ptr < read_buffer_len) return true;

    char ret = 0;
    return recv(conn, (char*)&ret, 1, MSG_PEEK) == 1;
}

u8 Debugger::read1() {
    if (read_buffer_ptr >= read_buffer_len) {
        read_buffer_ptr = 0;
        read_buffer_len = 0;

        int read = recv(conn, read_buffer, _countof(read_buffer), 0);
        if (read == -1) return 0;

        read_buffer_len = read;
    }

    return read_buffer[read_buffer_ptr++];
}

bool Debugger::write1(u8 ch) {
    return (send(conn, (char*)&ch, 1, 0) == 1);
}

#if 0
bool Debugger::read_stdin_until(char want, ccstr* pret) {
    Text_Renderer r;
    Frame frame;

    r.init();
    for (char c; headless_proc.read1(&c);) {
        if (c == want) {
            if (pret) *pret = r.finish();
            return true;
        }
        if (pret) r.writechar(c);
    }

    frame.restore();
    return false;
}
#endif

void Debugger::init() {
    ptr0(this);

    mem.init("dbg");
    loop_mem.init("dbg loop");
    state_mem_a.init("dbg state a");
    state_mem_b.init("dbg state b");
    breakpoints_mem.init("dbg breakpoints");
    watches_mem.init("dbg watches");
    stdout_mem.init("dbg stdout");

    SCOPED_MEM(&mem);

    stdout_line_buffer.init();
    lock.init();
    breakpoints.init();
    watches.init();

    calls.init();
    calls_lock.init();
    calls_mem.init("dbg calls");

#if OS_WINBLOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        cp_panic("WSAStartup failed");
#endif
}

void Debugger::cleanup() {
    lock.cleanup();

    if (thread) {
        kill_thread(thread);
        close_thread_handle(thread);
    }

#if OS_WINBLOWS
    WSACleanup();
#endif

    calls.cleanup();
    calls_lock.cleanup();
    calls_mem.cleanup();

    mem.cleanup();
    loop_mem.cleanup();
}

#if OS_WINBLOWS
#define ioctl_stub ioctlsocket
#define close_stub closesocket
#else
#define ioctl_stub ioctl
#define close_stub close
#endif

Packet* Debugger::send_packet(ccstr packet_name, lambda f, bool read) {
    Timer t; t.init("send_packet", step_over_time);

    {
        SCOPED_FRAME();

        Json_Renderer r;
        rend = &r;
        r.init();
        r.obj([&]() {
            r.field("method", [&]() { r.write("\"RPCServer.%s\"", packet_name); });
            r.field("params", [&]() { r.arr([&]() { r.obj(f); }); });
            r.field("id", packetid++);
        });

        auto data = r.finish();
		dbg_print("[\"send\"]\n%s", data);

        for (u32 i = 0; data[i] != '\0'; i++)
            write1(data[i]);
        write1('\n');
    }

    t.log("write shit out");

    if (!read) return NULL;

    Packet p;
    if (!read_packet(&p)) {
        error("couldn't read packet");
        return NULL;
    }

    t.log("read packet");

    auto packet = new_object(Packet);
    memcpy(packet, &p, sizeof(p));
    return packet;
}

bool Debugger::read_packet(Packet* p) {
    Timer t; t.init("read_packet", step_over_time);

    u_long mode_blocking = 0;
    ioctl_stub(conn, FIONBIO, &mode_blocking);

    defer {
        u_long mode_nonblocking = 1;
        ioctl_stub(conn, FIONBIO, &mode_nonblocking);
    };

    auto read = [&]() -> ccstr {
        Frame frame;
        Text_Renderer r;
        r.init();

        for (char c; (c = read1()) != 0;) {
            if (c == '\n') return r.finish();
            r.writechar(c);
        }

        frame.restore();
        return NULL;
    };

    auto run = [&]() -> bool {
        auto s = read();
        if (!s) {
            dbg_print("recv.run: !s");
            return false;
        }

        t.log("read from stream");

        Json_Navigator js;
        if (!js.parse(s)) {
            dbg_print("recv.run: js.parse returned false");
            dbg_print("%s", s);
            return false;
        }

        t.log("parse json");

        p->string = js.string;
        p->tokens = js.tokens;
        return true;
    };

    dbg_print("running");

    bool ok = run();
    t.log("run");

    if (ok) {
        SCOPED_FRAME();
		dbg_print("[\"recv\"]\n%s", p->string); // cp_format_json(p->string));
        return true;
    }

	dbg_print("[\"recv\"]\n\"<error: unable to read packet>\"");
    return false;
}

void Debugger::halt_when_already_running() {
    exec_halt(true);

    // Since we were already running, when we call halt, we're going to get
    // two State responses, one for the original command that made us run
    // in the first place, and one from the halt. Swallow the first response.

    Packet p; ptr0(&p);
    if (read_packet(&p))
        handle_new_state(&p);
    else
        state_flag = DLV_STATE_PAUSED;
}

void Debugger::pause_and_resume(fn<void()> f) {
    bool was_running = (state_flag == DLV_STATE_RUNNING);

    if (was_running) {
        exec_halt(true);
        Packet p; ptr0(&p);
        read_packet(&p);
    }

    f();

    if (was_running) {
        exec_continue(false);
    }
}

Packet* Debugger::set_breakpoint(ccstr filename, u32 lineno) {
    Packet *ret = NULL;

    pause_and_resume([&]() {
        ret = send_packet("CreateBreakpoint", [&]() {
            rend->field("Breakpoint", [&]() {
                rend->obj([&]() {
                    rend->field("file", filename);
                    rend->field("line", (int)lineno);
                });
            });
        });
    });

    return ret;
}

bool are_breakpoints_same(ccstr file1, u32 line1, ccstr file2, u32 line2) {
    return are_filepaths_equal(file1, file2) && (line1 == line2);
}

bool Debugger::unset_breakpoint(int id) {
    pause_and_resume([&]() {
        send_packet("ClearBreakpoint", [&]() {
            rend->field("Id", id);
        });
    });
    return true;
}

void Debugger::send_command(ccstr command, bool read, int goroutine_id) {
    SCOPED_FRAME();
    send_packet("Command", [&]() {
        rend->field("name", command);
        // rend->field("threadID", -1);
        if (goroutine_id != -1)
            rend->field("goroutineID", goroutine_id);
    }, read);
}

void Debugger::exec_continue(bool read) { send_command("continue", read); }
void Debugger::exec_step_into(bool read) { send_command("step", read); }
void Debugger::exec_step_out(bool read) { send_command("stepOut", read); }
void Debugger::exec_step_over(bool read) { send_command("next", read); }
void Debugger::exec_halt(bool read) { send_command("halt", read); }

List<Breakpoint>* Debugger::list_breakpoints() {
    auto resp = send_packet("ListBreakpoints", [&]() {});

    auto js = resp->js();

    auto breakpoints_idx = js.get(0, ".result.Breakpoints");
    auto breakpoints_len = js.array_length(breakpoints_idx);
    auto ret = new_list(Breakpoint, breakpoints_len);

    for (i32 i = 0; i < breakpoints_len; i++) {
        auto it = js.get(breakpoints_idx, i);

        auto id = js.num(js.get(it, ".id"));
        if (id < 0) continue;

        auto wksp_path = normalize_path_sep(world.current_path);
        auto full_path = normalize_path_sep(js.str(js.get(it, ".file")));
        auto relative_path = get_path_relative_to(full_path, wksp_path);

        auto bp = ret->append();
        ptr0(bp);

        bp->id = id;
        bp->file = relative_path;
        bp->name = js.str(js.get(it, ".name"));
        bp->line = js.num(js.get(it, ".line"));
        bp->function_name = js.str(js.get(it, ".functionName"));
        bp->is_goroutine = js.boolean(js.get(it, ".goroutine"));
        bp->is_tracepoint = js.boolean(js.get(it, ".continue"));
        bp->is_at_return_in_traced_function = js.boolean(js.get(it, ".traceReturn"));
        bp->num_stackframes = js.num(js.get(it, ".stacktrace"));

        {
            auto idx = js.get(it, ".addrs");
            auto len = js.array_length(idx);

            bp->addrs = new_list(u64, js.array_length(idx));
            for (i32 j = 0; j < len; j++)
                bp->addrs->append(js.num(js.get(idx, j)));
        }

        {
            auto idx = js.get(it, ".variables");
            if (idx != -1) {
                auto len = js.array_length(idx);

                bp->variable_expressions = new_list(ccstr, len);
                for (i32 j = 0; j < len; j++)
                    bp->variable_expressions->append(js.str(js.get(idx, j)));
            }
        }
    }

    return ret;
}

bool Json_Navigator::parse(ccstr s) {
    Timer t; t.init("Json_Navigator::parse");

    string = s;
    tokens = new_list(jsmntok_t);

    jsmn_parser parser;
    jsmn_init(&parser);

    int len = strlen(s);
    t.log("get len");

    auto ret = jsmn_parse(&parser, s, len, tokens);
    t.log("parse");

    switch (ret) {
    case JSMN_ERROR_INVAL:
    case JSMN_ERROR_NOMEM:
    case JSMN_ERROR_PART:
        return false;
    }
    return true;
}

int Json_Navigator::advance_node(int i) {
    auto tok = tokens->at(i++);
    for (u32 j = 0; j < tok.size; j++)
        i = advance_node(i);
    return i;
}

bool Json_Navigator::match(int i, ccstr s) {
    SCOPED_FRAME();

    auto tok = tokens->at(i);
    if (tok.type != JSMN_STRING)
        return false;

    auto newlen = tok.end - tok.start;
    auto buf = new_array(char, newlen + 1);
    for (u32 i = 0; i < newlen; i++)
        buf[i] = string[tok.start + i];;
    buf[newlen] = '\0';

    return streq(buf, s);

    /*
    auto len = strlen(s);
    if (tok.end - tok.start != len)
        return false;

    for (int j = 0; j < len; j++)
        if (string[tok.start + j] != s[j])
            return false;
    return true;
    */
}

Json_Key Json_Navigator::key(int i) {
    Json_Key k;
    k.type = KEY_TYPE_ARR;
    k.arr_key = i;
    return k;
}

Json_Key Json_Navigator::key(ccstr s) {
    Json_Key k;
    k.type = KEY_TYPE_OBJ;
    k.obj_key = s;
    return k;
}

i32 Json_Navigator::array_length(i32 i) {
    auto tok = tokens->at(i);
    if (tok.type != JSMN_ARRAY)
        return -1;
    return tok.size;
}

i32 Json_Navigator::get(i32 i, i32 idx) {
    SCOPED_FRAME();
    return get(i, cp_sprintf("[%d]", idx));
}

i32 Json_Navigator::get(i32 i, ccstr keys) {
    u32 j = 0;
    char ch;

    while ((ch = keys[j++]) != '\0') {
        SCOPED_FRAME();

        if (ch == '[') {
            Text_Renderer r;
            r.init();
            for (char ch; (ch = keys[j++]) != ']';)
                r.writechar(ch);

            auto idx = atoi(r.finish());

            auto tok = tokens->at(i++);
            if (tok.type != JSMN_ARRAY) return -1;
            if (idx >= tok.size) return -1;

            for (i32 j = 0; j < idx; j++)
                i = advance_node(i);
            continue;
        }

        if (ch == '.') {
            auto is_delim = [&](char ch) -> bool {
                switch (ch) {
                case '.':
                case '[':
                case '\0':
                    return true;
                }
                return false;
            };

            Text_Renderer r;
            r.init();
            while (!is_delim(keys[j]))
                r.writechar(keys[j++]);
            auto key = r.finish();

            auto tok = tokens->at(i++);
            if (tok.type != JSMN_OBJECT) return -1;

            auto find = [&]() -> bool {
                for (i32 j = 0; j < tok.size; j++) {
                    if (match(i, key))
                        return (i++), true;
                    i = advance_node(i);
                }
                return false;
            };

            if (!find()) return -1;
            continue;
        }
        return -1;
    }
    return i;
}

ccstr Json_Navigator::str(i32 i, u32* plen) {
    if (i == -1) return NULL;

    auto tok = tokens->at(i);
    *plen = (tok.end - tok.start);
    return string + tok.start;
}

ccstr Json_Navigator::str(i32 i) {
    if (i == -1) return NULL;

    u32 len;
    auto s = str(i, &len);

    auto ret = new_array(char, len + 1);
    strncpy(ret, s, len);
    ret[len] = '\0';
    return ret;
}

u64 Json_Navigator::num_u64(i32 i) {
    if (i == -1) return 0;
    SCOPED_FRAME();
    auto s = str(i);
    return strtoull(s, NULL, 10);
}

i32 Json_Navigator::num(i32 i) {
    if (i == -1) return 0;
    SCOPED_FRAME();
    auto s = str(i);
    return atoi(s);
    // return !s ? 0 : atoi(s);
}

bool Json_Navigator::boolean(i32 i) {
    if (i == -1) return false;
    auto tok = tokens->at(i);
    if (tok.type == JSMN_PRIMITIVE)
        if (string[tok.start] == 't')
            return true;
    return false;
}

void Debugger::start_loop() {
    auto func = [](void *param) {
        auto dbg = (Debugger*)param;

        while (true) {
            SCOPED_MEM(&dbg->loop_mem);
            MEM->reset();

            dbg->do_everything();
        }
    };

    SCOPED_MEM(&mem);
    thread = create_thread(func, this);
}

void Debugger::surface_error(ccstr msg) {
    // TODO: surface some kind of error, how do?
}

bool Debugger::start(Debug_Profile *debug_profile) {
    conn = -1;

    /*
    to build test:

        go test -c ${import_path} -o ${binary_name} --gcflags=\"all=-N -l\"

    to build normally:

        go build -o ${binary_name} --gcflags=\"all=-N -l\"
    */

    ccstr binary_path = NULL;
    ccstr test_function_name = NULL;

    // TODO: we need to "lock" debug and build functions
    switch (debug_profile->type) {
    case DEBUG_TEST_PACKAGE:
    case DEBUG_TEST_CURRENT_FUNCTION:
    case DEBUG_RUN_PACKAGE: {
        auto use_current_package = [&]() -> bool {
            switch (debug_profile->type) {
            case DEBUG_TEST_CURRENT_FUNCTION:
                return true;
            case DEBUG_TEST_PACKAGE:
                return debug_profile->test_package.use_current_package;
            case DEBUG_RUN_PACKAGE:
                return debug_profile->run_package.use_current_package;
            }
            return false;
        };

        ccstr package_path = NULL;

        auto get_info_from_current_editor = [&]() {
            auto editor = get_current_editor();
            if (!editor) return;
            if (editor->lang != LANG_GO) return;

            if (debug_profile->type == DEBUG_TEST_CURRENT_FUNCTION)
                if (!str_ends_with(editor->filepath, "_test.go"))
                    return;

            // if (!path_has_descendant(world.current_path, editor->filepath)) return;

            if (!world.workspace) return;

            auto mod = world.workspace->find_module_containing_resolved(editor->filepath);
            if (!mod) return;

            auto subpath = get_path_relative_to(cp_dirname(editor->filepath), mod->resolved_path);
            package_path = normalize_path_sep(path_join(mod->import_path, subpath), '/');

            if (debug_profile->type == DEBUG_TEST_CURRENT_FUNCTION) {
                if (editor->buf->tree) {
                    Parser_It it;
                    it.init(editor->buf);
                    auto root_node = new_ast_node(ts_tree_root_node(editor->buf->tree), &it);

                    find_nodes_containing_pos(root_node, editor->cur, true, [&](auto it) -> Walk_Action {
                        if (it->type() == TS_SOURCE_FILE)
                            return WALK_CONTINUE;

                        if (it->type() == TS_FUNCTION_DECLARATION) {
                            auto name = it->field(TSF_NAME);
                            if (name) {
                                auto func_name = name->string();
                                if (str_starts_with(func_name, "Test"))
                                    test_function_name = func_name;
                            }
                        }

                        return WALK_ABORT;
                    });
                }
            }
        };

        if (use_current_package())
            get_info_from_current_editor();
        else if (debug_profile->type == DEBUG_TEST_PACKAGE)
            package_path = debug_profile->test_package.package_path;
        else if (debug_profile->type == DEBUG_RUN_PACKAGE)
            package_path = debug_profile->run_package.package_path;

        if (!package_path || package_path[0] == '\0')
            return false;

        if (debug_profile->type == DEBUG_TEST_CURRENT_FUNCTION)
            if (!test_function_name || test_function_name[0] == '\0')
                return false;

        ccstr binary_name = NULL;
#if OS_WINBLOWS
        binary_name = "__debug_bin.exe";
#else
        binary_name = "__debug_bin";
#endif

        ccstr cmd = NULL;
        if (debug_profile->type == DEBUG_RUN_PACKAGE)
            cmd = cp_sprintf("%s build -o %s --gcflags=\"all=-N -l\" %s", world.go_binary_path, binary_name, package_path);
        else
            cmd = cp_sprintf("%s test -c %s -o %s --gcflags=\"all=-N -l\"", world.go_binary_path, package_path, binary_name);

        Build_Profile build_profile; ptr0(&build_profile);
        cp_strcpy_fixed(build_profile.label, "temp");
        cp_strcpy_fixed(build_profile.cmd, cmd);

        world.error_list.show = true;
        world.error_list.cmd_focus = true;
        kick_off_build(&build_profile);
        while (!world.build.done) sleep_milli(10);

        sleep_milli(100);

        dbg_print("build completed");

        if (world.build.errors.len || world.build.build_itself_had_error) {
            dbg_print("returning false, errors.len = %d, build_itself_had_error = %d", world.build.errors.len, world.build.build_itself_had_error);
            return false;
        }

        binary_path = binary_name;
        break;
    }

    case DEBUG_RUN_BINARY:
        if (debug_profile->run_binary.binary_path[0] == '\0') {
            send_tell_user("Please enter a path to the binary you want to debug under Project -> Project Settings.", NULL);
            return false;
        }

        binary_path = debug_profile->run_binary.binary_path;
        break;
    }

    if (!binary_path || binary_path[0] == '\0') {
        send_tell_user("Unable to find binary to debug.", NULL); // probably be more specific later
        return false;
    }

    dlv_proc.init();
    // dlv_proc.dont_use_stdout = true;
    dlv_proc.dir = world.current_path;
    // dlv_proc.create_new_console = true;

    ccstr delve_path = NULL;
    {
        auto path = GHGetDelvePath();
        if (path) {
            delve_path = cp_strdup(path);
            defer { GHFree(path); };
        }
    }

    if (!delve_path || delve_path[0] == '\0') {
        dbg_print("delve path is empty");
        send_tell_user("Couldn't find Delve. Please make sure it's installed and accessible from a shell.", NULL);
        return false;
    }

    auto is_port_available = [&](int port) {
        auto fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) return false;
        defer { close_stub(fd); };

        struct sockaddr_in addr; ptr0(&addr);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(fd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) return false;

        return true;
    };

    auto find_available_port = [&]() -> int {
        for (int port = 1234; port <= 9999; port++)
            if (is_port_available(port))
                return port;
        return 0;
    };

    auto delve_port = find_available_port();
    if (!delve_port) {
        send_tell_user("We were unable to find a port to open the Delve debugger on.", "Unable to run Delve");
        return false;
    }

    ccstr dlv_cmd = cp_sprintf("%s exec --headless --listen=127.0.0.1:%d %s", delve_path, delve_port, binary_path);
    if (debug_profile->type == DEBUG_TEST_CURRENT_FUNCTION)
        dlv_cmd = cp_sprintf("%s -- -test.v -test.run %s", dlv_cmd, test_function_name);

#ifdef CPU_ARM64
        dlv_cmd = cp_sprintf("arch -arm64 %s", dlv_cmd);
#endif

    dbg_print("delve command: %s", dlv_cmd);
    dbg_print("getcwd = %s", cp_getcwd());
    dbg_print("dlv_proc.dir = %s", dlv_proc.dir);

    if (!dlv_proc.run(dlv_cmd)) {
        dbg_print("failed to run");
        return false;
    }

    {
        stdout_mem.cleanup();
        stdout_mem.init("stdout_mem");
        stdout_line_buffer.len = 0;

        {
            SCOPED_MEM(&stdout_mem);
            stdout_lines.init();
        }

        auto callback = [](void *param) {
            ((Debugger*)param)->pipe_stdout_into_our_buffer();
        };

        pipe_stdout_thread = create_thread(callback, this);
        if (!pipe_stdout_thread) return false;
    }

    // read the first line
    char ch = 0;
    do {
        if (!dlv_proc.read1(&ch)) return false;
    } while (ch != '\n');

    auto server = "127.0.0.1";
    auto delve_port_str = cp_sprintf("%d", delve_port);

    addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result;
    if (getaddrinfo(server, delve_port_str, &hints, &result) != 0)
        return error("unable to resolve server:port %s:%s", server, delve_port_str), false;

    auto make_connection = [&]() -> int {
        for (auto ptr = result; ptr; ptr = ptr->ai_next) {
            auto fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (fd == -1) return -1;

            u_long io_mode = 1; // non-blocking
            if (connect(fd, ptr->ai_addr, ptr->ai_addrlen) != -1)
                if (ioctl_stub(fd, FIONBIO, &io_mode) == 0)
                    return fd;

            dbg_print("unable to connect: %s", get_socket_error());
            close_stub(fd);
        }
        return -1;
    };

    for (u32 i = 0; i < 4; i++, sleep_milli(1000)) {
        conn = make_connection();
        if (conn != -1) break;
    }

    if (conn == -1)
        return error("unable to connect: %s", get_socket_error()), false;

    auto resp = send_packet("SetApiVersion", [&]() {
        rend->field("APIVersion", 2);
    });

    if (!resp)
        return error("unable to set API version"), false;

    auto get_pid = [&]() {
        resp = send_packet("ProcessPid", [&]() {});
        if (!resp) return false;

        auto js = resp->js();
        auto pid_idx = js.get(0, ".result.Pid");
        if (pid_idx == -1) return false;
        debuggee_pid = js.num(pid_idx);

        return true;
    };

    if (!get_pid())
        return error("unable to grab pidset API version"), false;

    world.wnd_debug_output.selection = -1;
    world.wnd_debug_output.show = true;
    world.wnd_debug_output.cmd_focus = true;
    dbg_print("got to the end");

    return true;
}

void Debugger::send_tell_user(ccstr text, ccstr title) {
    world.message_queue.add([&](auto msg) {
        msg->type = MTM_TELL_USER;
        msg->tell_user_text = cp_strdup(text);
        msg->tell_user_title = cp_strdup(title);
    });
}

// This is meant to be called from a separate thread. All it does is
// continuously stream all the data from dlv_proc stdout into our buffer.
void Debugger::pipe_stdout_into_our_buffer() {
    auto &p = dlv_proc;

    while (true) {
        if (!p.can_read()) {
            sleep_milli(50);
            continue;
        }

        char ch = 0;
        if (!p.read1(&ch)) break;
        if (!ch) break;

        if (ch == '\n') {
            stdout_line_buffer.append('\0');
            {
                SCOPED_MEM(&stdout_mem);
                stdout_lines.append(cp_strdup(stdout_line_buffer.items));
                world.wnd_debug_output.cmd_scroll_to_end = true;
            }
            stdout_line_buffer.len = 0;
        } else {
            stdout_line_buffer.append(ch);
        }
    }
}

void Debugger::stop() {
    if (conn && conn != -1)
        close_stub(conn);

    if (pipe_stdout_thread) {
        kill_thread(pipe_stdout_thread);
        pipe_stdout_thread = NULL;
    }

    // stdout_mem.cleanup();

    dlv_proc.cleanup();
}

void Debugger::mutate_state(fn<void(Debugger_State *draft)> cb) {
    auto draft = state->copy();
    cb(draft);

    which_state_pool ^= 1;
    Pool *mem = which_state_pool ? &state_mem_a : &state_mem_b;
    mem->reset();

    SCOPED_MEM(mem);
    state = draft->copy();
    state_id++;
};

Dlv_Goroutine *find_goroutine(Debugger_State *state) {
    int goroutine_id = state->current_goroutine_id;
    if (goroutine_id == -1) // halting
        goroutine_id = state->goroutines->at(0).id;

    return state->goroutines->find([&](auto it) { return it->id == goroutine_id; });
}

Dlv_Frame *find_frame(Dlv_Goroutine *goroutine, int frame_id) {
    if (frame_id >= goroutine->frames->len) return NULL;

    return &goroutine->frames->items[frame_id];
}

bool Debugger::fill_stacktrace(Debugger_State *draft) {
    auto goroutine = find_goroutine(draft);
    if (!goroutine) return false;

    do {
        if (goroutine->fresh) break;

        auto resp = send_packet("Stacktrace", [&]() {
            rend->field("id", (int)goroutine->id);
            rend->field("depth", 50);
            rend->field("full", false);
        });

        auto js = resp->js();

        auto locations_idx = js.get(0, ".result.Locations");
        if (locations_idx == -1) break;

        auto locations_len = js.array_length(locations_idx);

        goroutine->frames = new_list(Dlv_Frame);

        for (u32 i = 0; i < locations_len; i++) {
            auto it = js.get(locations_idx, i);
            auto frame = goroutine->frames->append();
            frame->filepath  = js.str(js.get(it, ".file"));
            frame->lineno    = js.num(js.get(it, ".line"));
            frame->func_name = js.str(js.get(it, ".function.name"));
            frame->fresh     = false;
        }

        goroutine->fresh = true;
    } while (0);

    return true;
}

bool Debugger::fill_local_vars(Debugger_State *draft) {
    auto goroutine = find_goroutine(draft);
    if (!goroutine) return false;

    auto frame = find_frame(goroutine, draft->current_frame);
    if (!frame) return false;
    if (frame->fresh) return false;

    frame->locals = NULL;
    frame->args = NULL;

    auto js = send_packet("ListLocalVars", [&]() {
        rend->field("Scope", [&]() {
            rend->obj([&]() {
                rend->field("GoroutineId", (int)draft->current_goroutine_id);
                rend->field("Frame", (int)draft->current_frame);
                rend->field("DeferredCall", 0);
            });
        });
        rend->field("Cfg", [&]() {
            rend->obj([&]() {
                rend->field("followPointers", true);
                rend->field("maxVariableRecurse", 1);
                rend->field("maxStringLen", 64);
                rend->field("maxArrayValues", 64);
                rend->field("maxStructFields", -1);
            });
        });
    })->js();

    auto vars_idx = js.get(0, ".result.Variables");
    if (vars_idx != -1) {
        frame->locals = new_list(Dlv_Var*);
        save_list_of_vars(js, vars_idx, frame->locals);
    }

    return true;
}

bool Debugger::fill_function_args(Debugger_State *draft) {
    auto goroutine = find_goroutine(draft);
    if (!goroutine) return false;

    auto frame = find_frame(goroutine, draft->current_frame);
    if (!frame) return false;
    if (frame->fresh) return false;

    auto js = send_packet("ListFunctionArgs", [&]() {
        rend->field("Scope", [&]() {
            rend->obj([&]() {
                rend->field("GoroutineId", (int)draft->current_goroutine_id);
                rend->field("Frame", (int)draft->current_frame);
                rend->field("DeferredCall", 0);
            });
        });
        rend->field("Cfg", [&]() {
            rend->obj([&]() {
                rend->field("followPointers", true);
                rend->field("maxVariableRecurse", 1);
                rend->field("maxStringLen", 64);
                rend->field("maxArrayValues", 64);
                rend->field("maxStructFields", -1);
            });
        });
    })->js();

    auto vars_idx = js.get(0, ".result.Args");
    if (vars_idx != -1) {
        frame->args = new_list(Dlv_Var*);
        save_list_of_vars(js, vars_idx, frame->args);
    }

    frame->fresh = true;
    return true;
}

void Debugger::jump_to_frame() {
    if (exiting) return;

    auto goroutine = find_goroutine(state);
    if (!goroutine) return;

    auto frame = find_frame(goroutine, state->current_frame);
    if (!frame) return;

    world.message_queue.add([&](auto msg) {
        msg->type = MTM_GOTO_FILEPOS;
        msg->goto_file = cp_strdup(frame->filepath);
        msg->goto_pos = new_cur2(0, frame->lineno - 1);
    });

    if (debuggee_pid) {
        world.message_queue.add([&](auto msg) {
            msg->type = MTM_FOCUS_APP_DEBUGGER;
            msg->focus_app_debugger_pid = debuggee_pid;
        });
    }
}

void Debugger::do_everything() {
    bool read_something = false;

    defer {
        if (!read_something) {
            sleep_milli(100);
        }
    };

    auto copied_calls = new_list(Dlv_Call);
    {
        SCOPED_LOCK(&calls_lock);
        For (&calls)
            copied_calls->append(it.copy());
        calls.len = 0;
        calls_mem.reset();
    }

    For (copied_calls) {
        switch (it.type) {
        case DLVC_CREATE_WATCH: {
            auto &args = it.create_watch;

            auto watch = watches.append();
            watch->fresh = false;
            watch->state = DBGWATCH_PENDING;

            {
                SCOPED_MEM(&watches_mem);
                cp_strcpy_fixed(watch->expr, args.expression);
                cp_strcpy_fixed(watch->expr_tmp, args.expression);
            }

            if (state_flag == DLV_STATE_PAUSED) eval_watch(watch);
            break;
        }
        case DLVC_EDIT_WATCH: {
            auto &args = it.edit_watch;

            auto watch = &watches[args.watch_idx];

            watch->fresh = false;
            watch->state = DBGWATCH_PENDING;
            watch->editing = false;

            {
                SCOPED_MEM(&watches_mem);
                cp_strcpy_fixed(watch->expr, args.expression);
                cp_strcpy_fixed(watch->expr_tmp, args.expression);
            }

            if (state_flag == DLV_STATE_PAUSED) eval_watch(watch);
            break;
        }
        case DLVC_DELETE_WATCH: {
            auto &args = it.delete_watch;
            watches.remove(args.watch_idx);
            if (!watches.len) {
                watches_mem.cleanup();
                watches_mem.init("watches_mem");
            }
            break;
        }

        case DLVC_VAR_LOAD_MORE: {
            auto &args = it.var_load_more;
            if (args.state_id != state_id) break;

            auto var = *args.var;
            if (!var->incomplete()) break;

            bool notfound = false;
            auto newvar = var->copy();

            switch (var->kind) {
            case GO_KIND_STRUCT:
            case GO_KIND_INTERFACE:
                eval_expression(
                    cp_sprintf("*(*\"%s\")(0x%" PRIx64 ")", var->type, var->address),
                    state->current_goroutine_id,
                    state->current_frame,
                    newvar,
                    var->kind == GO_KIND_STRUCT ? SAVE_VAR_CHILDREN_APPEND : SAVE_VAR_CHILDREN_OVERWRITE
                );
                break;

            case GO_KIND_ARRAY:
            case GO_KIND_SLICE:
            case GO_KIND_STRING: {
                auto offset = var->kind == GO_KIND_STRING ? strlen(var->value) : var->children->len;
                eval_expression(
                    cp_sprintf("(*(*\"%s\")(0x%" PRIx64 "))[%d:]", var->type, var->address, offset),
                    state->current_goroutine_id,
                    state->current_frame,
                    newvar,
                    var->kind == GO_KIND_STRING ? SAVE_VAR_VALUE_APPEND : SAVE_VAR_CHILDREN_APPEND
                );
                break;
            }

            case GO_KIND_MAP:
                eval_expression(
                    cp_sprintf("(*(*\"%s\")(0x%" PRIx64 "))[%d:]", var->type, var->address, var->children->len / 2),
                    state->current_goroutine_id,
                    state->current_frame,
                    newvar,
                    SAVE_VAR_CHILDREN_APPEND
                );
                break;
            default:
                notfound = true;
                break;
            }

            if (notfound) break;

            Pool *mem = NULL;
            if (args.is_watch)
                mem = &watches_mem;
            else
                mem = which_state_pool ? &state_mem_a : &state_mem_b;

            {
                SCOPED_MEM(mem);
                auto ptr = new_object(Dlv_Var);
                memcpy(ptr, newvar->copy(), sizeof(Dlv_Var));
                *args.var = ptr;
            }
            break;
        }
        case DLVC_DELETE_ALL_BREAKPOINTS:
            pause_and_resume([&]() {
                if (state_flag != DLV_STATE_INACTIVE) {
                    For (&breakpoints) {
                        send_packet("ClearBreakpoint", [&]() {
                            rend->field("Id", (int)it.dlv_id);
                        });
                    }
                }
                breakpoints.len = 0;
            });
            break;

        case DLVC_SET_CURRENT_FRAME: {
            auto &args = it.set_current_frame;

            if (state->current_goroutine_id != args.goroutine_id)
                send_command("switchGoroutine", true, args.goroutine_id);

            mutate_state([&](auto draft) {
                draft->current_goroutine_id = args.goroutine_id;
                draft->current_frame = args.frame;

                fill_stacktrace(draft);
                fill_local_vars(draft);
                fill_function_args(draft);
            });

            state_flag = DLV_STATE_PAUSED;
            jump_to_frame();
            eval_watches();
            break;
        }
        case DLVC_STOP:
            exiting = true;
            if (state_flag == DLV_STATE_RUNNING)
                halt_when_already_running();

            {
                SCOPED_FRAME();
                send_packet("Detach", [&]() {
                    rend->field("Kill", true);
                });
            }

            stop();
            state_flag = DLV_STATE_INACTIVE;
            loop_mem.reset();
            break;

        case DLVC_START:
        case DLVC_DEBUG_TEST_UNDER_CURSOR: {
            {
                SCOPED_LOCK(&lock);

                which_state_pool ^= 1;
                Pool *mem = which_state_pool ? &state_mem_a : &state_mem_b;
                mem->reset();

                {
                    SCOPED_MEM(mem);
                    state = new_object(Debugger_State);
                    state->goroutines = new_list(Dlv_Goroutine);
                    state->current_goroutine_id = -1;
                    state->current_frame = 0;
                }

                state_flag = DLV_STATE_STARTING;
                packetid = 0;
                exiting = false;
            }

            Debug_Profile *debug_profile = NULL;
            if (it.type == DLVC_DEBUG_TEST_UNDER_CURSOR) {
                For (project_settings.debug_profiles) {
                    if (it.is_builtin && it.type == DEBUG_TEST_CURRENT_FUNCTION) {
                        debug_profile = &it;
                        break;
                    }
                }
            } else {
                if (it.type == DLVC_START)
                    if (it.start.use_custom_profile)
                        if (it.start.profile_index < project_settings.debug_profiles->len)
                            debug_profile = &project_settings.debug_profiles->items[it.start.profile_index];

                if (!debug_profile)
                    debug_profile = project_settings.get_active_debug_profile();
            }

            if (!debug_profile) {
                // be more specific?
                send_tell_user("Unable to find debug profile.", "Error");
                break;
            }

            if (!start(debug_profile)) {
                SCOPED_LOCK(&lock);
                state_flag = DLV_STATE_INACTIVE;
                loop_mem.reset();
                break;
            }

            auto new_ids = new_list(int);

            For (&breakpoints) {
                auto js = set_breakpoint(it.file, it.line)->js();
                auto idx = js.get(0, ".result.Breakpoint.id");
                new_ids->append(idx == -1 ? -1 : js.num(idx));
            }

            exec_continue(false);

            {
                SCOPED_LOCK(&lock);
                for (int i = 0; i < breakpoints.len; i++) {
                    auto id = new_ids->at(i);
                    if (id != -1)
                        breakpoints[i].dlv_id = id;
                    breakpoints[i].pending = false;
                }
                state_flag = DLV_STATE_RUNNING;
            }
            break;
        }

        case DLVC_TOGGLE_BREAKPOINT: {
            auto &args = it.toggle_breakpoint;

            auto bkpt = breakpoints.find([&](auto it) {
                return are_breakpoints_same(args.filename, args.lineno, it->file, it->line);
            });

            if (!bkpt) {
                Client_Breakpoint b;
                {
                    SCOPED_MEM(&breakpoints_mem);
                    b.file = cp_strdup(args.filename);
                }
                b.line = args.lineno;
                b.pending = true;
                bkpt = breakpoints.append(&b);

                if (state_flag != DLV_STATE_INACTIVE) {
                    auto js = set_breakpoint(bkpt->file, bkpt->line)->js();
                    bkpt->dlv_id = js.num(js.get(0, ".result.Breakpoint.id"));
                }

                bkpt->pending = false;
            } else {
                if (state_flag != DLV_STATE_INACTIVE)
                    unset_breakpoint(bkpt->dlv_id);
                breakpoints.remove(bkpt);
                if (!breakpoints.len)
                    breakpoints_mem.reset();
            }
            break;
        }
        case DLVC_BREAK_ALL:
            if (state_flag == DLV_STATE_RUNNING)
                halt_when_already_running();
            break;
        case DLVC_CONTINUE_RUNNING:
            if (state_flag == DLV_STATE_PAUSED) {
                state_flag = DLV_STATE_RUNNING;
                exec_continue(false);
            }
            break;
        case DLVC_STEP_INTO:
            if (state_flag == DLV_STATE_PAUSED) {
                state_flag = DLV_STATE_RUNNING;
                exec_step_into(false);
            }
            break;
        case DLVC_STEP_OVER:
            if (state_flag == DLV_STATE_PAUSED) {
                state_flag = DLV_STATE_RUNNING;
                exec_step_over(false);
                step_over_time = current_time_nano();
            }
            break;
        case DLVC_STEP_OUT:
            if (state_flag == DLV_STATE_PAUSED) {
                state_flag = DLV_STATE_RUNNING;
                exec_step_out(false);
            }
            break;
        case DLVC_RUN_UNTIL: break;
        }
    }

    if (state_flag != DLV_STATE_RUNNING) return;
    if (!can_read()) return;

    read_something = true;

    Packet p;
    if (!read_packet(&p)) {
        stop();
        state_flag = DLV_STATE_INACTIVE;
        loop_mem.reset();
        return;
    }

    u64 start = 0;
    if (step_over_time) {
        auto time_elapsed = current_time_nano() - step_over_time;
        dbg_print("response after step over took %d ms", time_elapsed / 1000000);
        start = current_time_nano();
    }

    handle_new_state(&p);

    if (step_over_time) {
        auto time_elapsed = current_time_nano() - start;
        dbg_print("handle new state took %d ms", time_elapsed / 1000000);
        step_over_time = 0;
    }
}

void Debugger::handle_new_state(Packet *p) {
    Timer t; t.init("step_over", step_over_time);

    // this might lock up main thread
    SCOPED_LOCK(&lock);

    if (!p) return;

    auto js = p->js();

    if (js.boolean(js.get(0, ".result.State.exited"))) {
        stop();
        state_flag = DLV_STATE_INACTIVE;
        loop_mem.reset();
        return;
    }

    if (js.boolean(js.get(0, ".result.State.Running"))) {
        state_flag = DLV_STATE_RUNNING;
        return;
    }

    if (js.boolean(js.get(0, ".result.State.NextInProgress"))) {
        send_packet("CancelNext", [&]() {});
    }

    t.log("check shit");

    struct Goroutine_With_Bkpt {
        int goroutine_id;
        ccstr breakpoint_file;
        u32 breakpoint_line;
        ccstr breakpoint_func_name;
    };

    List<Goroutine_With_Bkpt> goroutines_with_breakpoint;
    goroutines_with_breakpoint.init();

    auto threads_idx = js.get(0, ".result.State.Threads");
    if (threads_idx != -1) {
        auto threads_len = js.array_length(threads_idx);
        for (int i = 0; i < threads_len; i++) {
            auto it = js.get(threads_idx, i);

            auto bpidx = js.get(it, ".breakPoint");
            if (bpidx != -1) {
                auto entry = goroutines_with_breakpoint.append();
                entry->goroutine_id = js.num(js.get(it, ".goroutineID"));
                entry->breakpoint_file = js.str(js.get(bpidx, ".file"));
                entry->breakpoint_line = js.num(js.get(bpidx, ".line"));
                entry->breakpoint_func_name = js.str(js.get(bpidx, ".functionName"));
            }
        }

        dbg_print("found %d threads with breakpoints", goroutines_with_breakpoint.len);
    }

    mutate_state([&](auto draft) {
        ptr0(draft);

        // grab the current goroutine
        auto current_goroutine_idx = js.get(0, ".result.State.currentGoroutine");
        if (current_goroutine_idx != -1)
            draft->current_goroutine_id = js.num(js.get(current_goroutine_idx, ".id"));
        else
            draft->current_goroutine_id = -1;
        draft->current_frame = 0;
        draft->goroutines = new_list(Dlv_Goroutine);

        t.log("checking threads");

        // fetch goroutines
        do {
            auto resp = send_packet("ListGoroutines", [&]() {
                rend->field("Start", 0);
                rend->field("Count", 0);
                if (options.dbg_hide_system_goroutines) {
                    rend->field("Filters", [&]() {
                        rend->arr([&]() {
                            rend->obj([&]() {
                                rend->field("Kind", 7);
                                rend->field("Negated", false);
                            });
                        });
                    });
                }
            });
            auto js = resp->js();

            auto goroutines_idx = js.get(0, ".result.Goroutines");
            if (goroutines_idx == -1) break;

            auto goroutines_len = js.array_length(goroutines_idx);
            if (!goroutines_len) break;

            for (u32 i = 0; i < goroutines_len; i++) {
                auto it = js.get(goroutines_idx, i);
                auto goroutine = draft->goroutines->append();
                goroutine->id = js.num(js.get(it, ".id"));
                goroutine->curr_file = js.str(js.get(it, ".userCurrentLoc.file"));
                goroutine->curr_line = js.num(js.get(it, ".userCurrentLoc.line"));
                goroutine->curr_func_name = js.str(js.get(it, ".userCurrentLoc.function.name"));
                goroutine->status = js.num(js.get(it, ".status"));
                goroutine->thread_id = js.num(js.get(it, ".threadID"));
                goroutine->fresh = false;
            }
        } while (0);

        For (draft->goroutines) {
            auto entry = goroutines_with_breakpoint.find([&](auto g) { return g->goroutine_id == it.id; });
            if (!entry) continue;

            it.curr_file = entry->breakpoint_file;
            it.curr_line = entry->breakpoint_line;
            it.curr_func_name = entry->breakpoint_func_name;
            it.breakpoint_hit = true;
        }

        fill_stacktrace(draft);
        fill_local_vars(draft);
        fill_function_args(draft);
    });

    state_flag = DLV_STATE_PAUSED;
    jump_to_frame();
    eval_watches();
}
