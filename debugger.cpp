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

#include "debugger.hpp"

#include "world.hpp"
#include "os.hpp"
#include "utils.hpp"
#include "go.hpp"

#if OS_WIN
#include "win32.hpp"
#elif OS_LINUX
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#define DBG_DEBUG 1

#if DBG_DEBUG
#define dbg_print(fmt, ...) print("[dbg] " fmt, ##__VA_ARGS__)
#else
#define dbg_print(fmt, ...)
#endif

bool Dlv_Var::incomplete() {
    auto children_len = [&]() -> int { return children == NULL ? 0 : children->len; };

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
        return children_len() > 0 && children->at(0).only_addr;
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
    if (breakpoints == NULL)
        return false;

    For (*breakpoints)
        if (are_filepaths_equal(it.file, filename))
            if (it.line == line)
                return memcpy(out, &it, sizeof(Breakpoint)), true;

    return false;
}

i32 Debugger::get_current_goroutine_id() {
    auto resp = send_packet("State", [&]() {});
    auto js = resp->js();

    auto idx = js.get(0, ".result.State.currentThread.goroutineID");
    return idx == -1 ? -1 : js.num(idx);
}

void Debugger::save_list_of_vars(Json_Navigator js, i32 idx, List<Dlv_Var> *out) {
    auto len = js.array_length(idx);
    for (u32 i = 0; i < len; i++)
        save_single_var(js, js.get(idx, i), out->append());
}

void Debugger::save_single_var(Json_Navigator js, i32 idx, Dlv_Var* out, Save_Var_Mode save_mode) {
    SCOPED_MEM(&state_mem);

    if (save_mode == SAVE_VAR_NORMAL) {
        out->name = js.str(js.get(idx, ".name"));
        out->value = js.str(js.get(idx, ".value"));
        out->kind = (Go_Reflect_Kind)js.num(js.get(idx, ".kind"));
        out->len = js.num(js.get(idx, ".len"));
        out->cap = js.num(js.get(idx, ".cap"));
        out->address = js.num_u64(js.get(idx, ".addr"));
        out->kind_name = js.str(js.get(idx, ".realType"));
        out->flags = js.num(js.get(idx, ".flags"));
        out->only_addr = js.boolean(js.get(idx, ".onlyAddr"));

        auto unreadable = js.str(js.get(idx, ".unreadable"));
        if (unreadable != NULL && unreadable[0] != '\0')
            out->unreadable_description = unreadable;
    }

    if (save_mode == SAVE_VAR_VALUE_APPEND) {
        out->value = our_sprintf("%s%s", out->value, js.str(js.get(idx, ".value")));
    } else {
        if (save_mode == SAVE_VAR_NORMAL || save_mode == SAVE_VAR_CHILDREN_OVERWRITE)
            out->children = alloc_list<Dlv_Var>();
        save_list_of_vars(js, js.get(idx, ".children"), out->children);
    }
}

void Debugger::fetch_stackframe(Dlv_Goroutine *goroutine) {
    auto resp = send_packet("Stacktrace", [&]() {
        rend->field("id", (int)goroutine->id);
        rend->field("depth", 50);
        rend->field("full", false);
    });

    auto js = resp->js();

    auto locations_idx = js.get(0, ".result.Locations");
    if (locations_idx == -1) return;

    auto locations_len = js.array_length(locations_idx);

    {
        SCOPED_MEM(&state_mem);
        goroutine->frames = alloc_list<Dlv_Frame>();
    }

    for (u32 i = 0; i < locations_len; i++) {
        auto it = js.get(locations_idx, i);

        {
            SCOPED_MEM(&state_mem);
            auto frame = goroutine->frames->append();
            frame->filepath  = js.str(js.get(it, ".file"));
            frame->lineno    = js.num(js.get(it, ".line"));
            frame->func_name = js.str(js.get(it, ".function.name"));
            frame->fresh     = false;
        }
    }

    goroutine->fresh = true;
}

void Debugger::eval_watch(Dlv_Watch *watch, int goroutine_id, int frame) {
    // TODO: is there any difference between these two flags?
    watch->fresh = false;
    watch->state = DBGWATCH_PENDING;

    if (eval_expression(watch->expr, goroutine_id, frame, &watch->value, SAVE_VAR_NORMAL))
        watch->state = DBGWATCH_READY;
    else
        watch->state = DBGWATCH_ERROR;

    watch->fresh = true;
}

bool Debugger::eval_expression(ccstr expression, i32 goroutine_id, i32 frame, Dlv_Var* out, Save_Var_Mode save_mode) {
    if (goroutine_id == -1) goroutine_id = get_current_goroutine_id();
    if (goroutine_id == -1) return false;

    auto packet = send_packet("Eval", [&]() {
        rend->field("Expr", expression);
        rend->field("Scope", [&]() {
            rend->obj([&]() {
                rend->field("goroutineID", goroutine_id);
                rend->field("frame", frame);
            });
        });
        rend->field("Cfg", [&]() {
            rend->obj([&]() {
                rend->field("followPointers", true);
                rend->field("maxVariableRecurse", 1);
                rend->field("maxStringLen", 256);
                rend->field("maxArrayValues", 64);
                rend->field("maxStructFields", -1);
            });
        });
    });

    auto js = packet->js();

    auto variable_idx = js.get(0, ".result.Variable");
    if (variable_idx == -1) return false;

    save_single_var(js, variable_idx, out, save_mode);
    return true;
}

bool Debugger::can_read() {
    char ret = 0;
    return recv(conn, (char*)&ret, 1, MSG_PEEK) == 1;
}

u8 Debugger::read1() {
    u8 ret = 0;
    recv(conn, (char*)&ret, 1, 0);
    return ret;
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
            if (pret != NULL) *pret = r.finish();
            return true;
        }
        if (pret != NULL) r.writechar(c);
    }

    frame.restore();
    return false;
}
#endif

void Debugger::init() {
    ptr0(this);

    mem.init();
    loop_mem.init();
    state_mem.init();
    breakpoints_mem.init();
    watches_mem.init();

    SCOPED_MEM(&mem);

    lock.init();
    breakpoints.init();
    watches.init();

    calls.init();
    calls_lock.init();
    calls_mem.init();

#if OS_WIN
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        panic("WSAStartup failed");
#endif
}

void Debugger::cleanup() {
    lock.cleanup();

    if (thread != NULL) {
        kill_thread(thread);
        close_thread_handle(thread);
    }

#if OS_WIN
    WSACleanup();
#endif

    calls.cleanup();
    calls_lock.cleanup();
    calls_mem.cleanup();

    mem.cleanup();
    loop_mem.cleanup();
}

#if OS_WIN
#define ioctl_stub ioctlsocket
#define close_stub closesocket
#else
#define ioctl_stub ioctl
#define close_stub close
#endif

Packet* Debugger::send_packet(ccstr packet_name, lambda f, bool read) {
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

    if (!read) return NULL;

    Packet p;
    if (!read_packet(&p)) return NULL;

    auto packet = alloc_object(Packet);
    memcpy(packet, &p, sizeof(p));
    return packet;
}

bool Debugger::read_packet(Packet* p) {
    u_long mode_blocking = 0;
    u_long mode_nonblocking = 1;

#if OS_WIN
    ioctlsocket(conn, FIONBIO, &mode_blocking);
    defer { ioctlsocket(conn, FIONBIO, &mode_nonblocking); };
#else
    ioctl(conn, FIONBIO, &mode_blocking);
    defer { ioctl(conn, FIONBIO, &mode_nonblocking); };
#endif

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
        if (s == NULL)
            return false;

        auto is_jsmn_error = [&](int result) -> bool {
            switch (result) {
                case JSMN_ERROR_INVAL:
                case JSMN_ERROR_NOMEM:
                case JSMN_ERROR_PART:
                    return true;
            }
            return false;
        };

        Json_Navigator js;
        if (!js.parse(s))
            return false;

        p->string = js.string;
        p->tokens = js.tokens;
        return true;
    };

    if (run()) {
        SCOPED_FRAME();
		dbg_print("[\"recv\"]\n%s", p->string); // our_format_json(p->string));
        return true;
    }

	dbg_print("[\"recv\"]\n\"<error: unable to read packet>\"");
    return false;
}

void Debugger::halt_when_already_running() {
    exec_halt(true);

    // Since we were already running, when we call halt, we're going to get
    // two State responses, one for the original command that made us run
    // in the first place, and one from the halt. Swallow the second response.

    Packet p = {0};
    if (read_packet(&p))
        handle_new_state(&p);
    else
        state_flag = DLV_STATE_PAUSED;
}

void Debugger::pause_and_resume(fn<void()> f) {
    bool was_running = (state_flag == DLV_STATE_RUNNING);

    if (was_running)
        halt_when_already_running();

    f();

    if (was_running) {
        state_flag = DLV_STATE_RUNNING;
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
    auto ret = alloc_list<Breakpoint>(breakpoints_len);

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

            bp->addrs = alloc_list<u64>(js.array_length(idx));
            for (i32 j = 0; j < len; j++)
                bp->addrs->append(js.num(js.get(idx, j)));
        }

        {
            auto idx = js.get(it, ".variables");
            if (idx != -1) {
                auto len = js.array_length(idx);

                bp->variable_expressions = alloc_list<ccstr>(len);
                for (i32 j = 0; j < len; j++)
                    bp->variable_expressions->append(js.str(js.get(idx, j)));
            }
        }
    }

    return ret;
}

int parse_json_with_jsmn(ccstr s, jsmntok_t* tokens, u32 num_toks) {
    jsmn_parser parser;
    jsmn_init(&parser);
    return jsmn_parse(&parser, s, strlen(s), tokens, num_toks);
}

ccstr jsmn_type_str(int type) {
    switch (type) {
        define_str_case(JSMN_PRIMITIVE);
        define_str_case(JSMN_ARRAY);
        define_str_case(JSMN_OBJECT);
        define_str_case(JSMN_STRING);
    }
    return NULL;
}

bool Json_Navigator::parse(ccstr s) {
    string = s;

    auto is_jsmn_error = [&](int result) -> bool {
        switch (result) {
        case JSMN_ERROR_INVAL:
        case JSMN_ERROR_NOMEM:
        case JSMN_ERROR_PART:
            return true;
        }
        return false;
    };

    auto num_toks = parse_json_with_jsmn(string, NULL, 0);
    if (is_jsmn_error(num_toks))
        return false;

    Frame frame;

    tokens = alloc_list<jsmntok_t>(num_toks);
    tokens->len = num_toks;

    bool ret = !is_jsmn_error(parse_json_with_jsmn(string, tokens->items, num_toks));
    if (!ret) frame.restore();
    return ret;
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
    auto buf = alloc_array(char, newlen + 1);
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
    return get(i, our_sprintf("[%d]", idx));
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

    auto ret = alloc_array(char, len + 1);
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
    // return s == NULL ? 0 : atoi(s);
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
        SCOPED_MEM(&dbg->loop_mem);
        MEM->reset();
        dbg->run_loop();
    };

    SCOPED_MEM(&mem);
    thread = create_thread(func, this);
}

void Debugger::surface_error(ccstr msg) {
    // TODO: surface some kind of error, how do?
}

bool Debugger::start() {
    conn = -1;

    dlv_proc.init();
    // dlv_proc.use_stdin = true;
    dlv_proc.dont_use_stdout = true;
    dlv_proc.dir = world.current_path;
    dlv_proc.create_new_console = true;
    dlv_proc.run(our_sprintf("dlv exec --headless %s --listen=127.0.0.1:1234", world.settings.debug_binary_path));

    /*
    // read the first line
    char ch = 0;
    do {
        if (!dlv_proc.read1(&ch)) return false;
        printf("%c", ch);
    } while (ch != '\n');
    printf("\n");
    */

    auto server = "127.0.0.1";
    auto port = "1234";

    addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result;
    if (getaddrinfo(server, port, &hints, &result) != 0)
        return error("unable to resolve server:port %s:%s", server, port), false;

    auto make_connection = [&]() -> int {
        for (auto ptr = result; ptr != NULL; ptr = ptr->ai_next) {
            auto fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (fd == -1) return -1;

            u_long io_mode = 1; // non-blocking
            if (connect(fd, ptr->ai_addr, ptr->ai_addrlen) != -1)
                if (ioctl_stub(fd, FIONBIO, &io_mode) == 0)
                    return fd;

            print("unable to connect: %s", get_socket_error());
            close_stub(fd);
        }
        return -1;
    };

    for (u32 i = 0; i < 10; i++, sleep_milliseconds(1000)) {
        conn = make_connection();
        if (conn != -1) break;
    }

    if (conn == -1)
        return error("unable to connect: %s", get_socket_error()), false;

    auto resp = send_packet("SetApiVersion", [&]() {
        rend->field("APIVersion", 2);
    });

    if (resp == NULL)
        return error("unable to set API version"), false;

    return true;
}

void Debugger::stop() {
    if (conn != 0 && conn != -1)
        close_stub(conn);
    dlv_proc.cleanup();
}

void Debugger::set_current_goroutine(u32 goroutine_id) {
    // anything else?
    send_command("switchGoroutine", true, goroutine_id);
}

void Debugger::select_frame(u32 goroutine_id, u32 frame) {
    auto old_goroutine_id = state.current_goroutine_id;

    state.current_frame = 0;
    state.current_goroutine_id = goroutine_id;
    state.current_frame = frame;

    if (old_goroutine_id != goroutine_id)
        set_current_goroutine(goroutine_id);

    auto goroutine = state.goroutines.find([&](auto it) { return it->id == goroutine_id; });
    if (goroutine == NULL) return;

    if (!goroutine->fresh)
        fetch_stackframe(goroutine);

    auto dlvframe = &goroutine->frames->items[frame];
    if (!dlvframe->fresh)
        fetch_variables(goroutine_id, frame, dlvframe);

    For (watches) eval_watch(&it, goroutine_id, frame);

    if (!exiting) {
        world.add_event([&](auto msg) {
            msg->type = MTM_GOTO_FILEPOS;
            msg->goto_filepos.file = our_strcpy(dlvframe->filepath);
            msg->goto_filepos.pos = new_cur2((i32)0, (i32)dlvframe->lineno - 1);
        });
    }
}

void Debugger::run_loop() {
    defer {
        print("debugger loop ending???");
    };

    while (true) {
        {
            SCOPED_LOCK(&calls_lock);
            For (calls) {
                switch (it.type) {
                case DLVC_CREATE_WATCH:
                    {
                        auto &args = it.create_watch;

                        auto watch = watches.append();
                        watch->fresh = false;
                        watch->state = DBGWATCH_PENDING;

                        {
                            SCOPED_MEM(&watches_mem);
                            strcpy_safe(watch->expr, _countof(watch->expr), args.expression);
                            strcpy_safe(watch->expr_tmp, _countof(watch->expr_tmp), args.expression);
                        }

                        if (state_flag == DLV_STATE_PAUSED)
                            eval_watch(watch, state.current_goroutine_id, state.current_frame);
                    }
                    break;
                case DLVC_EDIT_WATCH:
                    {
                        auto &args = it.edit_watch;

                        auto watch = &watches[args.watch_idx];

                        watch->fresh = false;
                        watch->state = DBGWATCH_PENDING;
                        watch->editing = false;

                        {
                            SCOPED_MEM(&watches_mem);
                            strcpy_safe(watch->expr, _countof(watch->expr), args.expression);
                            strcpy_safe(watch->expr_tmp, _countof(watch->expr_tmp), args.expression);
                        }

                        if (state_flag == DLV_STATE_PAUSED)
                            eval_watch(watch, state.current_goroutine_id, state.current_frame);
                    }
                    break;
                case DLVC_DELETE_WATCH:
                    {
                        auto &args = it.delete_watch;
                        watches.remove(args.watch_idx);
                        if (watches.len == 0) {
                            watches_mem.cleanup();
                            watches_mem.init();
                        }
                    }
                    break;

                case DLVC_VAR_LOAD_MORE:
                    {
                        auto &args = it.var_load_more;
                        if (args.state_id != state_id) break;

                        auto var = args.var;
                        if (!var->incomplete()) break;

                        switch (var->kind) {
                        case GO_KIND_STRUCT:
                        case GO_KIND_INTERFACE:
                            eval_expression(
                                our_sprintf("*(*%s)(0x%" PRIx64 ")", var->kind_name, var->address),
                                state.current_goroutine_id,
                                state.current_frame,
                                var,
                                var->kind == GO_KIND_STRUCT ? SAVE_VAR_CHILDREN_APPEND : SAVE_VAR_CHILDREN_OVERWRITE
                            );
                            break;

                        case GO_KIND_ARRAY:
                        case GO_KIND_SLICE:
                        case GO_KIND_STRING:
                            {
                                auto offset = var->kind == GO_KIND_STRING ? strlen(var->value) : var->children->len;
                                eval_expression(
                                    our_sprintf("(*(*%s)(0x%" PRIx64 "))[%d:]", var->kind_name, var->address, offset),
                                    state.current_goroutine_id,
                                    state.current_frame,
                                    var,
                                    var->kind == GO_KIND_STRING ? SAVE_VAR_VALUE_APPEND : SAVE_VAR_CHILDREN_APPEND
                                );
                            }
                            break;

                        case GO_KIND_MAP:
                            eval_expression(
                                our_sprintf("(*(*%s)(0x%" PRIx64 "))[%d:]", var->kind_name, var->address, var->children->len / 2),
                                state.current_goroutine_id,
                                state.current_frame,
                                var,
                                SAVE_VAR_CHILDREN_APPEND
                            );
                            break;
                        }
                    }
                    break;
                case DLVC_DELETE_ALL_BREAKPOINTS:
                    pause_and_resume([&]() {
                        if (state_flag != DLV_STATE_INACTIVE) {
                            For (breakpoints) {
                                send_packet("ClearBreakpoint", [&]() {
                                    rend->field("Id", (int)it.dlv_id);
                                });
                            }
                        }
                        breakpoints.len = 0;
                    });
                    break;

                case DLVC_SET_CURRENT_FRAME:
                    {
                        auto &args = it.set_current_frame;
                        select_frame(args.goroutine_id, args.frame);
                        /*
                        function select_frame:
                            load vars and watches

                        function select_goroutine:
                            set current goroutine
                            if goroutine is not fresh
                                get stacktrace

                        DLVC_SET_CURRENT_GOROUTINE:
                            if goroutine is already selected
                                i guess do nothing?
                            else
                                select_goroutine goroutine
                                select_frame: goroutine, first frame

                        DLVC_SET_CURRENT_FRAME:
                            if goroutine is not already selected
                                select_goroutine goroutine
                            select_frame goroutine, frame
                         */

                    }
                    break;
                case DLVC_SET_CURRENT_GOROUTINE:
                    {
                        auto &args = it.set_current_goroutine;
                        select_frame(args.goroutine_id, 0);
                    }
                    break;
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
                    {
                        {
                            SCOPED_LOCK(&lock);
                            ptr0(&state);

                            state.current_goroutine_id = -1;
                            state.current_frame = -1;

                            state_flag = DLV_STATE_STARTING;
                            packetid = 0;
                            exiting = false;
                        }

                        if (!start()) {
                            SCOPED_LOCK(&lock);
                            state_flag = DLV_STATE_INACTIVE;
                            break;
                        }

                        auto new_ids = alloc_list<int>();

                        For (breakpoints) {
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
                    }
                    break;
#if 0
                        auto& params = it.eval_watches;
                        For (watches)
                            it.state = DBGWATCH_PENDING;

                        auto results = alloc_array(bool, watches.len);

                        u32 i = 0;
                        For (watches)
                            results[i++] = eval_expression(it.expr, -1, params.frame, &it.value);

                        i = 0;
                        For (watches)
                            it.state = (results[i++] ? DBGWATCH_READY : DBGWATCH_ERROR);
                    }
                    break;
#endif

                case DLVC_TOGGLE_BREAKPOINT:
                    {
                        auto &args = it.toggle_breakpoint;

                        auto bkpt = breakpoints.find([&](auto it) {
                            return are_breakpoints_same(args.filename, args.lineno, it->file, it->line);
                        });

                        if (bkpt == NULL) {
                            Client_Breakpoint b;
                            {
                                SCOPED_MEM(&breakpoints_mem);
                                b.file = our_strcpy(args.filename);
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
                            if (breakpoints.len == 0)
                                breakpoints_mem.reset();
                        }
                    }
                    break;
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
                    }
                    break;
                case DLVC_STEP_OUT:
                    if (state_flag == DLV_STATE_PAUSED) {
                        state_flag = DLV_STATE_RUNNING;
                        exec_step_out(false);
                    }
                    break;
                case DLVC_RUN_UNTIL: break;
                case DLVC_CHANGE_VARIABLE: break;
                }
            }
            calls.len = 0;
            calls_mem.reset();
        }

        if (state_flag != DLV_STATE_RUNNING) continue;
        if (!can_read()) continue;

        Packet p;
        if (!read_packet(&p)) {
            stop();
            state_flag = DLV_STATE_INACTIVE;
            loop_mem.reset();
            continue;
        }

        // TODO: there needs to be a SCOPED_FRAME here, or loop_mem will grow
        handle_new_state(&p);
    }
}

void Debugger::handle_new_state(Packet *p) {
    // this might lock up main thread
    SCOPED_LOCK(&lock);

    if (p == NULL) return;

    state_id++;

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

    ptr0(&state);

    state.current_goroutine_id = -1;
    state.current_frame = 0;

    auto current_goroutine_idx = js.get(0, ".result.State.currentGoroutine");
    if (current_goroutine_idx != -1)
        state.current_goroutine_id = js.num(js.get(current_goroutine_idx, ".id"));

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

        print("found %d threads with breakpoints", goroutines_with_breakpoint.len);
    }

    {
        state_mem.reset();
        SCOPED_MEM(&state_mem);
        fetch_goroutines();
        state_flag = DLV_STATE_PAUSED;
    }

    select_frame(state.current_goroutine_id, state.current_frame);

    For (state.goroutines) {
        auto entry = goroutines_with_breakpoint.find([&](auto g) { return g->goroutine_id == it.id; });
        if (entry != NULL) {
            SCOPED_MEM(&state_mem);
            it.curr_file = entry->breakpoint_file;
            it.curr_line = entry->breakpoint_line;
            it.curr_func_name = entry->breakpoint_func_name;
            it.breakpoint_hit = true;
        }
    }

    int goroutine_id = state.current_goroutine_id;
    if (goroutine_id == -1) // halting
        goroutine_id = state.goroutines[0].id;
    select_frame(goroutine_id, state.current_frame);
}

void Debugger::fetch_variables(int goroutine_id, int frame, Dlv_Frame *dlvframe) {
    Json_Navigator js;

    dlvframe->locals = NULL;
    dlvframe->args = NULL;

    js = send_packet("ListLocalVars", [&]() {
        rend->field("Scope", [&]() {
            rend->obj([&]() {
                rend->field("GoroutineId", goroutine_id);
                rend->field("Frame", frame);
                rend->field("DeferredCall", 0);
            });
        });
        rend->field("Cfg", [&]() {
            rend->obj([&]() {
                rend->field("followPointers", true);
                rend->field("maxVariableRecurse", 2);
                rend->field("maxStringLen", 64);
                rend->field("maxArrayValues", 64);
                rend->field("maxStructFields", -1);
            });
        });
    })->js();

    auto vars_idx = js.get(0, ".result.Variables");
    if (vars_idx != -1) {
        dlvframe->locals = alloc_list<Dlv_Var>();
        save_list_of_vars(js, vars_idx, dlvframe->locals);
    }

    js = send_packet("ListFunctionArgs", [&]() {
        rend->field("Scope", [&]() {
            rend->obj([&]() {
                rend->field("GoroutineId", goroutine_id);
                rend->field("Frame", frame);
                rend->field("DeferredCall", 0);
            });
        });
        rend->field("Cfg", [&]() {
            rend->obj([&]() {
                rend->field("followPointers", true);
                rend->field("maxVariableRecurse", 2);
                rend->field("maxStringLen", 64);
                rend->field("maxArrayValues", 64);
                rend->field("maxStructFields", -1);
            });
        });
    })->js();

    vars_idx = js.get(0, ".result.Args");
    if (vars_idx != -1) {
        dlvframe->args = alloc_list<Dlv_Var>();
        save_list_of_vars(js, vars_idx, dlvframe->args);
    }

    dlvframe->fresh = true;
}

bool Debugger::fetch_goroutines() {
    // get goroutines
    auto resp = send_packet("ListGoroutines", [&]() {
        rend->field("Start", 0);
        rend->field("Count", 0);
    });
    auto js = resp->js();

    auto goroutines_idx = js.get(0, ".result.Goroutines");
    if (goroutines_idx == -1) return false;

    auto goroutines_len = js.array_length(goroutines_idx);
    if (goroutines_len == 0) return false;

    {
        SCOPED_MEM(&state_mem);
        state.goroutines.init();
    }

    for (u32 i = 0; i < goroutines_len; i++) {
        auto it = js.get(goroutines_idx, i);
        auto goroutine = state.goroutines.append();

        {
            SCOPED_MEM(&state_mem);
            goroutine->id = js.num(js.get(it, ".id"));
            goroutine->curr_file = js.str(js.get(it, ".userCurrentLoc.file"));
            goroutine->curr_line = js.num(js.get(it, ".userCurrentLoc.line"));
            goroutine->curr_func_name = js.str(js.get(it, ".userCurrentLoc.function.name"));
            goroutine->status = js.num(js.get(it, ".status"));
            goroutine->thread_id = js.num(js.get(it, ".threadID"));
            goroutine->fresh = false;
        }
    }
}
