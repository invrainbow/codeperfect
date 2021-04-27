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

List<Dlv_Var>* Debugger::save_list_of_vars(Json_Navigator js, i32 idx) {
    auto len = js.array_length(idx);
    if (len == 0) return NULL;

    List<Dlv_Var> *ret = NULL;
    {
        SCOPED_MEM(&state_mem);
        ret = alloc_list<Dlv_Var>(len);
    }

    for (u32 i = 0; i < len; i++)
        save_single_var(js, js.get(idx, i), ret->append());
    return ret;
}

void Debugger::save_single_var(Json_Navigator js, i32 idx, Dlv_Var* dest) {
    SCOPED_MEM(&state_mem);

    dest->name = js.str(js.get(idx, ".name"));
    dest->value = js.str(js.get(idx, ".value"));
    dest->gotype = (Go_Reflect_Kind)js.num(js.get(idx, ".kind"));
    dest->gotype_name = js.str(js.get(idx, "realType"));
    dest->children = save_list_of_vars(js, js.get(idx, ".children"));

    if (dest->gotype == GO_KIND_STRUCT || dest->gotype == GO_KIND_ARRAY)
        dest->delve_reported_number_of_children = js.get(idx, ".len");
}

void Debugger::fetch_stackframe(Dlv_Goroutine *goroutine) {
    auto resp = send_packet("Stacktrace", [&]() {
        rend->field("id", (int)goroutine->id);
        rend->field("depth", 1);
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
            frame->freshness = DLVF_NEEDFILL;
        }
    }

    goroutine->freshness = DLVF_FRESH;
}

bool Debugger::eval_expression(ccstr expression, i32 goroutine_id, i32 frame_id, Dlv_Var* out) {
    if (goroutine_id == -1) goroutine_id = get_current_goroutine_id();
    if (goroutine_id == -1) return false;

    auto packet = send_packet("Eval", [&]() {
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

    auto js = packet->js();

    auto variable_idx = js.get(0, ".result.Variable");
    if (variable_idx == -1) return false;

    save_single_var(js, variable_idx, out);
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

void Debugger::send_command(ccstr command, bool read) {
    SCOPED_FRAME();
    send_packet("Command", [&]() {
        rend->field("name", command);
        rend->field("threadID", -1);
        rend->field("goroutineID", -1);
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

i32 Json_Navigator::num(i32 i) {
    SCOPED_FRAME();
    auto s = str(i);
    return atoi(s);
    // return s == NULL ? 0 : atoi(s);
}

bool Json_Navigator::boolean(i32 i) {
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

void Debugger::run_loop() {
    defer {
        print("debugger loop ending???");
    };

    while (true) {
        {
            SCOPED_LOCK(&calls_lock);
            For (calls) {
                switch (it.type) {
                case DLVC_STOP:
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
                        world.wnd_debugger.current_goroutine = -1;
                        world.wnd_debugger.current_frame = -1;

                        ptr0(&state);
                        state_flag = DLV_STATE_STARTING;
                        packetid = 0;

                        if (!start()) {
                            state_flag = DLV_STATE_INACTIVE;
                            break;
                        }

                        {
                            SCOPED_LOCK(&lock);
                            For (breakpoints) {
                                auto js = set_breakpoint(it.file, it.line)->js();
                                auto idx = js.get(0, ".result.Breakpoint.id");
                                if (idx != -1)
                                    it.dlv_id = js.num(idx);
                                it.pending = false;
                            }
                        }
                        exec_continue(false);
                        state_flag = DLV_STATE_RUNNING;
                    }
                    break;

                case DLVC_EVAL_SINGLE_WATCH:
                    {
                        auto& params = it.eval_single_watch;

                        auto& watch = watches[params.watch_id];
                        watch.state = DBGWATCH_PENDING;
                        if (!eval_expression(watch.expr, -1, params.frame_id, &watch.value))
                            watch.state = DBGWATCH_ERROR;
                        else
                            watch.state = DBGWATCH_READY;
                    }
                    break;
                case DLVC_EVAL_WATCHES:
                    {
                        auto& params = it.eval_watches;

                        For (watches)
                            it.state = DBGWATCH_PENDING;

                        auto results = alloc_array(bool, watches.len);

                        u32 i = 0;
                        For (watches)
                            results[i++] = eval_expression(it.expr, -1, params.frame_id, &it.value);

                        i = 0;
                        For (watches)
                            it.state = (results[i++] ? DBGWATCH_READY : DBGWATCH_ERROR);
                    }
                    break;
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
    if (p == NULL) return;

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
    }

    // TODO: when we halt, current_goroutine_id may (often seems to) be unset

    {
        state_mem.reset();
        SCOPED_MEM(&state_mem);
        fetch_goroutines();
        state_flag = DLV_STATE_PAUSED;
    }

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
}

void Debugger::fetch_variables(int goroutine_id, int frame_idx, Dlv_Frame *frame) {
    Json_Navigator js;

    js = send_packet("ListLocalVars", [&]() {
        rend->field("Scope", [&]() {
            rend->obj([&]() {
                rend->field("GoroutineId", goroutine_id);
                rend->field("Frame", frame_idx);
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
    if (vars_idx != -1)
        frame->locals = save_list_of_vars(js, vars_idx);

    js = send_packet("ListFunctionArgs", [&]() {
        rend->field("Scope", [&]() {
            rend->obj([&]() {
                rend->field("GoroutineId", goroutine_id);
                rend->field("Frame", frame_idx);
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
    if (vars_idx != -1)
        frame->locals = save_list_of_vars(js, vars_idx);

    frame->freshness = DLVF_FRESH;
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
            goroutine->freshness = DLVF_NEEDFILL;
        }

        if (goroutine->id == state.current_goroutine_id) {
            fetch_stackframe(goroutine);
            fetch_variables(goroutine->id, state.current_frame, &goroutine->frames->items[0]);
        }
    }
}
