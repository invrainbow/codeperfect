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

#define DEBUGGER_LOG 0

Json_Navigator Packet::js() {
    Json_Navigator ret;
    ret.string = string;
    ret.tokens = tokens;
    return ret;
}

bool Debugger::find_breakpoint(ccstr filename, u32 line, Breakpoint* out) {
    SCOPED_FRAME();

    auto breakpoints = list_breakpoints();
    if (breakpoints == NULL)
        return false;

    For (*breakpoints)
        if (streq(it.file, filename))
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

List<Dbg_Var>* Debugger::save_list_of_vars(Json_Navigator js, i32 idx) {
    auto len = js.array_length(idx);
    if (len == 0) return NULL;

    auto ret = alloc_list<Dbg_Var>(len);
    for (u32 i = 0; i < len; i++)
        save_single_var(js, js.get(idx, i), ret->append());
    return ret;
}

void Debugger::save_single_var(Json_Navigator js, i32 idx, Dbg_Var* dest) {
    dest->name = js.str(js.get(idx, ".name"));
    dest->value = js.str(js.get(idx, ".value"));
    dest->gotype = (GoReflectKind)js.num(js.get(idx, ".kind"));
    dest->gotype_name = js.str(js.get(idx, "realType"));
    dest->children = save_list_of_vars(js, js.get(idx, ".children"));

    if (dest->gotype == GO_KIND_STRUCT || dest->gotype == GO_KIND_ARRAY)
        dest->delve_reported_number_of_children = js.get(idx, ".len");
}

List<Dbg_Location>* Debugger::get_stackframe(i32 goroutine_id) {
    if (goroutine_id == -1) goroutine_id = get_current_goroutine_id();
    if (goroutine_id == -1) return NULL;

    auto resp = send_packet("Stacktrace", [&]() {
        rend->field("id", goroutine_id);
        rend->field("depth", 50);
        rend->field("full", false);
        rend->field("cfg", [&]() {
            rend->obj([&]() {
                rend->field("followPointers", true);
                rend->field("maxVariableRecurse", 2);
                rend->field("maxStringLen", 64);
                rend->field("maxArrayValues", 64);
                rend->field("maxStructFields", -1);
            });
        });
    });

    auto js = resp->js();

    auto locations_idx = js.get(0, ".result.Locations");
    if (locations_idx == -1) return NULL;

    auto locations_len = js.array_length(locations_idx);
    auto ret = alloc_list<Dbg_Location>(locations_len);

    for (u32 i = 0; i < locations_len; i++) {
        auto location_idx = js.get(locations_idx, i);
        if (location_idx == -1) continue;

        auto loc = ret->append();
        loc->filepath = js.str(js.get(location_idx, ".file"));
        loc->lineno = js.num(js.get(location_idx, ".line"));
        loc->func_name = js.str(js.get(location_idx, ".function.name"));

        auto locals_idx = js.get(location_idx, ".Locals");
        if (locals_idx != -1)
            loc->locals = save_list_of_vars(js, locals_idx);
    }

    return ret;
}

bool Debugger::eval_expression(ccstr expression, i32 goroutine_id, i32 frame_id, Dbg_Var* out) {
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

    SCOPED_MEM(&mem);

    lock.init();
    breakpoints.init();
    watches.init();
    call_queue.init();

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
        if (DEBUGGER_LOG)
            print("[\"send\"]\n%s", data);

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
        if (DEBUGGER_LOG)
            print("[\"recv\"]\n%s", our_format_json(p->string));
        return true;
    }

    if (DEBUGGER_LOG)
        print("[\"recv\"]\n\"<error: unable to read packet>\"");
    return false;
}

Packet* Debugger::set_breakpoint(ccstr filename, u32 lineno) {
    return send_packet("CreateBreakpoint", [&]() {
        rend->field("Breakpoint", [&]() {
            rend->obj([&]() {
                rend->field("file", filename);
                rend->field("line", (int)lineno);
            });
        });
    });
}

bool are_breakpoints_same(ccstr file1, u32 line1, ccstr file2, u32 line2) {
    return streq(file1, file2) && (line1 == line2);
}

bool Debugger::unset_breakpoint(ccstr filename, u32 lineno) {
    Breakpoint bp;
    if (!find_breakpoint(filename, lineno, &bp)) return false;

    SCOPED_FRAME();
    send_packet("ClearBreakpoint", [&]() {
        rend->field("Id", (int)bp.id);
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

        auto start = MEM->sp;

        auto wksp_path = get_normalized_path(world.current_path);
        auto full_path = get_normalized_path(js.str(js.get(it, ".file")));
        auto relative_path = get_path_relative_to(full_path, wksp_path);

        MEM->sp = start + strlen(relative_path) + 1;

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
        bp->num_stackframes = js.num(js.get(it, ".stackframe"));

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

    tokens = alloc_list<jsmntok_t>(num_toks);
    tokens->len = num_toks;

    // TODO: if it failed, free the list of tokens we just alloc'd
    return !is_jsmn_error(parse_json_with_jsmn(string, tokens->items, num_toks));
}

int Json_Navigator::advance_node(int i) {
    auto tok = tokens->at(i++);
    for (u32 j = 0; j < tok.size; j++)
        i = advance_node(i);
    return i;
}

bool Json_Navigator::match(int i, ccstr s) {
    auto tok = tokens->at(i);
    if (tok.type != JSMN_STRING)
        return false;

    auto len = strlen(s);
    if (tok.end - tok.start != len)
        return false;

    for (int j = 0; j < len; j++)
        if (string[tok.start + j] != s[j])
            return false;
    return true;
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
    dlv_proc.run("dlv exec --headless main.exe --listen=127.0.0.1:1234");

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
    // TODO: probably kill dlv_proc too
}

void Debugger::run_loop() {
    defer {
        print("debugger loop ending???");
    };

    while (true) {
        Dbg_Call call;
        if (call_queue.pop(&call)) {
            switch (call.type) {
            case DBGCALL_START:
                {
                    // TODO: initialize everything else we need to for the debugger.
                    // should this even go in debugger_loop_thread? or like inside the key callback?
                    world.wnd_debugger.current_location = -1;

                    ptr0(&state);
                    state_flag = DBGSTATE_STARTING;
                    packetid = 0;

                    if (!start()) {
                        state_flag = DBGSTATE_INACTIVE;
                        break;
                    }

                    {
                        SCOPED_LOCK(&lock);
                        For (breakpoints) {
                            set_breakpoint(it.file, it.line);
                            it.pending = false;
                        }
                    }
                    exec_continue(false);
                    state_flag = DBGSTATE_RUNNING;
                }
                break;

            case DBGCALL_EVAL_SINGLE_WATCH:
                {
                    auto& params = call.eval_single_watch;

                    auto& watch = watches[params.watch_id];
                    watch.state = DBGWATCH_PENDING;
                    if (!eval_expression(watch.expr, -1, params.frame_id, &watch.value))
                        watch.state = DBGWATCH_ERROR;
                    else
                        watch.state = DBGWATCH_READY;
                }
                break;
            case DBGCALL_EVAL_WATCHES:
                {
                    auto& params = call.eval_watches;

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
            case DBGCALL_SET_BREAKPOINT:
                {
                    auto& params = call.set_breakpoint;

                    auto resp = set_breakpoint(params.filename, params.lineno);
                    if (resp == NULL) {
                        surface_error("Unable to set breakpoint.");
                        break;
                    }

                    auto find_func = [&](Client_Breakpoint* bp) -> bool {
                        return are_breakpoints_same(bp->file, bp->line, params.filename, params.lineno);
                    };

                    auto js = resp->js();
                    if (js.get(0, ".result.Breakpoint.id") == -1) {
                        surface_error("Unable to set breakpoint.");
                        breakpoints.remove(find_func);
                    } else {
                        auto bkpt = breakpoints.find(find_func);
                        if (bkpt != NULL) {
                            bkpt->pending = false;
                        } else {
                            // TODO: error -- this shouldn't happen
                        }
                    }
                }
                break;
            case DBGCALL_UNSET_BREAKPOINT:
                unset_breakpoint(
                    call.unset_breakpoint.filename,
                    call.unset_breakpoint.lineno
                );
                break;
            case DBGCALL_CONTINUE_RUNNING:
                state_flag = DBGSTATE_RUNNING;
                exec_continue(false);
                break;
            case DBGCALL_STEP_INTO:
                state_flag = DBGSTATE_RUNNING;
                exec_step_into(false);
                break;
            case DBGCALL_STEP_OVER:
                state_flag = DBGSTATE_RUNNING;
                exec_step_over(false);
                break;
            case DBGCALL_RUN_UNTIL: break;
            case DBGCALL_CHANGE_VARIABLE: break;
            }
            continue;
        }

        if (state_flag != DBGSTATE_RUNNING) continue;
        if (!can_read()) continue;

        Packet p;
        if (!read_packet(&p)) {
            stop();
            state_flag = DBGSTATE_INACTIVE;
            continue;
        }

        // FIXME: memory is going to overflow here with all of our js.str(...) calls without freeing

        auto js = p.js();

        auto is_exited = js.boolean(js.get(0, ".result.State.exited"));
        if (is_exited) {
            stop();
            state_flag = DBGSTATE_INACTIVE;
            continue;
        }

        // auto is_recording = js.boolean(js.get(0, ".result.State.Recording"));
        auto is_running = js.boolean(js.get(0, ".result.State.Running"));

        // debugger hit breakpoint
        if (!is_running) {
            state.stackframe = get_stackframe();
            state.file_stopped_at = get_normalized_path(js.str(js.get(0, ".result.State.currentThread.file")));
            state.line_stopped_at = js.num(js.get(0, ".result.State.currentThread.line"));
            state_flag = DBGSTATE_PAUSED;
        } else {
            state_flag = DBGSTATE_RUNNING;
        }
    }
}
