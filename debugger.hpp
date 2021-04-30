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
    u64 num_u64(i32 i);
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
    int dlv_id;
};

enum Dlv_State {
    DLV_STATE_INACTIVE = 0,
    DLV_STATE_STARTING,
    DLV_STATE_RUNNING,
    DLV_STATE_PAUSED,
};

enum Dlv_Call_Type {
    DLVC_TOGGLE_BREAKPOINT = 1,
    DLVC_CONTINUE_RUNNING,
    DLVC_STEP_INTO,
    DLVC_STEP_OVER,
    DLVC_STEP_OUT,
    DLVC_RUN_UNTIL,
    DLVC_CHANGE_VARIABLE,
    DLVC_EVAL_WATCHES,
    DLVC_EVAL_SINGLE_WATCH,
    DLVC_START,
    DLVC_BREAK_ALL,
    DLVC_STOP,
    DLVC_SET_CURRENT_FRAME,
    DLVC_SET_CURRENT_GOROUTINE,
    DLVC_DELETE_ALL_BREAKPOINTS,
    DLVC_VAR_LOAD_MORE,
};

// https://godoc.org/reflect#Kind
enum Go_Reflect_Kind {
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

enum {
	DLV_VAR_ESCAPED = 1 << 0,
	DLV_VAR_SHADOWED = 1 << 1,
	DLV_VAR_CONSTANT  = 1 << 2,
	DLV_VAR_ARGUMENT = 1 << 3,
	DLV_VAR_RETURNARGUMENT = 1 << 4,
	DLV_VAR_FAKEADDRESS = 1 << 5,
};

struct Dlv_Var {
    ccstr name;
    int flags;
    Go_Reflect_Kind kind;
    ccstr kind_name;
    int len;
    int cap;
    ccstr value; // do we need anything deeper than this?
    List<Dlv_Var>* children;
    bool is_shadowed;
    u64 address;
    bool only_addr; // for interface types
    ccstr unreadable_description;

    bool incomplete();
};

enum Dlv_Watch_State {
    DBGWATCH_PENDING,
    DBGWATCH_READY,
    DBGWATCH_ERROR,
};

struct Dlv_Watch {
    char expr[256];
    Dlv_Watch_State state;
    Dlv_Var value;
};

struct Dlv_Frame {
    ccstr filepath;
    u32 lineno;
    ccstr func_name;
    List<Dlv_Var> *locals;
    List<Dlv_Var> *args;
    bool fresh;
};

struct Dlv_Goroutine {
    u32 id;
    List<Dlv_Frame> *frames;

    ccstr curr_file;
    u32 curr_line;
    ccstr curr_func_name;
    int status; // TODO: make enum type; i don't know what this is yet
    int thread_id;
    bool breakpoint_hit;
    bool fresh;
};

struct Dlv_Call {
    Dlv_Call_Type type;
    union {
        struct {
            ccstr filename;
            u32 lineno;
        } toggle_breakpoint;

        struct {
            u32 goroutine_id;
            u32 frame_id;
        } set_current_frame;

        struct {
            u32 goroutine_id;
        } set_current_goroutine;

        struct {
            u32 frame_id;
        } eval_watches;

        struct {
            u32 frame_id;
            u32 watch_id;
        } eval_single_watch;

        struct {
            int state_id;
            Dlv_Var *var;
        } var_load_more;
    };
};

enum Save_Var_Mode {
    SAVE_VAR_NORMAL,
    SAVE_VAR_CHILDREN_APPEND,
    SAVE_VAR_CHILDREN_OVERWRITE,
    SAVE_VAR_VALUE_APPEND,
};

struct Debugger {
    Pool mem;
    Pool loop_mem;   // throwaway memory for miscellaneous operations inside loop
    Pool state_mem;  // memory for holding state info; torn down & rebuilt every time state changes.
    Pool breakpoints_mem;
    Pool watches_mem;

    Lock lock;
    List<Client_Breakpoint> breakpoints;
    List<Dlv_Watch> watches;
    Thread_Handle thread;
    Json_Renderer* rend;

    List<Dlv_Call> calls;
    Lock calls_lock;
    Pool calls_mem;

    // per session stuff
    Process dlv_proc;
    int conn;
    int packetid;

    int state_id;

    Dlv_State state_flag;
    struct {
        List<Dlv_Goroutine> goroutines;
        i32 current_goroutine_id;
        i32 current_frame;
    } state;
    bool waiting_for_dlv_state;

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
    bool unset_breakpoint(int id);

    void send_command(ccstr command, bool read, int goroutine_id = -1);
    void exec_continue(bool read);
    void exec_step_into(bool read);
    void exec_step_out(bool read);
    void exec_step_over(bool read);
    void exec_halt(bool read);

    bool find_breakpoint(ccstr filename, u32 line, Breakpoint* out);
    bool can_read();
    List<Breakpoint>* list_breakpoints();
    bool eval_expression(ccstr expression, i32 goroutine_id, i32 frame_id, Dlv_Var* out, Save_Var_Mode save_mode = SAVE_VAR_NORMAL);
    i32 get_current_goroutine_id();

    void save_list_of_vars(Json_Navigator js, i32 idx, List<Dlv_Var>* out);
    void save_single_var(Json_Navigator js, i32 idx, Dlv_Var* out, Save_Var_Mode save_mode = SAVE_VAR_NORMAL);
    void start_loop();
    void run_loop();
    void surface_error(ccstr msg);

    void push_call(Dlv_Call_Type type, fn<void(Dlv_Call *call)> f);
    void push_call(Dlv_Call_Type type);

    void fetch_stackframe(Dlv_Goroutine *goroutine);
    void fetch_variables(int goroutine_id, int frame_idx, Dlv_Frame *frame);
    bool fetch_goroutines();
    void handle_new_state(Packet *p);
    void pause_and_resume(fn<void()> f);
    void halt_when_already_running();
    void select_frame(u32 goroutine_id, u32 frame_id);
    void set_current_goroutine(u32 goroutine_id);
};

void debugger_loop_thread(void*);
