#include "os.hpp"

#if OS_LINUX

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <gtk/gtk.h>

// for places where i can't be fucked to learn the linux
// way of doing things, just use c++ stdlib (requires c++17)
#include <filesystem>
#include <chrono>

#include "utils.hpp"

Check_Path_Result check_path(ccstr path) {
    struct stat st;

    if (stat(path, &st) == -1)
        return CPR_NONEXISTENT;
    return S_ISDIR(st.st_mode) ? CPR_DIRECTORY : CPR_FILE;
}

u32 get_normalized_path(ccstr path, char *buf, u32 len) {
    auto ret = std::filesystem::absolute(path).c_str();
    auto retlen = strlen(ret);

    if (len == 0) return retlen + 1;
    if (!strcpy_safe(buf, len, ret)) return 0;

    return retlen;
}

bool are_filepaths_same_file(ccstr path1, ccstr path2) {
    struct stat a, b;

    if (stat(path1, &a) == -1) return false;
    if (stat(path2, &b) == -1) return false;

    return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

u64 current_time_in_nanoseconds() {
    // i cbf to figure out the linux way to do it, just use std::chrono
    // jblow can throw a floppy disk at my head, i'm frankly tired of this project

    auto start = std::chrono::high_resolution_clock::now();
    auto start_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(start);
    return (u64)start_ns.time_since_epoch().count();
}

// TODO: fill all this shit out
void Process::cleanup() {
    close(stdin_pipe[PIPE_READ]);
    close(stdin_pipe[PIPE_WRITE]);
    close(stdout_pipe[PIPE_READ]);
    close(stdout_pipe[PIPE_WRITE]);
}

void close_pipe_handle(int *fd) {
    if (*fd != 0) {
        close(*fd);
        *fd = 0;
    }
}

bool Process::run(ccstr _cmd) {
    ptr0(this);

    auto err = [&](ccstr s) -> bool {
        perror(s);
        cleanup();
        return false;
    };

    if (pipe(stdin_pipe) < 0) return err("allocating pipe for child input redirect");

    if (pipe(stdout_pipe) < 0) return err("allocating pipe for child output redirect");

    pid = fork();
    if (pid == -1) return err("forking");

    if (pid == 0) {
        // used by parent only
        close_pipe_handle(&stdin_pipe[PIPE_WRITE]);
        close_pipe_handle(&stdout_pipe[PIPE_READ]);

        if (dup2(stdin_pipe[PIPE_READ], STDIN_FILENO) == -1) exit(errno);
        if (dup2(stdout_pipe[PIPE_WRITE], STDOUT_FILENO) == -1) exit(errno);
        if (dup2(stdout_pipe[PIPE_WRITE], STDERR_FILENO) == -1) exit(errno);

        // we've duplicated, don't need anymore
        close_pipe_handle(&stdin_pipe[PIPE_READ]);
        close_pipe_handle(&stdout_pipe[PIPE_WRITE]);

        // run our command
        exit(execlp("/bin/bash", "bash", "-c", "echo 8", NULL));
    }

    // used by child only
    close_pipe_handle(&stdin_pipe[PIPE_READ]);
    close_pipe_handle(&stdout_pipe[PIPE_WRITE]);

    return true;
}

Process_Status Process::status() {
    int status;
    auto w = waitpid(pid, &status, WNOHANG);
    if (w == -1) return PROCESS_ERROR;

    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
        return PROCESS_DONE;
    }

    return PROCESS_WAITING;
}

bool Process::peek(char *ch) {
    if (peek_buffer_full)
        panic("can only peek 1 character at a time");

    if (!read1(&peek_buffer)) return false;

    peek_buffer_full = true;
    *ch = peek_buffer;
    return true;
}

bool Process::can_read() {
    struct timeval timeout = {0};

    fd_set fs;
    FD_ZERO(&fs);
    FD_SET(stdout_pipe[PIPE_READ], &fs);

    auto ret = select(FD_SETSIZE, &fs, NULL, NULL, &timeout);
    if (ret == -1) {
        error("something went wrong");
        return false;
    }
    return (ret > 0);
}

bool Process::read1(char* out) {
    if (peek_buffer_full) {
        peek_buffer_full = false;
        return peek_buffer;
    }

    char ch = 0;
    if (read(stdout_pipe[PIPE_READ], &ch, 1) == 1) {
        *out = ch;
        return true;
    }
    return false;
}

bool Process::write1(char ch) {
    return (write(stdin_pipe[PIPE_WRITE], &ch, 1) == 1);
}

void Process::flush() {
    // nothing to do here: https://stackoverflow.com/a/43188944
}

void Process::done_writing() {
    close_pipe_handle(&stdin_pipe[PIPE_WRITE]);
}

bool list_directory(ccstr folder, list_directory_cb cb) {
    auto dir = opendir(folder);
    if (dir == NULL) return false;
    defer { closedir(dir); };

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (streq(ent->d_name, ".")) continue;
        if (streq(ent->d_name, "..")) continue;

        Dir_Entry info;
        info.type = (ent->d_type == DT_DIR ? DIRENT_DIR : DIRENT_FILE);
        strcpy_safe(info.name, _countof(info.name), ent->d_name);
        cb(&info);
    }

    return true;
}

struct Thread_Ctx {
    Thread_Callback callback;
    void* param;
};

void* _run_thread(void* p) {
    auto ctx = (Thread_Ctx*)p;
    ctx->callback(ctx->param);
    our_free(ctx);
    return 0;
}

Thread_Handle create_thread(Thread_Callback callback, void* param) {
    auto ctx = (Thread_Ctx*)our_malloc(sizeof(Thread_Ctx));
    ctx->callback = callback;
    ctx->param = param;

    pthread_t h = 0;
    if (pthread_create(&h, NULL, _run_thread, (void*)ctx) == 0)
        return (Thread_Handle)h;
    return (Thread_Handle)0;
}

void close_thread_handle(Thread_Handle h) {
    return; // win32's CloseHandle isn't really a thing
}

void kill_thread(Thread_Handle h) {
    pthread_cancel((pthread_t)h);
}

void Lock::init() {
    pthread_mutex_init(&lock, NULL);
}

void Lock::cleanup() {
    pthread_mutex_destroy(&lock);
}

bool Lock::enter() {
    return pthread_mutex_trylock(&lock) == 0;
}

bool Lock::leave() {
    return pthread_mutex_unlock(&lock) == 0;
}

File_Result File::init(ccstr path, u32 mode, File_Open_Mode open_mode) {
    char open_mode_str[5] = {0};

    {
        u32 i = 0;

        // TODO: fill based on open_mode
        if (mode & FILE_MODE_READ) open_mode_str[i++] = 'r';
        if (mode & FILE_MODE_WRITE) open_mode_str[i++] = 'w';

        open_mode_str[i] = '\0';
    }

    f = fopen(path, open_mode_str);
    if (f == NULL) return FILE_RESULT_FAILURE;

    // TODO: handle the FILE_RESULT_ALREADY_EXISTS case.

    return FILE_RESULT_SUCCESS;
}

void File::cleanup() {
    fclose(f);
}

bool File::seek(u32 pos) {
    auto result = (pos == FILE_SEEK_END) ? fseek(f, 0, SEEK_END) : fseek(f, pos, SEEK_SET);
    return (result == 0);
}

bool File::read(char *buf, s32 size, s32 *bytes_read) {
    auto n = fread(buf, 1, size, f);
    if (n == 0) return false;
    if (bytes_read != NULL) *bytes_read = n;
    return (n == size);
}

bool File::write(char *buf, s32 size, s32 *bytes_written) {
    auto n = fwrite(buf, 1, size, f);
    if (n == 0) return false;
    if (bytes_written != NULL) *bytes_written = n;
    return (n == size);
}

void sleep_milliseconds(u32 milliseconds) {
    usleep((u64)milliseconds * 1000);
}

bool let_user_select_file(Select_File_Opts* opts) {
    if (!gtk_init_check(NULL, NULL)) return false;

    auto get_action = [&]() -> GtkFileChooserAction {
        if (opts->folder)
            return GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        if (opts->save)
            return GTK_FILE_CHOOSER_ACTION_OPEN;
        return GTK_FILE_CHOOSER_ACTION_SAVE;
    };

    auto dialog = gtk_file_chooser_dialog_new("Open File", NULL, get_action(), "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    defer { gtk_widget_destroy(dialog); };

    auto res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res != GTK_RESPONSE_ACCEPT) return false;

    auto filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    if (filename == NULL) return false;
    defer { g_free(filename); };

    strcpy_safe(opts->buf, opts->bufsize, filename);
    return true;
}

bool copy_file(ccstr src, ccstr dest, bool overwrite) {
    auto flags = std::filesystem::copy_options::skip_existing;
    if (overwrite)
        flags = std::filesystem::copy_options::overwrite_existing;
    return std::filesystem::copy_file(src, dest, flags);
}

#endif
