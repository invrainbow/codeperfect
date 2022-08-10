#include "os.hpp"

#if OS_LINUX

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
// #include <gtk/gtk.h>

// for places where i can't be fucked to learn the linux
// way of doing things, just use c++ stdlib (requires c++17)
#include <filesystem>
#include <chrono>

#include "utils.hpp"
#include "defer.hpp"

u32 get_normalized_path(ccstr path, char *buf, u32 len) {
    auto ret = std::filesystem::absolute(path).c_str();
    auto retlen = strlen(ret);

    if (!len) return retlen + 1;
    if (!cp_strcpy(buf, len, ret)) return 0;

    return retlen;
}

u64 current_time_nano() {
    // i cbf to figure out the linux way to do it, just use std::chrono
    // jblow can throw a floppy disk at my head, i'm frankly tired of this project

    auto start = std::chrono::high_resolution_clock::now();
    auto start_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(start);
    return (u64)start_ns.time_since_epoch().count();
}

void sleep_milliseconds(u32 milliseconds) {
    usleep((u64)milliseconds * 1000);
}

// TODO: fix the gtk crap, now we're forced to use 4.0
#if 0
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
    if (!filename) return false;
    defer { g_free(filename); };

    cp_strcpy(opts->buf, opts->bufsize, filename);
    return true;
}
#endif

ccstr get_executable_path() {
    auto ret = alloc_array(char, PATH_MAX);
    auto count = readlink("/proc/self/exe", ret, PATH_MAX);
    if (count == -1) return NULL;
    return ret;
}

int _cmp_trampoline(const void *a, const void *b, void *param) {
    return (*(compare_func*)param)(a, b);
}

void xplat_quicksort(void *list, s32 num, s32 size, compare_func cmp) {
    qsort_r(list, num, size, _cmp_trampoline, &cmp);
}

#endif
