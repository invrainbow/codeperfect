#pragma once

#include "ostype.hpp"

#if OS_WINBLOWS
#   include "win32.hpp"
#   include <direct.h>
#   define getcwd _getcwd
#elif OS_MAC || OS_LINUX
#   include <pthread.h>
#   include <errno.h>
#   include <signal.h>
#   include <unistd.h>
#   if OS_LINUX
#       include <sys/epoll.h>
#       include <sys/inotify.h>
#       include <limits.h>
#   endif
#endif

#if OS_WINBLOWS
#   define PATH_SEP '\\'
#else
#   define PATH_SEP '/'
#endif

#if OS_LINUX
#   define FILEPATHS_CASE_SENSITIVE 1
#else
#   define FILEPATHS_CASE_SENSITIVE 0
#endif

#ifdef DEBUG_BUILD
#   if defined (_MSC_VER)
#       define BREAK_HERE() __debugbreak()
#   elif defined(__clang__)
#       define BREAK_HERE() __builtin_debugtrap()
#   elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#       define BREAK_HERE() __asm__ volatile("int $0x03")
#   elif defined(__GNUC__) && defined(__thumb__)
#       define BREAK_HERE() __asm__ volatile(".inst 0xde01")
#   elif defined(__GNUC__) && defined(__arm__) && !defined(__thumb__)
#       define BREAK_HERE() __asm__ volatile(".inst 0xe7f001f0");
#   else
#       define BREAK_HERE()
#   endif
#else
#   define BREAK_HERE()
#endif

#include "common.hpp"

#ifdef DEBUG_BUILD
#define cp_panic(s) do { BREAK_HERE(); exit(1); } while (0)
#else
NORETURN void cp_panic(ccstr s);
#endif

#define cp_assert(x) do { if (!(x)) cp_panic("assertion failed"); } while (0)

#include "list.hpp"
#include "mem.hpp"

enum Process_Status {
    PROCESS_WAITING,
    PROCESS_ERROR,
    PROCESS_DONE,
};

struct Process {
    // all these options are getting really hairy...
    ccstr cmd;
    ccstr dir;
    bool use_stdin;
    bool dont_use_stdout; // inverted because by default we want to use stdout
    bool create_new_console;
    bool keep_open_after_exit;
    bool skip_shell; // don't run command through a shell
    Process_Status saved_status;

#if OS_WINBLOWS
    HANDLE stdin_w;
    HANDLE stdin_r;
    HANDLE stdout_w;
    HANDLE stdout_r;
    DWORD pid;
    HANDLE proc;
#elif OS_MAC || OS_LINUX
    union {
        struct {
            int stdin_pipe[2];
            int stdout_pipe[2];
        };
        struct {
            int stdin_pipe_read;
            int stdin_pipe_write;
            int stdout_pipe_read;
            int stdout_pipe_write;
        };
    };
    int pid;
    char peek_buffer;
    bool peek_buffer_full;
#endif

    char read_buffer[1024];
    int read_buffer_ptr;
    int read_buffer_len;

    u32 exit_code;

    void init() {
        ptr0(this);
        saved_status = PROCESS_WAITING;
    }

    void cleanup();
    bool run(ccstr _cmd);

    Process_Status os_status();

    Process_Status status() {
        if (saved_status == PROCESS_WAITING)
            saved_status = os_status();
        return saved_status;
    }

    bool peek(char *ch);
    bool can_read();
    bool os_can_read();

    bool read1(char* out);
    int readn(char* out, int n);

    bool write1(char ch);
    bool writestr(ccstr s, s32 len = 0);
    ccstr readall(u32* plen = NULL);
    void flush();
    void done_writing();
};

enum File_Type {
    FILE_TYPE_NORMAL,
    FILE_TYPE_DIRECTORY,
};

u64 current_time_nano();
u64 current_time_milli();

bool are_filepaths_same_file(ccstr path1, ccstr path2);
bool are_filepaths_equal(ccstr a, ccstr b);

struct Lock {
#if OS_WINBLOWS
    CRITICAL_SECTION lock;
#elif OS_MAC || OS_LINUX
    pthread_mutex_t lock;
#endif

    void init();
    void cleanup();
    bool try_enter();
    void enter();
    void leave();
};

struct Scoped_Lock {
    Lock *lock;
    Scoped_Lock(Lock *_lock) { lock = _lock; lock->enter(); }
    ~Scoped_Lock() { lock->leave(); }
};

#define SCOPED_LOCK(lock) Scoped_Lock GENSYM(SCOPED_LOCk)(lock)

ccstr get_normalized_path(ccstr path);

enum Check_Path_Result {
    CPR_NONEXISTENT = 0,
    CPR_FILE,
    CPR_DIRECTORY,
};

Check_Path_Result check_path(ccstr path);

#if OS_WINBLOWS

#define get_last_error() get_win32_error()
#define get_specific_error(x) get_win32_error(x)
#define get_socket_error() get_win32_error(WSAGetLastError())

ccstr get_win32_error(DWORD error = -1);

#elif OS_MAC || OS_LINUX

#define get_last_error() cp_sprintf("(%d) %s", errno, strerror(errno))
#define get_socket_error() strerror(errno)

#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

enum Dir_Entry_Type {
    DIRENT_FILE,
    DIRENT_DIR,
};

struct Dir_Entry {
    Dir_Entry_Type type;
    char name[MAX_PATH];
};

typedef fn<bool(Dir_Entry*)> list_directory_cb;

bool list_directory(ccstr path, list_directory_cb cb);

// TODO: group these into a struct?
typedef void* Thread_Handle;
typedef void (*Thread_Callback)(void*);
Thread_Handle create_thread(Thread_Callback callback, void* param = NULL);
void close_thread_handle(Thread_Handle h);
void kill_thread(Thread_Handle h);
NORETURN void exit_thread(int retval);

enum {
    FILE_MODE_READ = 1 << 0,
    FILE_MODE_WRITE = 1 << 1,
};

enum File_Open_Mode {
    FILE_OPEN_EXISTING,
    FILE_CREATE_NEW,
};

enum File_Result {
    FILE_RESULT_OK,
    FILE_RESULT_FAILURE,
    FILE_RESULT_ALREADY_EXISTS,
};

#define FILE_SEEK_END (u32)(-1)
#define FILE_SEEK_ERROR (u32)(-1)

struct File {
#if OS_WINBLOWS
    HANDLE h;
#elif OS_MAC || OS_LINUX
    int fd;
#endif

    File_Result init(ccstr path, int access, File_Open_Mode open_mode);

    File_Result init_read(ccstr path) {
        return init(path, FILE_MODE_READ, FILE_OPEN_EXISTING);
    }

    File_Result init_write(ccstr path) {
        return init(path, FILE_MODE_WRITE, FILE_CREATE_NEW);
    }

    void cleanup();
    bool read(char *buf, s32 size);
    bool write(ccstr buf, s32 size);
    u32 seek(u32 pos);
};

void max_out_clock_frequency();
void sleep_milli(u32 milliseconds);

struct Select_File_Opts {
    cstr buf;
    u32 bufsize;
    ccstr starting_folder;
    bool folder;
    bool save;
};

bool let_user_select_file(Select_File_Opts* opts);
bool os_let_user_select_file(Select_File_Opts* opts);
bool copy_file(ccstr src, ccstr dest);
bool ensure_directory_exists(ccstr path);
bool delete_rm_rf(ccstr path);
bool delete_file(ccstr path);

ccstr normalize_path_sep(ccstr path, char sep = 0);
bool is_sep(char ch);

typedef fn<int(const void *a, const void *b)> compare_func;
void cp_quicksort(void *list, s32 num, s32 size, compare_func cmp);

u64 get_file_size(ccstr file);

struct File_Mapping_Opts {
    bool write;
    // File_Open_Mode open_mode;
    i64 initial_size;
};

struct File_Mapping {
    u8 *data;
    i64 len;
    File_Mapping_Opts opts;

#if OS_WINBLOWS
    HANDLE file;
    HANDLE mapping;
    bool create_actual_file_mapping(LARGE_INTEGER size);
#elif OS_MAC || OS_LINUX
    int fd;
    bool create_actual_file_mapping(i64 size);
#endif

    bool init(ccstr path) {
        File_Mapping_Opts opts = {0};
        opts.write = false;
        return init(path, &opts);
    }

    bool init(ccstr path, File_Mapping_Opts *opts);
    bool flush(i64 bytes_to_flush);
    bool finish_writing(i64 final_size);
    bool resize(i64 newlen);
    void cleanup();
};

File_Mapping *map_file_into_memory(ccstr path);
File_Mapping *map_file_into_memory(ccstr path, File_Mapping_Opts *opts);

ccstr rel_to_abs_path(ccstr path, ccstr cwd = NULL);

struct Fs_Event {
    char filepath[MAX_PATH];
};

#if OS_LINUX
#define INOTIFY_BUFSIZE (64 * (sizeof(struct inotify_event) + NAME_MAX + 1))
#endif

struct Fs_Watcher {
    ccstr path;
    Pool mem;

#if OS_WINBLOWS
    FILE_NOTIFY_INFORMATION *buf;
    bool has_more;
    s32 offset;
    HANDLE dir_handle;
    OVERLAPPED ol;

    bool initiate_wait();
#elif OS_MAC
    List<Fs_Event> events;
    int curr;

    void* stream;     // FSEventStreamRef
    void* context;    // FSEventStreamContext
    void* run_loop;   // CFRunLoopRef

    void handle_event(size_t count, ccstr *paths);
    void run_thread();
#elif OS_LINUX
    int ino_fd;
    int epoll_fd;
    epoll_event epoll_ev;
    char curr_buf[INOTIFY_BUFSIZE];
    u32 curr_len;
    u32 curr_off;
    void *wd_table;

    ccstr wd_to_key(int wd);
    void wd_table_set(int wd, ccstr name);
    ccstr wd_table_get(int wd);
    void wd_table_remove(int wd);
#endif

    bool init(ccstr _path) {
        ptr0(this);
        mem.init("fs_watcher mem");
        path = _path;
        if (!platform_init()) {
            cleanup();
            return false;
        }
        return true;
    }

    void cleanup() {
        platform_cleanup();
        mem.cleanup();
    }

    bool platform_init();
    void platform_cleanup();
    bool next_event(Fs_Event *event);
};

// this must be case-insensitive
ccstr get_path_relative_to(ccstr full, ccstr base);

ccstr get_canon_path(ccstr path);

bool move_file_atomically(ccstr src, ccstr dest);

enum Ask_User_Result {
    ASKUSER_ERROR = 0,
    ASKUSER_YES,
    ASKUSER_NO,
    ASKUSER_CANCEL,
};

Ask_User_Result ask_user_yes_no(ccstr text, ccstr title, ccstr yeslabel, ccstr nolabel, bool cancel = false);
Ask_User_Result ask_user_yes_no_cancel(ccstr text, ccstr title, ccstr yeslabel, ccstr nolabel);
void tell_user(ccstr text, ccstr title);
Ask_User_Result os_ask_user_yes_no(ccstr text, ccstr title, ccstr yeslabel, ccstr nolabel, bool cancel = false);
void os_tell_user(ccstr text, ccstr title);

#define tell_user_error(text) tell_user(text, NULL)

ccstr get_executable_path();
bool cp_chdir(ccstr dir);

bool create_directory(ccstr path);
bool touch_file(ccstr path);

void init_platform_crap();

ccstr cp_getcwd();

int get_unix_time();
void write_to_syslog(ccstr s);

bool list_all_fonts(List<ccstr> *out);

enum Font_Data_Type {
    FONT_DATA_MALLOC,
    FONT_DATA_MMAP,
    FONT_DATA_FIXED,
};

struct Font_Data {
    Font_Data_Type type;
    union {
        struct {
            u8 *data;
            u32 len;
        };
        File_Mapping *fm;
    };

    u8* get_data() {
        if (type == FONT_DATA_MALLOC) return data;
        if (type == FONT_DATA_MMAP) return fm->data;
        if (type == FONT_DATA_FIXED) return data;
        cp_panic("invalid font type");
    }

    u32 get_len() {
        if (type == FONT_DATA_MALLOC) return len;
        if (type == FONT_DATA_MMAP) return fm->len;
        if (type == FONT_DATA_FIXED) return len;
        cp_panic("invalid font type");
    }

    void cleanup() {
        if (type == FONT_DATA_MALLOC) {
            if (data != NULL) {
                free(data);
                data = NULL;
                len = 0;
            }
        } else if (type == FONT_DATA_MMAP) {
            if (fm != NULL) {
                fm->cleanup();
                fm = NULL;
            }
        }
    }
};

Font_Data* load_system_ui_font();
// set dont_check_name to true when we're fairly sure the font exists
// such as when it's a name returned by fontconfig
Font_Data* load_font_data_by_name(ccstr name, bool dont_check_name);

ccstr _cp_dirname(ccstr path);
ccstr cp_dirname(ccstr path);
ccstr cp_basename(ccstr path);

void fork_self(List<char*> *args = NULL, bool exit_this = true);

typedef fn<int(const void *it)> bs_test_func;
int binary_search(void *list, s32 num, s32 size, bs_test_func test);

ccstr generate_stack_trace(ccstr message = NULL);
NORETURN void exit_from_crash_handler();
void install_crash_handlers();

int get_current_focused_window_pid();

bool move_file_or_directory(ccstr src, ccstr dest);
