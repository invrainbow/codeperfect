#include "os.hpp"

#if OS_LINUX

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <syslog.h>
#include <gtk/gtk.h>
#include <fontconfig/fontconfig.h>

// for places where i can't be fucked to learn the linux
// way of doing things, just use c++ stdlib (requires c++17)
#include <filesystem>
#include <chrono>

#include "world.hpp"
#include "utils.hpp"
#include "defer.hpp"

void Fs_Watcher::platform_cleanup() {
    if (epoll_fd && epoll_fd != -1) {
        if (ino_fd && ino_fd != -1)
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ino_fd, 0);
        close(epoll_fd);
        epoll_fd = 0;
    }

    if (ino_fd && ino_fd != -1) {
        close(ino_fd);
        ino_fd = 0;
    }
}

ccstr Fs_Watcher::wd_to_key(int wd) {
    return cp_sprintf("%d", wd);
}

void Fs_Watcher::wd_table_set(int wd, ccstr name) {
    SCOPED_MEM(&mem);
    auto table = (Table<ccstr>*)wd_table;
    print("setting %s %d", name, wd);
    table->set(wd_to_key(wd), cp_strdup(name));
}

ccstr Fs_Watcher::wd_table_get(int wd) {
    auto table = (Table<ccstr>*)wd_table;
    return table->get(wd_to_key(wd));
}

void Fs_Watcher::wd_table_remove(int wd) {
    auto table = (Table<ccstr>*)wd_table;
    table->remove(wd_to_key(wd));
}

bool Fs_Watcher::platform_init() {
    ino_fd = inotify_init1(IN_NONBLOCK);
    if (ino_fd == -1) return false;

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) return false;

    ptr0(&epoll_ev);
    epoll_ev.events = EPOLLIN | EPOLLET;
    epoll_ev.data.fd = ino_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ino_fd, &epoll_ev) == -1) return false;

    {
        SCOPED_MEM(&mem);
        auto table = alloc_object(Table<int>);
        table->init();
        wd_table = (void*)table;
    }

    auto wd = inotify_add_watch(ino_fd, path, IN_CREATE | IN_DELETE | IN_MODIFY);
    if (wd == -1) return false;

    wd_table_set(wd, path);

    fn<void(ccstr)> foo = [&](ccstr dirpath) {
        list_directory(dirpath, [&](auto it) -> bool {
            if (it->type != DIRENT_DIR) return true;

            auto folder = path_join(dirpath, it->name);

            // just continue i guess? should we fail?
            auto wd = inotify_add_watch(ino_fd, folder, IN_CREATE | IN_DELETE | IN_MODIFY);
            if (wd == -1) return true;

            wd_table_set(wd, folder);
            foo(folder);
            return true;
        });
    };

    foo(path);
    return true;
}

bool Fs_Watcher::next_event(Fs_Event *out) {
    if (curr_off + sizeof(struct inotify_event) > curr_len) {
        epoll_event ev; ptr0(&ev);
        auto nevents = epoll_wait(epoll_fd, &ev, 1, 0);
        if (nevents != 1) return false;

        auto len = read(ev.data.fd, curr_buf, _countof(curr_buf));
        if (len == -1) return false;

        curr_len = len;
        curr_off = 0;
    }

    auto it = (struct inotify_event*)(curr_buf + curr_off);
    if (!it->len) return false;
    if (it->wd == -1) return false;
    if (it->mask & IN_Q_OVERFLOW) return false;

    auto dir = wd_table_get(it->wd);
    if (!dir) return false;

    if (streq(it->name, "models.go")) {
        print("break here");
    }

    auto fullpath = path_join(dir, it->name);

#ifdef DEBUG_BUILD
    print("%s", fullpath); // TODO: delete
#endif

    do {
        if (!(it->mask & IN_ISDIR)) break;

        if (it->mask & IN_CREATE) {
            auto wd = inotify_add_watch(ino_fd, fullpath, IN_CREATE | IN_DELETE | IN_MODIFY);
            if (wd == -1) break;
            wd_table_set(wd, fullpath);
        } else if (it->mask & IN_DELETE) {
            inotify_rm_watch(ino_fd, it->wd);
            wd_table_remove(it->wd);
        }
    } while (0);

    bool ret = false;

    if (it->len + 1 <= _countof(out->filepath)) {
        cp_strcpy(out->filepath, _countof(out->filepath), fullpath);
        ret = true;
    }

    curr_off += sizeof(inotify_event) + it->len;
    return ret;
}

bool list_all_fonts(List<ccstr> *out) {
    auto config = FcInitLoadConfigAndFonts();
    if (!config) return false;

    auto pat = FcPatternCreate();
    if (!pat) return false;
    defer { FcPatternDestroy(pat); };

    auto objset = FcObjectSetBuild(FC_FAMILY, FC_LANG, NULL);
    if (!objset) return false;
    defer { FcObjectSetDestroy(objset); };

    auto fs = FcFontList(config, pat, objset);
    if (!fs) return NULL;
    defer { FcFontSetDestroy(fs); };

    for (int i = 0; i < fs->nfont; i++) {
        FcPattern* font = fs->fonts[i];
        FcChar8 *family = NULL;

        if (FcPatternGetString(font, FC_FAMILY, 0, &family) != FcResultMatch) continue;

        out->append(cp_strdup((char*)family));
    }

    return true;
}

Font_Data* load_font_data_by_name(ccstr name) {
    auto config = FcInitLoadConfigAndFonts();
    if (!config) return NULL;

    auto pat = FcNameParse((const FcChar8*)name);
    if (!pat) return NULL;
    defer { FcPatternDestroy(pat); };

    FcConfigSubstitute(config, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult res;
    auto font = FcFontMatch(config, pat, &res);
    if (!font) return NULL;
    defer { FcPatternDestroy(font); };

    FcChar8* filepath = NULL;
    if (FcPatternGetString(font, FC_FILE, 0, &filepath) != FcResultMatch) return NULL;

    auto fm = map_file_into_memory((char*)filepath);
    if (!fm) return NULL;

    auto ret = alloc_object(Font_Data);
    ret->type = FONT_DATA_MMAP;
    ret->fm = fm;
    return ret;
}

void init_platform_crap() {}

int run_dialog(ccstr text, ccstr title, fn<void(GtkDialog*)> cb) {
    if (!gtk_init_check(NULL, NULL)) return ASKUSER_ERROR;

	auto dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_NONE,
        "%s",
        text
    );

    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_keep_above(GTK_WINDOW(dialog), 1);
    gtk_window_present(GTK_WINDOW(dialog));
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), 1);

    cb(GTK_DIALOG(dialog));

	auto result = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	while (gtk_events_pending()) gtk_main_iteration();

    return result;
}

Ask_User_Result ask_user_yes_no(ccstr text, ccstr title, ccstr yeslabel, ccstr nolabel, bool cancel) {
    int result = run_dialog(text, title, [&](auto dialog) {
        gtk_dialog_add_button(GTK_DIALOG(dialog), yeslabel, GTK_RESPONSE_YES);
        gtk_dialog_add_button(GTK_DIALOG(dialog), nolabel, GTK_RESPONSE_NO);
        if (cancel) gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    });

    switch (result) {
    case GTK_RESPONSE_YES: return ASKUSER_YES;
    case GTK_RESPONSE_NO: return ASKUSER_NO;
    case GTK_RESPONSE_CANCEL: return ASKUSER_CANCEL;
    }
    return ASKUSER_ERROR;
}

void tell_user(ccstr text, ccstr title) {
    run_dialog(text, title, [&](auto dialog) {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "OK", GTK_RESPONSE_OK);
    });
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

bool let_user_select_file(Select_File_Opts* opts) {
    if (!gtk_init_check(NULL, NULL)) return false;

    auto get_action = [&]() -> GtkFileChooserAction {
        if (opts->folder) return GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        if (opts->save) return GTK_FILE_CHOOSER_ACTION_OPEN;
        return GTK_FILE_CHOOSER_ACTION_SAVE;
    };

    auto dialog = gtk_file_chooser_dialog_new("Open File", NULL, get_action(), "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    defer { gtk_widget_destroy(dialog); };

    // gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ALWAYS);
    // gtk_window_set_keep_above(GTK_WINDOW(dialog), 1);
    // gtk_window_present(GTK_WINDOW(dialog));
	// gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), 1);

    auto res = gtk_dialog_run(GTK_DIALOG(dialog));
	defer { gtk_widget_destroy(dialog); };
    if (res != GTK_RESPONSE_ACCEPT) return false;

	// while (gtk_events_pending()) gtk_main_iteration();

    auto filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    if (!filename) return false;
    defer { g_free(filename); };

    cp_strcpy(opts->buf, opts->bufsize, filename);
    return true;
}

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

void write_to_syslog(ccstr s) {
    static bool open = false;

    if (!open) {
        openlog("codeperfect", LOG_CONS | LOG_NDELAY | LOG_NOWAIT | LOG_PERROR, LOG_USER);
        open = true;
    }

    syslog(LOG_USER | LOG_ERR, "%s", s);
}

#endif
