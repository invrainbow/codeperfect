#include "os.hpp"

#ifdef OS_WIN // whole file is windows only

#include "win32.hpp"
#include "world.hpp"

#include <shlwapi.h>
#include <shlobj.h>
#include <stdlib.h>
#include <search.h>

Check_Path_Result check_path(ccstr path) {
    WIN32_FIND_DATAA find_data;
    HANDLE h = FindFirstFileA(path, &find_data);
    if (h != INVALID_HANDLE_VALUE) {
        FindClose(h);
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return CPR_DIRECTORY;
        return CPR_FILE;
    }
    return CPR_NONEXISTENT;
}

char* get_win32_error(DWORD error) {
    if (error == -1) error = GetLastError();

    char* str = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&str, 0, NULL);
    return str;
}

char* get_winsock_error() {
    char* str = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&str, 0, NULL);
    return str;
}

u32 get_normalized_path(ccstr path, char *buf, u32 len) {
    HANDLE f = CreateFileA(path, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;

    defer { CloseHandle(f); };

    auto result = GetFinalPathNameByHandleA(f, buf, len, FILE_NAME_NORMALIZED);
    if (result > 0 && buf != NULL && result + 1 <= len) {
        memmove(buf, buf + 4, sizeof(char) * (result - 4 + 1));
        return result - 4;
    }
    return result;
}

bool get_win32_file_info(ccstr path, BY_HANDLE_FILE_INFORMATION* out) {
    HANDLE h = CreateFileA(path, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    defer { CloseHandle(h); };
    return GetFileInformationByHandle(h, out);
}

bool are_filepaths_same_file(ccstr path1, ccstr path2) {
    BY_HANDLE_FILE_INFORMATION info1, info2;

    if (!get_win32_file_info(path1, &info1)) return false;
    if (!get_win32_file_info(path2, &info2)) return false;

    if (info1.dwVolumeSerialNumber != info2.dwVolumeSerialNumber) return false;
    if (info1.nFileIndexLow != info2.nFileIndexLow) return false;
    if (info1.nFileIndexHigh != info2.nFileIndexHigh) return false;

    return true;
}

u64 current_time_in_nanoseconds() {
    static LARGE_INTEGER freq = { 0 };

    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    now.QuadPart *= 1000000000;
    return now.QuadPart / freq.QuadPart;
}

void close_and_null_handle(HANDLE* ph) {
    if (*ph != NULL)
        CloseHandle(*ph);
    *ph = NULL;
}

void Process::cleanup() {
    if (status() == PROCESS_WAITING)
        TerminateProcess(proc, 0);

    close_and_null_handle(&stdin_r);
    close_and_null_handle(&stdin_w);
    close_and_null_handle(&stdout_r);
    close_and_null_handle(&stdout_w);
    close_and_null_handle(&proc);
}

// this is by far the stupidest bug ever
// https://devblogs.microsoft.com/oldnewthing/20200306-00/?p=103538
CRITICAL_SECTION global_create_process_lock;
void init_global_create_process_lock() {
    InitializeCriticalSection(&global_create_process_lock);
}
int _ = (init_global_create_process_lock(), 0);

bool Process::run(ccstr _cmd) {
    EnterCriticalSection(&global_create_process_lock);
    defer { LeaveCriticalSection(&global_create_process_lock); };

    cmd = _cmd;

    SECURITY_ATTRIBUTES sa = { 0 };
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    if (!dont_use_stdout && !CreatePipe(&stdout_r, &stdout_w, &sa, 0))
        return false;
    if (use_stdin && !CreatePipe(&stdin_r, &stdin_w, &sa, 0))
        return false;

    SetHandleInformation(stdout_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdin_w, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFOA si = { 0 };

    si.cb = sizeof(si);
    if (!dont_use_stdout) {
        si.hStdError = stdout_w;
        si.hStdOutput = stdout_w;
    }
    if (use_stdin)
        si.hStdInput = stdin_r;
    si.dwFlags = STARTF_USESTDHANDLES;

    {
        SCOPED_FRAME();

        // auto args = our_sprintf("cmd /S /K \"start cmd /S /K \"%s\"\"", cmd);
        auto args = our_sprintf("cmd /S /C %s\"%s\"", keep_open_after_exit ? "/K " : "", cmd);
        print("(Process::run) %s", args);
        if (!CreateProcessA(NULL, (LPSTR)args, NULL, NULL, TRUE, create_new_console ? CREATE_NEW_CONSOLE : 0, NULL, dir, &si, &pi)) {
            error("(CreateProcessA) error: %s", get_win32_error());
            return false;
        }
    }

    close_and_null_handle(&stdout_w);
    close_and_null_handle(&stdin_r);
    close_and_null_handle(&pi.hThread);

    proc = pi.hProcess;
    pid = pi.dwProcessId;

    return true;
}

Process_Status Process::status(bool allow_non_zero_exit_code) {
    switch (WaitForSingleObject(proc, 0)) {
        case WAIT_TIMEOUT:
            return PROCESS_WAITING;
        case WAIT_OBJECT_0:
            {
                DWORD code;
                GetExitCodeProcess(proc, &code);
                exit_code = (u32)code;
                return PROCESS_DONE;
            }
        case WAIT_FAILED:
            {
                print("process error: %s", get_win32_error());
            }
            return PROCESS_ERROR;
    }
    return PROCESS_ERROR;
}

bool Process::peek(char *ch) {
    DWORD read = 0, total, left;
    if (!PeekNamedPipe(stdout_r, ch, 1, &read, &total, &left))
        return false;
    return read == 1;
}

bool Process::can_read() {
    DWORD total = 0;
    if (!PeekNamedPipe(stdout_r, NULL, 0, NULL, &total, NULL))
        return false;
    return total > 0;
}

bool Process::read1(char* ch) {
    DWORD n = 0;
    bool ret = (ReadFile(stdout_r, ch, 1, &n, NULL) && (n == 1));
    if (!ret) {
        error("error reading from process: %s", get_last_error());
        print("another line to break on");
    }
    return ret;
}

bool Process::writestr(ccstr s, s32 len) {
    if (len == 0) len = strlen(s);
    DWORD n = 0;
    return WriteFile(stdin_w, s, len, &n, NULL) && (n == len);
}

bool Process::write1(char ch) {
    DWORD n = 0;
    return WriteFile(stdin_w, &ch, 1, &n, NULL) && (n == 1);
}

void Process::flush() {
    FlushFileBuffers(stdin_w);
}

void Process::done_writing() {
    close_and_null_handle(&stdin_w);
}

bool list_directory(ccstr folder, list_directory_cb cb) {
    WIN32_FIND_DATAA find_data;

    auto find = FindFirstFileA(path_join(folder, "*"), &find_data);
    if (find == INVALID_HANDLE_VALUE) return false;
    defer { FindClose(find); };

    do {
        if (streq(find_data.cFileName, ".")) continue;
        if (streq(find_data.cFileName, "..")) continue;

        Dir_Entry info;
        info.type = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? DIRENT_DIR : DIRENT_FILE);
        strcpy_s(info.name, _countof(info.name), find_data.cFileName);

        cb(&info);
    } while (FindNextFileA(find, &find_data));

    return true;
}

struct Thread_Ctx {
    Thread_Callback callback;
    void* param;
};

Thread_Handle create_thread(Thread_Callback callback, void* param) {
    auto ctx = (Thread_Ctx*)our_malloc(sizeof(Thread_Ctx));
    ctx->callback = callback;
    ctx->param = param;

    auto run = [](void *p) -> DWORD WINAPI {
        auto ctx = (Thread_Ctx*)p;
        ctx->callback(ctx->param);
        our_free(ctx);
        return 0;
    };

    return (Thread_Handle)CreateThread(NULL, 0, run, ctx, 0, NULL);
}

void close_thread_handle(Thread_Handle h) {
    CloseHandle(h);
}

void kill_thread(Thread_Handle h) {
    TerminateThread(h, 0);
}

void Lock::init() {
    ptr0(this);
    mutex = CreateMutex(NULL, FALSE, NULL);
}

void Lock::cleanup() {
    CloseHandle(mutex);
}

bool Lock::try_enter() {
    auto res = WaitForSingleObject(mutex, 0);
    if (res == WAIT_ABANDONED)
        panic("mutex abandoned");
    return res == WAIT_OBJECT_0;
}

bool Lock::enter() {
    auto res = WaitForSingleObject(mutex, INFINITE);
    if (res != WAIT_OBJECT_0) {
        print("res: %d", res);
        panic("failed to enter mutex");
    }
    return true;
}

bool Lock::leave() {
    return ReleaseMutex(mutex);
}

void File::cleanup() {
    CloseHandle(h);
}

File_Result File::init(ccstr path, u32 mode, File_Open_Mode open_mode) {
    auto get_win32_open_mode = [&]() {
        switch (open_mode) {
            case FILE_OPEN_EXISTING: return OPEN_EXISTING;
            case FILE_CREATE_NEW: return CREATE_ALWAYS;
        }
        return 0;
    };

    DWORD win32_mode = 0;
    if (mode & FILE_MODE_READ) win32_mode |= GENERIC_READ;
    if (mode & FILE_MODE_WRITE) win32_mode |= GENERIC_WRITE;

    h = CreateFileA(path, win32_mode, 0, NULL, get_win32_open_mode(), FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) return FILE_RESULT_SUCCESS;
    if (GetLastError() == ERROR_FILE_EXISTS) return FILE_RESULT_ALREADY_EXISTS;

    print("%s", get_last_error());
    return FILE_RESULT_FAILURE;
}

u32 File::seek(u32 pos) {
    if (pos == FILE_SEEK_END)
        return SetFilePointer(h, 0, NULL, FILE_END);
    return SetFilePointer(h, pos, NULL, FILE_BEGIN);
}

bool File::read(char *buf, s32 size, s32 *bytes_read) {
    DWORD n = 0;

    if (!ReadFile(h, buf, size, &n, NULL)) return false;

    if (bytes_read != NULL)
        *bytes_read = (s32)n;
    return (n == size);
}

bool File::write(char *buf, s32 size, s32 *bytes_written) {
    DWORD n = 0;
    if (!WriteFile(h, buf, size, &n, NULL)) return false;

    if (bytes_written != NULL)
        *bytes_written = (s32)n;
    return (n == size);
}

void sleep_milliseconds(u32 milliseconds) {
    Sleep((DWORD)milliseconds);
}

wchar_t* ansi_to_unicode(ccstr s) {
    auto len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (len == 0) return NULL;

    auto ret = alloc_array(wchar_t, len);
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, ret, len) != len) return NULL;
    return ret;
}

bool let_user_select_file(Select_File_Opts* opts) {
    SCOPED_FRAME();

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return false;
    defer { CoUninitialize(); };

    IFileDialog* dialog = NULL;
    auto clsid = (opts->save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog);
    auto iid = (opts->save ? IID_IFileSaveDialog : IID_IFileOpenDialog);

    hr = CoCreateInstance(clsid, NULL, CLSCTX_ALL, iid, (void**)(&dialog));
    if (FAILED(hr)) return false;
    defer { dialog->Release(); };

    if (opts->folder)
        if (FAILED(dialog->SetOptions(FOS_PICKFOLDERS)))
            return false;

    IShellItem *default_folder = NULL;
    defer { if (default_folder != NULL) default_folder->Release(); };

    if (opts->starting_folder != NULL) {
        if (FAILED(SHCreateItemFromParsingName(ansi_to_unicode(opts->starting_folder), NULL, IID_IShellItem, (void**)&default_folder)))
            return false;
        if (FAILED(dialog->SetDefaultFolder(default_folder)))
            return false;
    }

    if (FAILED(dialog->Show(NULL))) return false;

    IShellItem *item;
    hr = dialog->GetResult(&item);
    if (FAILED(hr)) return false;
    defer { item->Release(); };

    PWSTR path;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return false;
    defer { CoTaskMemFree(path); };

    u32 i = 0;
    for (; i < opts->bufsize - 1 && path[i] != '\0'; i++)
        opts->buf[i] = (char)path[i];
    opts->buf[i] = '\0';
    return true;
}

bool copy_file(ccstr src, ccstr dest, bool overwrite) {
    return CopyFileA(src, dest, !overwrite);
}

bool ensure_directory_exists(ccstr path) {
    SCOPED_FRAME();

    auto newpath = normalize_path_sep(path);
    auto res = SHCreateDirectoryExA(NULL, (ccstr)newpath, NULL);

    switch (res) {
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
    case ERROR_SUCCESS:
        return true;
    }

    print("ensure_directory_exists error: %s", Win32_Error(get_win32_error(res)).str);
    return false;
}

bool delete_rm_rf(ccstr path) {
    auto hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return false;
    defer { CoUninitialize(); };

    IFileOperation* op = NULL;

    hr = CoCreateInstance(CLSID_FileOperation, NULL, CLSCTX_ALL, IID_PPV_ARGS(&op));
    if (FAILED(hr)) return false;
    defer { op->Release(); };

    if (FAILED(op->SetOperationFlags(FOF_NO_UI))) return false;

    IShellItem* item = NULL;

    {
        SCOPED_FRAME();
        auto wpath = ansi_to_unicode(path);
        hr = SHCreateItemFromParsingName(wpath, NULL, IID_PPV_ARGS(&item));
    }

    if (FAILED(hr)) {
        // file doesn't exist, so from the perspective of `rm -rf`, we succeeded
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            return true;
        return false;
    }

    defer { item->Release(); };

    if (FAILED(op->DeleteItem(item, NULL))) return false;
    if (FAILED(op->PerformOperations())) return false;

    return true;
}


int _cmp_trampoline(void *param, const void *a, const void *b) {
    return (*(compare_func*)param)(a, b);
}

void xplat_quicksort(void *list, s32 num, s32 size, compare_func cmp) {
    qsort_s(list, num, size, _cmp_trampoline, &cmp);
}

void *xplat_binary_search(const void *key, void *list, s32 num, s32 size, compare_func cmp) {
    return bsearch_s(key, list, num, size, _cmp_trampoline, &cmp);
}

#define GET_FILE_SIZE_ERROR ((u64)-1)

u64 _get_file_size(HANDLE f) {
    LARGE_INTEGER size;
    if (!GetFileSizeEx(f, &size)) return GET_FILE_SIZE_ERROR;
    return (u64)size.QuadPart;
}

u64 get_file_size(ccstr path) {
    HANDLE f = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return GET_FILE_SIZE_ERROR;
    defer { CloseHandle(f); };
    return _get_file_size(f);
}

ccstr rel_to_abs_path(ccstr path) {
    auto len = GetFullPathNameA(path, 0, NULL, NULL);
    if (len == 0) return NULL;

    Frame frame;
    auto ret = alloc_array(char, len);
    if (GetFullPathNameA(path, len, ret, NULL) != len) {
        frame.restore();
        return NULL;
    }
    return ret;
}

const s32 FS_WATCHER_BUFSIZE = 1024 * 32;

bool Fs_Watcher::init(ccstr _path) {
    ptr0(this);

    path = _path;
    dir_handle = CreateFileA(path, FILE_LIST_DIRECTORY, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    if (dir_handle == INVALID_HANDLE_VALUE) {
        print("unable to create file for fswatcher: %s", get_last_error());
        return false;
    }

    buf = alloc_array(FILE_NOTIFY_INFORMATION, FS_WATCHER_BUFSIZE);
    initiate_wait();
    return true;
}

void Fs_Watcher::cleanup() {
    if (dir_handle != NULL) CloseHandle(dir_handle);
}

bool Fs_Watcher::initiate_wait() {
    auto flags = FILE_NOTIFY_CHANGE_FILE_NAME |
        FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_ATTRIBUTES |
        FILE_NOTIFY_CHANGE_LAST_WRITE |
        FILE_NOTIFY_CHANGE_CREATION;

    if (!ReadDirectoryChangesW(dir_handle, buf, sizeof(FILE_NOTIFY_INFORMATION) * FS_WATCHER_BUFSIZE, TRUE, flags, NULL, &ol, NULL)) {
        error("unable to read directory changes: %s", get_last_error());
        return false;
    }
    return true;
}

bool Fs_Watcher::next_event(Fs_Event *event) {
    if (!has_more) {
        DWORD bytes_read = 0;
        if (!GetOverlappedResult(dir_handle, &ol, &bytes_read, FALSE))
            return false;

        initiate_wait();

        if (bytes_read == 0) {
            print("number of events overflowed our FILE_NOTIFY_INFORMATION buffer");
            return false;
        }

        offset = 0;
        has_more = true;
    }

    auto copy_file_name = [&](FILE_NOTIFY_INFORMATION *info, char *dest, s32 destsize) {
        // support unicode later
        // and to be honest maybe our string class should just handle seamless ansi/unicode transition lol
        auto len = info->FileNameLength / sizeof(WCHAR);
        u32 i = 0;
        for (; i < len && i < destsize-1; i++)
            dest[i] = (char)info->FileName[i];
        dest[i] = 0;
    };

    auto info = (FILE_NOTIFY_INFORMATION*)((u8*)buf + offset);
    if (info->Action == FILE_ACTION_RENAMED_NEW_NAME)
        return next_event(event);   // we shouldn't be here, ask for next event lmao

    ptr0(event);

    copy_file_name(info, event->filepath, _countof(event->filepath));
    switch (info->Action) {
    case FILE_ACTION_ADDED:
        event->type = FSEVENT_CREATE;
        break;
    case FILE_ACTION_REMOVED:
        event->type = FSEVENT_DELETE;
        break;
    case FILE_ACTION_MODIFIED:
        event->type = FSEVENT_CHANGE;
        break;
    case FILE_ACTION_RENAMED_OLD_NAME:
        event->type = FSEVENT_RENAME;
        if (info->NextEntryOffset == 0) return false;

        offset += info->NextEntryOffset;
        info = (FILE_NOTIFY_INFORMATION*)((u8*)buf + offset);

        if (info->Action != FILE_ACTION_RENAMED_NEW_NAME) return false;

        copy_file_name(info, event->new_filepath, _countof(event->new_filepath));
        break;
    }

    if (info->NextEntryOffset == 0)
        has_more = false;
    else
        offset += info->NextEntryOffset;

    return true;
}

Entire_File *read_entire_file(ccstr path) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    defer { CloseHandle(f); };

    LARGE_INTEGER size;
    if (!GetFileSizeEx(f, &size)) return NULL;
    if (size.HighPart > 0) return NULL;

    auto flen = size.LowPart;

    HANDLE map = CreateFileMapping(f, 0, PAGE_READONLY, 0, flen, NULL);
    if (map == NULL) return NULL;
    defer { CloseHandle(map); };

    auto buf = MapViewOfFile(map, FILE_SHARE_READ, 0, 0, flen);
    if (buf == NULL) return NULL;

    auto ret = alloc_object(Entire_File);
    ret->data = (u8*)buf;
    ret->len = flen;
    return ret;
}

void free_entire_file(Entire_File *file) {
    UnmapViewOfFile(file->data);
}

ccstr get_path_relative_to(ccstr full, ccstr base) {
    Frame frame;

    auto full2 = normalize_path_sep(full);
    auto base2 = normalize_path_sep(base);
    auto buf = alloc_array(char, MAX_PATH+1);

    if (!PathRelativePathToA(buf, base2, FILE_ATTRIBUTE_DIRECTORY, full2, 0)) {
        frame.restore();
        return NULL;
    }

    // remove the "./" or ".\\" at the beginning
    if (buf[0] == '.' && is_sep(buf[1])) buf += 2;

    return buf;
}

ccstr get_canon_path(ccstr path) {
    Frame frame;
    auto ret = alloc_array(char, MAX_PATH+1);
    if (!PathCanonicalizeA(ret, path)) {
        frame.restore();
        return NULL;
    }
    return ret;
}

bool move_file_atomically(ccstr src, ccstr dest) {
    SCOPED_FRAME();

    auto h = CreateFile(dest, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);

    return ReplaceFileA(dest, src, NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL);
}

void *read_font_data_from_name(s32 *len, ccstr name) {
    auto hfont = CreateFontA(
        0, 0, 0, 0, 0, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
        name
    );
    if (hfont == NULL) return NULL;
    defer { DeleteObject(hfont); };

    auto hdc = CreateCompatibleDC(NULL);
    if (hdc == NULL) return NULL;
    defer { DeleteDC(hdc); };

    SelectObject(hdc, hfont);
    auto size = GetFontData(hdc, 0, 0, NULL, 0);
    if (size == 0) return NULL;

    Frame frame;

    auto ret = alloc_array(char, size);
    if (GetFontData(hdc, 0, 0, ret, size) != size) {
        frame.restore();
        return NULL;
    }

    *len = size;
    return ret;
}

void *read_font_data_from_first_found(s32 *plen, ...) {
    va_list vl;
    va_start(vl, plen);

    void *font_data = NULL;
    s32 len = 0;

    ccstr curr = NULL;
    while ((curr = va_arg(vl, ccstr)) != NULL) {
        Frame frame;
        font_data = read_font_data_from_name(&len, curr);
        if (font_data != NULL) break;
        frame.restore();
    }

    va_end(vl);

    if (font_data != NULL)
        *plen = len;
    return font_data;
}

Ask_User_Result ask_user_yes_no_cancel(void* parent_window, ccstr text, ccstr title) {
    int ret = MessageBoxA((HWND)parent_window, text, title, MB_YESNOCANCEL | MB_ICONEXCLAMATION | MB_TOPMOST);
    switch (ret) {
    case IDYES: return ASKUSER_YES;
    case IDNO: return ASKUSER_NO;
    case IDCANCEL: return ASKUSER_CANCEL;
    }
    return ASKUSER_ERROR;
}

#endif
