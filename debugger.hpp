#pragma once

#define JSMN_HEADER
#include "jsmn.h"
#include "common.hpp"
#include "os.hpp"
#include "utils.hpp"
#include "list.hpp"

const char DELVE_PATH[] = "/Users/user/go/bin/dlv";

ccstr jsmn_type_str(int type);

enum Json_KeyType {
    KEY_TYPE_ARR,
    KEY_TYPE_OBJ,
    KEY_TYPE_END,
};

struct Json_Key {
    Json_KeyType type;
    union {
        int arr_key;
        ccstr obj_key;
    };
};

const Json_Key KEY_END = { KEY_TYPE_END };

int parse_json_with_jsmn(ccstr s, jsmntok_t* tokens, u32 num_toks);

struct Json_Navigator {
    ccstr string;
    List<jsmntok_t>* tokens;

    bool parse(ccstr s);
    int advance_node(int i);
    bool match(int i, ccstr s);
    Json_Key key(int i);
    Json_Key key(ccstr s);
    i32 array_length(i32 i);
    i32 get(i32 i, ccstr keys);
    i32 get(i32 i, i32 index);
    ccstr str(i32 i, u32* plen);
    ccstr str(i32 i);
    i32 num(i32 i);
    bool boolean(i32 i);
};

struct Packet {
    ccstr string;
    List<jsmntok_t>* tokens;

    Json_Navigator js();
};

struct Breakpoint {
    u32 id;
    ccstr name;
    List<u64>* addrs;
    ccstr file;
    u32 line;
    ccstr function_name;

    bool is_goroutine;
    bool is_tracepoint;
    bool is_at_return_in_traced_function;
    int num_stackframes;
    List<ccstr>* variable_expressions;

    // ???
    // HitCount map[string]uint64 `json:"hitCount"`
    // TotalHitCount uint64 `json:"totalHitCount"
};

bool are_breakpoints_same(ccstr file1, u32 line1, ccstr file2, u32 line2);

struct Client_Breakpoint {
    ccstr file;
    u32 line;
    bool pending;
};

enum DbgState {
    DBGSTATE_INACTIVE = 0,
    DBGSTATE_STARTING,
    DBGSTATE_RUNNING,
    DBGSTATE_PAUSED,
};

enum Dbg_CallType {
    DBGCALL_SET_BREAKPOINT = 1,
    DBGCALL_UNSET_BREAKPOINT,
    DBGCALL_CONTINUE_RUNNING,
    DBGCALL_STEP_INTO,
    DBGCALL_STEP_OVER,
    DBGCALL_STEP_OUT,
    DBGCALL_RUN_UNTIL,
    DBGCALL_CHANGE_VARIABLE,
    DBGCALL_EVAL_WATCHES,
    DBGCALL_EVAL_SINGLE_WATCH,
    DBGCALL_START,
};

struct Dbg_Call {
    Dbg_CallType type;
    union {
        struct {
            ccstr filename;
            u32 lineno;
        } set_breakpoint;

        struct {
            ccstr filename;
            u32 lineno;
        } unset_breakpoint;

        struct {
            u32 frame_id;
        } eval_watches;

        struct {
            u32 frame_id;
            u32 watch_id;
        } eval_single_watch;

        struct {} continue_running;
        struct {} step_info;
        struct {} step_over;
        struct {} step_out;
        struct {} run_until;
        struct {} change_variable;
    };
};

// https://godoc.org/reflect#Kind
enum GoReflectKind {
    GO_KIND_INVALID = 0,
    GO_KIND_BOOL,
    GO_KIND_INT,
    GO_KIND_INT8,
    GO_KIND_INT16,
    GO_KIND_INT32,
    GO_KIND_INT64,
    GO_KIND_UINT,
    GO_KIND_UINT8,
    GO_KIND_UINT16,
    GO_KIND_UINT32,
    GO_KIND_UINT64,
    GO_KIND_UINTPTR,
    GO_KIND_FLOAT32,
    GO_KIND_FLOAT64,
    GO_KIND_COMPLEX64,
    GO_KIND_COMPLEX128,
    GO_KIND_ARRAY,
    GO_KIND_CHAN,
    GO_KIND_FUNC,
    GO_KIND_INTERFACE,
    GO_KIND_MAP,
    GO_KIND_PTR,
    GO_KIND_SLICE,
    GO_KIND_STRING,
    GO_KIND_STRUCT,
    GO_KIND_UNSAFEPOINTER,
};

struct Dbg_Var {
    ccstr name;
    GoReflectKind gotype;
    ccstr gotype_name;
    ccstr value; // do we need anything deeper than this?
    List<Dbg_Var>* children;
    u32 delve_reported_number_of_children;
};

enum Dbg_WatchState {
    DBGWATCH_PENDING,
    DBGWATCH_READY,
    DBGWATCH_ERROR,
};

struct Dbg_Watch {
    char expr[256];
    Dbg_WatchState state;
    Dbg_Var value;
};

struct Dbg_Location {
    ccstr filepath;
    u32 lineno;
    ccstr func_name;
    List<Dbg_Var>* locals;
};

struct Debugger {
    Pool mem;
    Pool loop_mem;

    Lock lock;
    List<Client_Breakpoint> breakpoints;
    List<Dbg_Watch> watches;
    In_Memory_Queue<Dbg_Call> call_queue;
    Thread_Handle thread;
    Json_Renderer* rend;

    // per session stuff
    Process dlv_proc;
    int conn;
    int packetid;

    DbgState state_flag;
    struct {
        ccstr file_stopped_at;
        u32 line_stopped_at;
        List<Dbg_Location>* stackframe;
    } state;

    // Debugger has no cleanup method, because it's meant to run for the
    // program's entire lifespan.

    void init();
    void cleanup();
    bool start();
    void stop();

    u8 read1();
    bool write1(u8 ch);
    Packet* send_packet(ccstr packet_name, lambda f, bool read = true);
    bool read_packet(Packet* p);
    Packet* set_breakpoint(ccstr filename, u32 lineno);
    bool unset_breakpoint(ccstr filename, u32 lineno);
    void send_command(ccstr command, bool read);
    void exec_continue(bool read);
    void exec_step_into(bool read);
    void exec_step_out(bool read);
    void exec_step_over(bool read);
    bool find_breakpoint(ccstr filename, u32 line, Breakpoint* out);
    bool can_read();
    List<Breakpoint>* list_breakpoints();
    bool eval_expression(ccstr expression, i32 goroutine_id, i32 frame_id, Dbg_Var* out);
    i32 get_current_goroutine_id();
    List<Dbg_Location>* get_stackframe(i32 goroutine_id = -1);

    List<Dbg_Var>* save_list_of_vars(Json_Navigator js, i32 idx);
    void save_single_var(Json_Navigator js, i32 idx, Dbg_Var* out);
    void start_loop();
    void run_loop();
    void surface_error(ccstr msg);
};

void debugger_loop_thread(void*);
