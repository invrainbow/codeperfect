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
#include <libgen.h>
#include "cwalk.h"

// for places where i can't be fucked to learn the linux
// way of doing things, just use c++ stdlib (requires c++17)
#include <filesystem>

#include "utils.hpp"
#include "defer.hpp"

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

void sleep_milliseconds(u32 ms) {
    struct timespec elapsed, tv;

    elapsed.tv_sec = ms / 1000;
    elapsed.tv_nsec = (ms % 1000) * 1000000;

    tv.tv_sec = elapsed.tv_sec;
    tv.tv_nsec = elapsed.tv_nsec;
    nanosleep(&tv, &elapsed);
}

void Fs_Watcher::handle_event(size_t count, ccstr *paths) {
    for (size_t i = 0; i < count; i++) {
        auto relpath = get_path_relative_to(paths[i], path);
        if (streq(relpath, ".")) relpath = "";

        auto ev = events.append();
        cp_strcpy_fixed(ev->filepath, relpath);
    }
}

bool Fs_Watcher::platform_init() {
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

void Fs_Watcher::platform_cleanup() {
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
