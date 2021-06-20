#include "os.hpp"

#if OS_MAC // whole file is mac only

#include <stdio.h>
#include <cwalk.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ftw.h>

// for places where i can't be fucked to learn the linux
// way of doing things, just use c++ stdlib (requires c++17)
#include <filesystem>

#include "utils.hpp"

Check_Path_Result check_path(ccstr path) {
    struct stat st;

    if (stat(path, &st) == -1)
        return CPR_NONEXISTENT;
    return S_ISDIR(st.st_mode) ? CPR_DIRECTORY : CPR_FILE;
}

ccstr get_normalized_path(ccstr path) {
    auto ret = realpath(path, NULL);
    if (ret == NULL) return NULL;
    defer { free(ret); };

    return our_strcpy(ret);
}

bool are_filepaths_same_file(ccstr path1, ccstr path2) {
    struct stat a, b;

    if (stat(path1, &a) == -1) return false;
    if (stat(path2, &b) == -1) return false;

    return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

void max_out_clock_frequency() {
    // nothing to do here for macos
}

u64 current_time_in_nanoseconds() {
    static uint64_t start_mach;
    static double frequency;
    static bool initialized = false;

    if (!initialized) {
        start_mach = mach_absolute_time();

        mach_timebase_info_data_t info;
        mach_timebase_info(&info);
        frequency = info.numer / info.denom;

        initialized = true;
    }

    return (u64)((mach_absolute_time() - start_mach) * frequency);
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
    cmd = _cmd;

    auto err = [&](ccstr s) -> bool {
        perror(s);
        cleanup();
        return false;
    };

    // create_new_console, keep_open_after_exit, and skip_shell are ignored.

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

        if (dir != NULL) chdir(dir);

        exit(execlp("/bin/bash", "bash", "-c", cmd, NULL));
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
        *out = peek_buffer;
        return true;
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

bool Process::writestr(ccstr s, s32 len) {
    if (len == 0) len = strlen(s);
    return (write(stdin_pipe[PIPE_WRITE], s, len) == len);
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

bool Lock::try_enter() {
    return pthread_mutex_trylock(&lock) == 0;
}

void Lock::enter() {
    pthread_mutex_lock(&lock);
}

void Lock::leave() {
    pthread_mutex_unlock(&lock);
}

File_Result File::init(ccstr path, int access, File_Open_Mode open_mode) {
    char open_mode_str[5] = {0};

    int flags = 0;
    if (access & FILE_MODE_READ) flags |= O_RDONLY;
    if (access & FILE_MODE_WRITE) flags |= O_WRONLY;
    if (open_mode == FILE_CREATE_NEW)
        flags |= (O_CREAT | O_TRUNC);

    int mode = 0;
    if (flags & O_CREAT)
        mode = 0644;

    fd = open(path, flags, mode);
    if (fd == -1) return FILE_RESULT_FAILURE;

    // TODO: handle the FILE_RESULT_ALREADY_EXISTS case.
    return FILE_RESULT_SUCCESS;
}

void File::cleanup() {
    close(fd);
}

u32 File::seek(u32 pos) {
    auto ret = (pos == FILE_SEEK_END ? lseek(fd, 0, SEEK_END) : lseek(fd, pos, SEEK_SET));
    if (ret == -1) return FILE_SEEK_ERROR;
    return ret;
}

bool File::read(char *buf, s32 size) {
    int off = 0;

    while (off < size) {
        auto n = ::read(fd, buf + off, size - off);
        if (n == -1 || n == 0) return false;
        off += n;
    }

    return true;
}

bool File::write(char *buf, s32 size) {
    int off = 0;

    while (off < size) {
        auto n = ::write(fd, buf + off, size - off);
        if (n == -1 || n == 0) return false;
        off += n;
    }

    return true;
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

/*
    if (type & MB_YESNO)
        dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, text );
    else
        dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, text );
*/

Ask_User_Result do_gtk_dialog(ccstr text, ccstr title, fn<void(GtkDialog*)> f) {
    if (!gtk_init_check(NULL, NULL)) return ASKUSER_ERROR;

    auto dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, text);

    f(GTK_DIALOG(dialog));

    gtk_window_set_title(GTK_WINDOW(dialog), title);
    auto result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(GTK_WIDGET(dialog));

    if (result == GTK_RESPONSE_DELETE_EVENT) return ASKUSER_NO;

    switch (result) {
    case ASKUSER_YES:
    case ASKUSER_NO:
    case ASKUSER_CANCEL:
        return (Ask_User_Result)result;
    }

    return ASKUSER_ERROR;
}

Ask_User_Result ask_user_yes_no_cancel(ccstr text, ccstr title) {
    return do_gtk_dialog(text, title, [&](auto dialog) {
        gtk_dialog_add_button(dialog, "Yes", ASKUSER_YES);
        gtk_dialog_add_button(dialog, "No", ASKUSER_NO);
        gtk_dialog_add_button(dialog, "Cancel", ASKUSER_CANCEL);
    });
}

Ask_User_Result ask_user_yes_no(ccstr text, ccstr title) {
    return do_gtk_dialog(text, title, [&](auto dialog) {
        gtk_dialog_add_button(dialog, "Yes", ASKUSER_YES);
        gtk_dialog_add_button(dialog, "No", ASKUSER_NO);
    });
}

void tell_user(ccstr text, ccstr title) {
    do_gtk_dialog(text, title, [&](auto dialog) {
        gtk_dialog_add_button(dialog, "OK", ASKUSER_YES);
    });
}

ccstr get_executable_path() {
    uint32_t size = 0;
    _NSGetExecutablePath(NULL, &size);
    if (size == 0) return NULL;

    auto ret = alloc_array(char, size+1);
    if (_NSGetExecutablePath(ret, &size) != 0) return NULL;

    return ret;
}

bool set_run_on_computer_startup(ccstr key, ccstr path_to_exe) {
    // TODO
    return true;
}

bool touch_file(ccstr path) {
    // commenting out O_EXCL - don't fail if file already exists (that's good)
    auto fd = open(path, O_CREAT /* | O_EXCL*/, 0644);
    if (fd == -1) return false;
    close(fd);
    return true;
}

bool xplat_chdir(ccstr dir) {
    return chdir(dir) == 0;
}

bool delete_rm_rf(ccstr path) {
    auto unlink_cb = [](const char *fpath, const struct stat*, int, struct FTW*) -> int {
        return remove(fpath);
    };
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS) == 0;
}

ccstr get_canon_path(ccstr path) {
    auto new_path = alloc_list<ccstr>();

    For (*make_path(path)->parts) {
        if (streq(it, "")) continue;
        if (streq(it, ".")) continue;

        if (streq(it, "..")) {
            new_path->len--;
            continue;
        }

        new_path->append(it);
    }

    Path p;
    p.init(new_path);
    return p.str(PATH_SEP);
}

ccstr rel_to_abs_path(ccstr path) {
    int size = 5;
    char *cwd;

    while (true) {
        Frame frame;
        cwd = alloc_array(char, size);
        if (getcwd(cwd, size) != NULL) break;

        frame.restore();
        size *= 5;
    }

    int len = cwk_path_get_absolute(cwd, path, NULL, 0);
    if (len == 0) return NULL;

    Frame frame;
    auto ret = alloc_array(char, len+1);

    if (cwk_path_get_absolute(cwd, path, NULL, 0) == 0) {
        frame.restore();
        return NULL;
    }

    return ret;
}

int _cmp_trampoline(void *param, const void *a, const void *b) {
    return (*(compare_func*)param)(a, b);
}

void xplat_quicksort(void *list, s32 num, s32 size, compare_func cmp) {
    qsort_r(list, num, size, &cmp, _cmp_trampoline);
}

// no bsearch_s, i guess i have to implement this myself...
void *xplat_binary_search(const void *key, void *list, s32 num, s32 size, compare_func cmp) {
    int lo = 0, hi = num-1;
    while (lo <= hi) {
        auto mid = (lo+hi)/2;
        auto curr = (void*)((char*)list + mid*size);
        auto result = cmp(key, curr);

        if (result > 0)
            lo = mid + 1;
        else if (result < 0)
            hi = mid - 1;
        else
            return curr;
    }
    return NULL;
}

bool create_directory(ccstr path) {
    return mkdir(path, 0777) == NULL;
}

void sleep_milliseconds(u32 ms) {
    struct timespec elapsed, tv;

    elapsed.tv_sec = ms / 1000;
    elapsed.tv_nsec = (ms % 1000) * 1000000;

    tv.tv_sec = elapsed.tv_sec;
    tv.tv_nsec = elapsed.tv_nsec;
    nanosleep(&tv, &elapsed);
}

bool move_file_atomically(ccstr src, ccstr dest) {
    return rename(src, dest) == 0;
}

ccstr get_path_relative_to(ccstr full, ccstr base) {
    auto pfull = make_path(full)->parts;
    auto pbase = make_path(base)->parts;

    int i = 0;
    for (; i < pfull->len && i < pbase->len; i++)
        if (!streqi(pfull->at(i), pbase->at(i)))
            break;

    int shared = i;

    auto ret = alloc_list<ccstr>();
    for (int i = shared; i < pbase->len; i++)
        ret->append("..");
    for (int i = shared; i < pfull->len; i++)
        ret->append(pfull->at(i));

    Path p;
    p.init(ret);
    return p.str();
}

bool File_Mapping::create_actual_file_mapping(i64 size) {
    if (opts.write) {
        lseek(fd, size-1, SEEK_SET);
        write(fd, "", 1);
        data = (u8*)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    } else {
        data = (u8*)mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    }

    if (data == MAP_FAILED) {
        error("create_actual_file_mapping: %s", get_last_error());
        data = NULL;
        return false;
    }
    return true;
}

bool File_Mapping::init(ccstr path, File_Mapping_Opts *_opts) {
    ptr0(this);
    fd = -1;

    bool ok = false;

    memcpy(&opts, _opts, sizeof(opts));

    fd = opts.write ? open(path, O_CREAT|O_RDWR|O_TRUNC, 0644) : open(path, O_RDONLY);
    if (fd == -1) {
        error("file_mapping::init > open: %s", get_last_error());
        return false;
    }
    defer { if (!ok) { close(fd); fd = -1; } };

    if (opts.write) {
        len = opts.initial_size;
    } else {
        struct stat statbuf;
        if (stat(path, &statbuf) != 0) return false;
        len = statbuf.st_size;
    }

    if (!create_actual_file_mapping(len)) return false;

    ok = true;
    return true;
}

bool File_Mapping::flush(i64 bytes_to_flush) {
    return msync(data, bytes_to_flush, MS_SYNC) == 0;
}

bool File_Mapping::finish_writing(i64 final_size) {
    if (!flush(final_size)) return false;

    munmap(data, len); data = NULL;
    auto ret = ftruncate(fd, final_size);
    close(fd); fd = -1;
    return ret;
}

bool File_Mapping::resize(i64 newlen) {
    if (opts.write) {
        if (!flush(len)) return false;
        munmap(data, len); data = NULL;
    }

    if (!create_actual_file_mapping(newlen)) return false;

    len = newlen;
    return true;
}

void File_Mapping::cleanup() {
    if (data != NULL) {
        munmap(data, len);
        data = NULL;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }
}

bool Fs_Watcher::init(ccstr _path) {
    ptr0(this);
    // TODO
    return true;
}

void Fs_Watcher::cleanup() {
    // TODO
}

bool Fs_Watcher::next_event(Fs_Event *event) {
    // TODO
    return false;
}

#endif
