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
#include "win32.hpp"
#endif

#if OS_WIN
#define PATH_SEP '\\'
#else
#define PATH_SEP = '/'
#endif

#include "common.hpp"

enum Process_Status {
    PROCESS_WAITING,
    PROCESS_ERROR,
    PROCESS_DONE,
};

enum Pipe_Direction { PIPE_READ = 0, PIPE_WRITE = 1 };

struct Process {
    ccstr cmd;
    ccstr dir;
    bool use_stdin;

#if OS_WIN
    HANDLE stdin_w;
    HANDLE stdin_r;
    HANDLE stdout_w;
    HANDLE stdout_r;
    DWORD pid;
    HANDLE proc;
#elif OS_LINUX
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
    Process_Status status(bool allow_non_zero_exit_code = false);
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

struct Lock {
#if OS_WIN
    CRITICAL_SECTION lock;
#elif OS_LINUX
    pthread_mutex_t lock;
#endif

    void init();
    void cleanup();
    bool enter();
    bool leave();
};

struct Scoped_Lock {
    Lock *lock;
    Scoped_Lock(Lock *_lock) { lock = _lock; lock->enter(); }
    ~Scoped_Lock() { lock->leave(); }
};

#define SCOPED_LOCK(lock) Scoped_Lock GENSYM(SCOPED_LOCk)(lock)

u32 get_normalized_path(ccstr path, char *buf, u32 len);
ccstr get_normalized_path(ccstr path);

enum Check_Path_Result {
    CPR_NONEXISTENT = 0,
    CPR_FILE,
    CPR_DIRECTORY,
};

Check_Path_Result check_path(ccstr path);

#if OS_WIN

struct Win32_Error {
    char* str;
    Win32_Error(char *_str) { str = _str; }
    ~Win32_Error() { if (str != NULL) LocalFree((HLOCAL)str); }
};

#define get_last_error() Win32_Error(get_win32_error()).str
#define get_socket_error() Win32_Error(get_winsock_error()).str

char* get_win32_error(DWORD error = -1);
char* get_winsock_error();

#elif OS_LINUX

#define get_last_error() strerror(errno)
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

struct File {
#if OS_WIN
    HANDLE h;
#elif OS_LINUX
    FILE *f;
#endif

    File_Result init(ccstr path, u32 mode, File_Open_Mode open_mode);
    void cleanup();
    bool read(char *buf, s32 size, s32 *bytes_read = NULL);
    bool write(char *buf, s32 size, s32 *bytes_written = NULL);
    u32 seek(u32 pos);
};

void sleep_milliseconds(u32 milliseconds);

struct Select_File_Opts {
    cstr buf;
    u32 bufsize;
    bool folder;
    bool save;
};

bool let_user_select_file(Select_File_Opts* opts);
bool copy_file(ccstr src, ccstr dest, bool overwrite);
bool ensure_directory_exists(ccstr path);
bool delete_rm_rf(ccstr path);

cstr normalize_path_separator(cstr path, char sep = 0);
bool is_sep(char ch);

typedef fn<int(const void *a, const void *b)> compare_func;

void xplat_quicksort(void *list, s32 num, s32 size, compare_func cmp);
void *xplat_binary_search(const void *key, void *list, s32 num, s32 size, compare_func cmp);

u64 get_file_size(ccstr file);

struct Entire_File {
    u8 *data;
    s32 len;
};

Entire_File *read_entire_file(ccstr path);
void free_entire_file(Entire_File *file);
ccstr rel_to_abs_path(ccstr path);

enum Fs_Event_Type {
    FSEVENT_CHANGE,
    FSEVENT_DELETE,
    FSEVENT_CREATE,
    FSEVENT_RENAME,
};

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
#endif

    bool init(ccstr _path);
    void cleanup();

    bool initiate_wait();
    bool next_event(Fs_Event *event);
};

// this must be case-insensitive
ccstr get_path_relative_to(ccstr full, ccstr base);

ccstr get_canon_path(ccstr path);

