#pragma once

#define OS_WIN 0
#define OS_MAC 0
#define OS_LINUX 0

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#undef OS_WIN
#define OS_WIN 1
#elif defined(__APPLE__)
#undef OS_MAC
#define OS_MAC 1
#elif defined(__linux__)
#undef OS_LINUX
#define OS_LINUX 1
#else
#error "what the fuck OS is this?"
#endif

#if OS_WIN
#   include "win32.hpp"
#elif OS_MAC
#   include "pthread.h"
#   include "errno.h"
#endif

#if OS_WIN
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#if OS_LINUX
#define FILEPATHS_CASE_SENSITIVE 1
#else
#define FILEPATHS_CASE_SENSITIVE 0
#endif

#include "common.hpp"

enum Process_Status {
    PROCESS_WAITING,
    PROCESS_ERROR,
    PROCESS_DONE,
};

enum Pipe_Direction { PIPE_READ = 0, PIPE_WRITE = 1 };

struct Process {
    // all these options are getting really hairy...
    ccstr cmd;
    ccstr dir;
    bool use_stdin;
    bool dont_use_stdout; // inverted because by default we want to use stdout
    bool create_new_console;
    bool keep_open_after_exit;
    bool skip_shell; // don't run command through a shell

#if OS_WIN
    HANDLE stdin_w;
    HANDLE stdin_r;
    HANDLE stdout_w;
    HANDLE stdout_r;
    DWORD pid;
    HANDLE proc;
#elif OS_MAC
    int stdin_pipe[2];
    int stdout_pipe[2];
    int pid;
    char peek_buffer;
    bool peek_buffer_full;
#endif

    u32 exit_code;

    void init() {
        ptr0(this);
    }

    void cleanup();
    bool run(ccstr _cmd);
    Process_Status status();
    bool peek(char *ch);
    bool can_read();
    bool read1(char* ch);
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

u64 current_time_in_nanoseconds();

bool are_filepaths_same_file(ccstr path1, ccstr path2);
bool are_filepaths_equal(ccstr a, ccstr b);

struct Lock {
#if OS_WIN
    CRITICAL_SECTION lock;
#elif OS_MAC
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

#if OS_WIN

#define get_last_error() get_win32_error()
#define get_specific_error(x) get_win32_error(x)
#define get_socket_error() get_win32_error(WSAGetLastError())

ccstr get_win32_error(DWORD error = -1);

#elif OS_MAC

#define get_last_error() our_sprintf("(%d) %s", errno, strerror(errno))
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

typedef fn<void(Dir_Entry*)> list_directory_cb;
bool list_directory(ccstr path, list_directory_cb cb);

// TODO: group these into a struct?
typedef void* Thread_Handle;
typedef void (*Thread_Callback)(void*);
Thread_Handle create_thread(Thread_Callback callback, void* param);
void close_thread_handle(Thread_Handle h);
void kill_thread(Thread_Handle h);

enum {
    FILE_MODE_READ = 1 << 0,
    FILE_MODE_WRITE = 1 << 1,
};

enum File_Open_Mode {
    FILE_OPEN_EXISTING,
    FILE_CREATE_NEW,
};

enum File_Result {
    FILE_RESULT_SUCCESS,
    FILE_RESULT_FAILURE,
    FILE_RESULT_ALREADY_EXISTS,
};

#define FILE_SEEK_END (u32)(-1)
#define FILE_SEEK_ERROR (u32)(-1)

struct File {
#if OS_WIN
    HANDLE h;
#elif OS_MAC
    int fd;
#endif

    File_Result init(ccstr path, int access, File_Open_Mode open_mode);
    void cleanup();
    bool read(char *buf, s32 size);
    bool write(char *buf, s32 size);
    u32 seek(u32 pos);
};

void max_out_clock_frequency();
void sleep_milliseconds(u32 milliseconds);

struct Select_File_Opts {
    cstr buf;
    u32 bufsize;
    ccstr starting_folder;
    bool folder;
    bool save;
};

bool let_user_select_file(Select_File_Opts* opts);
bool copy_file(ccstr src, ccstr dest, bool overwrite);
bool ensure_directory_exists(ccstr path);
bool delete_rm_rf(ccstr path);

ccstr normalize_path_sep(ccstr path, char sep = 0);
bool is_sep(char ch);

typedef fn<int(const void *a, const void *b)> compare_func;

void xplat_quicksort(void *list, s32 num, s32 size, compare_func cmp);
void *xplat_binary_search(const void *key, void *list, s32 num, s32 size, compare_func cmp);

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

#if OS_WIN
    HANDLE file;
    HANDLE mapping;
    bool create_actual_file_mapping(LARGE_INTEGER size);
#elif OS_MAC
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

ccstr rel_to_abs_path(ccstr path);

enum Fs_Event_Type {
    FSEVENT_CHANGE,
    FSEVENT_DELETE,
    FSEVENT_CREATE,
    FSEVENT_RENAME,
};

ccstr fs_event_type_str(Fs_Event_Type t);

struct Fs_Event {
    Fs_Event_Type type;
    char filepath[MAX_PATH];
    char new_filepath[MAX_PATH];
};

struct Fs_Watcher {
#if OS_WIN
    FILE_NOTIFY_INFORMATION *buf;
    bool has_more;
    s32 offset;
    ccstr path;
    HANDLE dir_handle;
    OVERLAPPED ol;

    bool initiate_wait();
#elif OS_MAC
    int put_something_here_so_struct_isnt_empty;
#endif

    bool init(ccstr _path);
    void cleanup();
    bool next_event(Fs_Event *event);
};

// this must be case-insensitive
ccstr get_path_relative_to(ccstr full, ccstr base);

ccstr get_canon_path(ccstr path);

bool move_file_atomically(ccstr src, ccstr dest);

enum Charset_Type {
    CS_ENGLISH,
    CS_CHINESE_SIM,
    CS_CHINESE_TRAD,
    CS_KOREAN,
    CS_JAPANESE,
    CS_CYRILLIC,
};

void *read_font_data_from_name(s32 *len, ccstr name, Charset_Type charset);
void *read_font_data_from_first_found(s32 *plen, Charset_Type charset, ...);

enum Ask_User_Result {
    ASKUSER_ERROR = 0,
    ASKUSER_YES,
    ASKUSER_NO,
    ASKUSER_CANCEL,
};

Ask_User_Result ask_user_yes_no_cancel(ccstr text, ccstr title);
Ask_User_Result ask_user_yes_no(ccstr text, ccstr title);
void tell_user(ccstr text, ccstr title);
ccstr get_executable_path();
bool set_run_on_computer_startup(ccstr key, ccstr path_to_exe);

bool xplat_chdir(ccstr dir);

bool create_directory(ccstr path);
bool touch_file(ccstr path);
