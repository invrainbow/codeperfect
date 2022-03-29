#include "os.hpp"

#if OS_MAC // whole file is mac only

#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <ftw.h>
#include <CoreServices/CoreServices.h>
#include "cwalk.h"

// for places where i can't be fucked to learn the linux
// way of doing things, just use c++ stdlib (requires c++17)
#include <filesystem>

#include "utils.hpp"
#include "defer.hpp"

Check_Path_Result check_path(ccstr path) {
    struct stat st;

    if (stat(path, &st) == -1)
        return CPR_NONEXISTENT;
    return S_ISDIR(st.st_mode) ? CPR_DIRECTORY : CPR_FILE;
}

ccstr get_normalized_path(ccstr path) {
    auto ret = realpath(path, NULL);
    if (!ret) return NULL;
    defer { free(ret); };

    return cp_strcpy(ret);
}

bool are_filepaths_same_file(ccstr path1, ccstr path2) {
    struct stat a, b;

    if (stat(path1, &a) == -1) return false;
    if (stat(path2, &b) == -1) return false;

    return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

u64 current_time_nano() {
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
    close(stdin_pipe_read);
    close(stdin_pipe_write);
    close(stdout_pipe_read);
    close(stdout_pipe_write);
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
    if (!peek_buffer_full) {
        if (!read1(&peek_buffer))
            return false;
        peek_buffer_full = true;
    }

    *ch = peek_buffer;
    return true;
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

bool Process::read1(char* out) {
    if (peek_buffer_full) {
        peek_buffer_full = false;
        *out = peek_buffer;
        return true;
    }

    char ch = 0;
    if (read(stdout_pipe_read, &ch, 1) == 1) {
        *out = ch;
        return true;
    }
    return false;
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
        strcpy_safe_fixed(info.name, ent->d_name);
        if (!cb(&info)) break;
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

void exit_thread(int retval) {
    pthread_exit((void*)retval);
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

bool copy_file(ccstr src, ccstr dest, bool overwrite) {
    auto flags = std::filesystem::copy_options::skip_existing;
    if (overwrite)
        flags = std::filesystem::copy_options::overwrite_existing;
    return std::filesystem::copy_file(src, dest, flags);
}

ccstr get_executable_path() {
    uint32_t size = 0;
    _NSGetExecutablePath(NULL, &size);
    if (!size) return NULL;

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

bool delete_file(ccstr path) {
    return (remove(path) == 0);
}

ccstr get_canon_path(ccstr path) {
    auto new_path = alloc_list<ccstr>();
    auto p = make_path(path);

    int extra_dotdots = 0;

    For (*p->parts) {
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
    For (*new_path)
        ret->append(it);

    Path pret;
    pret.init(ret);
    return pret.str(PATH_SEP);
}

ccstr rel_to_abs_path(ccstr path) {
    int size = 5;
    char *cwd;

    while (true) {
        Frame frame;
        cwd = alloc_array(char, size);
        if (getcwd(cwd, size)) break;

        frame.restore();
        size *= 5;
    }

    int len = cwk_path_get_absolute(cwd, path, NULL, 0);
    if (!len) return NULL;

    Frame frame;
    auto ret = alloc_array(char, len+1);

    if (cwk_path_get_absolute(cwd, path, ret, len+1) == 0) {
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

        if (result)
            lo = mid + 1;
        else if (result < 0)
            hi = mid - 1;
        else
            return curr;
    }
    return NULL;
}

bool create_directory(ccstr path) {
    return !mkdir(path, 0777);
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

    if (len)
        if (!create_actual_file_mapping(len))
            return false;

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

void Fs_Watcher::handle_event(size_t count, ccstr *paths) {
    for (size_t i = 0; i < count; i++) {
        auto relpath = get_path_relative_to(paths[i], path);
        if (streq(relpath, ".")) relpath = "";

        auto ev = events.append();
        strcpy_safe_fixed(ev->filepath, relpath);
    }
}

bool Fs_Watcher::platform_specific_init() {
    mem.init("fs_watcher mem");
    SCOPED_MEM(&mem);

    events.init();

    context = (void*)alloc_object(FSEventStreamContext);
    ((FSEventStreamContext*)context)->version = 0;
    ((FSEventStreamContext*)context)->info = this;
    ((FSEventStreamContext*)context)->retain = nullptr;
    ((FSEventStreamContext*)context)->release = nullptr;
    ((FSEventStreamContext*)context)->copyDescription = nullptr;

    auto paths = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFStringRef pathref = CFStringCreateWithCString(kCFAllocatorDefault, path, kCFStringEncodingUTF8);
    CFArrayAppendValue(paths, pathref);
    CFRelease(pathref);

    auto callback = [](auto, auto param, auto num, auto paths, auto flags, auto) {
        ((Fs_Watcher*)param)->handle_event(num, (ccstr*)paths);
    };

    // for now uncommenting kFSEventStreamCreateFlagIgnoreSelf so file changes via the ide are picked up
    // but in the future we really should just manually trigger the filechange callback when we are saving a file
    stream = (void*)FSEventStreamCreate(
        (CFAllocatorRef)NULL,
        callback,
        (FSEventStreamContext*)context,
        paths,
        kFSEventStreamEventIdSinceNow,
        0.5,
        kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagFileEvents
    );

    run_loop = (void*)CFRunLoopGetCurrent();
    FSEventStreamScheduleWithRunLoop((FSEventStreamRef)stream, (CFRunLoopRef)run_loop, kCFRunLoopDefaultMode);
    return FSEventStreamStart((FSEventStreamRef)stream);
}

void Fs_Watcher::cleanup() {
    if (stream) {
        FSEventStreamStop((FSEventStreamRef)stream);
        FSEventStreamUnscheduleFromRunLoop((FSEventStreamRef)stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        FSEventStreamRelease((FSEventStreamRef)stream);
    }

    mem.cleanup();
}

bool Fs_Watcher::next_event(Fs_Event *event) {
    if (curr >= events.len) {
        curr = 0;
        {
            mem.reset();
            SCOPED_MEM(&mem);
            events.init();
        }

        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
        if (!events.len)
            return false;
    }

    memcpy(event, &events[curr++], sizeof(Fs_Event));
    return true;
}

int cpu_count() {
    int mib[4];
    int ret = 1;
    size_t len = sizeof(ret);

    mib[0] = CTL_HW;
    mib[1] = HW_AVAILCPU;
    sysctl(mib, 2, &ret, &len, NULL, 0);
    if (ret >= 1) return ret;

    mib[1] = HW_NCPU;
    sysctl(mib, 2, &ret, &len, NULL, 0);
    if (ret >= 1) return ret;

    return 1;
}

#endif
