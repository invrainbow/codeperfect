#include "os.hpp"

#if OS_MAC || OS_LINUX // whole file is mac/linux

#include <filesystem>

#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <unistd.h>
#include <ftw.h>
#include <libgen.h>
#include "cwalk.h"

#include "utils.hpp"
#include "defer.hpp"
#include "world.hpp"

struct Thread_Ctx {
    Thread_Callback callback;
    void* param;
};

void* _run_thread(void* p) {
    auto ctx = (Thread_Ctx*)p;
    ctx->callback(ctx->param);
    cp_free(ctx);
    return 0;
}

Thread_Handle create_thread(Thread_Callback callback, void* param) {
    auto ctx = (Thread_Ctx*)cp_malloc(sizeof(Thread_Ctx));
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

NORETURN void exit_thread(int retval) {
    pthread_exit((void*)(uptr)retval);
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
    if (flags & O_CREAT) mode = 0644;

    fd = open(path, flags, mode);
    if (fd == -1) return FILE_RESULT_FAILURE;

    // TODO: handle the FILE_RESULT_ALREADY_EXISTS case.
    return FILE_RESULT_OK;
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
        if (n == -1 || !n) return false;
        off += n;
    }

    return true;
}

bool File::write(ccstr buf, s32 size) {
    int off = 0;

    while (off < size) {
        auto n = ::write(fd, buf + off, size - off);
        if (n == -1 || !n) return false;
        off += n;
    }

    return true;
}

// TODO: fill all this shit out
void Process::cleanup() {
    close(stdin_pipe_read);
    close(stdin_pipe_write);
    close(stdout_pipe_read);
    close(stdout_pipe_write);
}

void close_pipe_handle(int *fd) {
    if (*fd) {
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

    // create_new_console, keep_open_after_exit, skip_shell, and
    // dont_use_stdout are ignored.

    if (pipe(stdin_pipe) < 0) return err("allocating pipe for child input redirect");
    if (pipe(stdout_pipe) < 0) return err("allocating pipe for child output redirect");

    pid = fork();
    if (pid == -1) return err("forking");

    if (!pid) {
        // used by parent only
        close_pipe_handle(&stdin_pipe_write);
        close_pipe_handle(&stdout_pipe_read);

        if (dup2(stdin_pipe_read, STDIN_FILENO) == -1) exit(errno);
        if (dup2(stdout_pipe_write, STDOUT_FILENO) == -1) exit(errno);
        if (dup2(stdout_pipe_write, STDERR_FILENO) == -1) exit(errno);

        // we've duplicated, don't need anymore
        close_pipe_handle(&stdin_pipe_read);
        close_pipe_handle(&stdout_pipe_write);

        if (dir) chdir(dir);

        exit(execlp("/bin/bash", "bash", "-c", cmd, NULL));
    }

    // used by child only
    close_pipe_handle(&stdin_pipe_read);
    close_pipe_handle(&stdout_pipe_write);

    return true;
}

Process_Status Process::os_status() {
    int status;
    auto w = waitpid(pid, &status, WNOHANG);
    if (w == -1) return PROCESS_ERROR;

    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
        return PROCESS_DONE;
    }
    return PROCESS_WAITING;
}

bool Process::can_read() {
    struct timeval timeout = {0};

    fd_set fs;
    FD_ZERO(&fs);
    FD_SET(stdout_pipe_read, &fs);

    auto ret = select(FD_SETSIZE, &fs, NULL, NULL, &timeout);
    if (ret == -1) {
        error("something went wrong");
        return false;
    }
    return (ret > 0);
}

int Process::readn(char* buf, int n) {
    return read(stdout_pipe_read, buf, n);
}

bool Process::write1(char ch) {
    return (write(stdin_pipe_write, &ch, 1) == 1);
}

bool Process::writestr(ccstr s, s32 len) {
    if (!len) len = strlen(s);
    return (write(stdin_pipe_write, s, len) == len);
}

void Process::flush() {
    // nothing to do here: https://stackoverflow.com/a/43188944
}

void Process::done_writing() {
    close_pipe_handle(&stdin_pipe_write);
}

bool create_directory(ccstr path) {
    return !mkdir(path, 0777);
}

bool touch_file(ccstr path) {
    // commenting out O_EXCL - don't fail if file already exists (that's good)
    auto fd = open(path, O_CREAT /* | O_EXCL*/, 0644);
    if (fd == -1) return false;
    close(fd);
    return true;
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

    if (len && !create_actual_file_mapping(len)) return false;

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
    if (!newlen) return false;

    if (opts.write) {
        if (!flush(len)) return false;
        munmap(data, len); data = NULL;
    }

    if (!create_actual_file_mapping(newlen)) return false;

    len = newlen;
    return true;
}

void File_Mapping::cleanup() {
    if (data) {
        munmap(data, len);
        data = NULL;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }
}

ccstr _cp_dirname(ccstr path) {
    // dirname might overwrite mem we pass it
    // and might return pointer to statically allocated mem
    // so send it a copy and copy what it gives us
    return cp_strdup(dirname((char*)cp_strdup(path)));
}

ccstr cp_basename(ccstr path) {
    auto ret = cp_strdup(path);
    return cp_strdup(basename((char*)ret));
}

bool move_file_atomically(ccstr src, ccstr dest) {
    return rename(src, dest) == 0;
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

bool delete_file(ccstr path) {
    return (remove(path) == 0);
}

bool are_filepaths_same_file(ccstr path1, ccstr path2) {
    struct stat a, b;

    if (stat(path1, &a) == -1) return false;
    if (stat(path2, &b) == -1) return false;

    return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

Check_Path_Result check_path(ccstr path) {
    struct stat st;

    if (stat(path, &st) == -1)
        return CPR_NONEXISTENT;
    return S_ISDIR(st.st_mode) ? CPR_DIRECTORY : CPR_FILE;
}

bool list_directory(ccstr folder, list_directory_cb cb) {
    auto dir = opendir(folder);
    if (!dir) return false;
    defer { closedir(dir); };

    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (streq(ent->d_name, ".")) continue;
        if (streq(ent->d_name, "..")) continue;

        Dir_Entry info;
        info.type = (ent->d_type == DT_DIR ? DIRENT_DIR : DIRENT_FILE);
        cp_strcpy_fixed(info.name, ent->d_name);
        if (!cb(&info)) break;
    }

    return true;
}

ccstr get_canon_path(ccstr path) {
    auto new_path = alloc_list<ccstr>();
    auto p = make_path(path);

    int extra_dotdots = 0;

    For (p->parts) {
        if (streq(it, "")) continue;
        if (streq(it, ".")) continue;

        if (streq(it, "..")) {
            if (new_path->len > 0)
                new_path->len--;
            else
                extra_dotdots++;
            continue;
        }

        new_path->append(it);
    }

    auto ret = alloc_list<ccstr>();
    for (int i = 0; i < extra_dotdots; i++)
        ret->append("..");
    For (new_path)
        ret->append(it);

    Path pret;
    pret.init(ret);
    return pret.str(PATH_SEP);
}

ccstr get_normalized_path(ccstr path) {
    auto ret = realpath(path, NULL);
    if (!ret) return NULL;
    defer { free(ret); };

    return cp_strdup(ret);
}

// just add to utils.cpp? is there any reason to call win32 api version on
// windows
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

NORETURN void exit_from_crash_handler() {
    _exit(1);
}

#endif // OS_MAC || OS_LINUX
