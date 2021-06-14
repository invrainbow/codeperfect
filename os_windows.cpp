#include "os.hpp"

#ifdef OS_WIN // whole file is windows only

#include "win32.hpp"
#include "world.hpp"

#include <shlwapi.h>
#include <shlobj.h>
#include <stdlib.h>
#include <search.h>
#include <pathcch.h>

// stupid character conversion functions
// =====================================

wchar_t* to_wide(ccstr s, int slen = -1) {
    auto len = MultiByteToWideChar(CP_UTF8, 0, s, slen, NULL, 0);
    if (len == 0) return NULL;

    auto ret = alloc_array(wchar_t, len + 1);
    if (MultiByteToWideChar(CP_UTF8, 0, s, slen, ret, len) != len) return NULL;

    ret[len] = L'\0';
    return ret;
}

ccstr to_utf8(const wchar_t *s, int slen = -1) {
    auto len = WideCharToMultiByte(CP_UTF8, 0, s, slen, NULL, 0, NULL, NULL);
    if (len == 0) return NULL;

    auto ret = alloc_array(char, len+1);
    if (WideCharToMultiByte(CP_UTF8, 0, s, slen, ret, len, NULL, NULL) != len) return NULL;

    ret[len] = '\0';
    return ret;
}

// actual code that does things -__-
// =================================

Check_Path_Result check_path(ccstr path) {
    SCOPED_FRAME();

    WIN32_FIND_DATAW find_data;
    HANDLE h = FindFirstFileW(to_wide(path), &find_data);
    if (h != INVALID_HANDLE_VALUE) {
        FindClose(h);
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return CPR_DIRECTORY;
        return CPR_FILE;
    }
    return CPR_NONEXISTENT;
}

ccstr get_win32_error(DWORD errcode) {
    if (errcode == -1) errcode = GetLastError();

    wchar_t *str = NULL;
    if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&str, 0, NULL) == 0)
        return NULL;
    defer { LocalFree(str); };

    return to_utf8(str);
}

ccstr get_normalized_path(ccstr path) {
    HANDLE f = CreateFileW(to_wide(path), 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;

    defer { CloseHandle(f); };

    auto len = GetFinalPathNameByHandleW(f, NULL, 0, FILE_NAME_NORMALIZED);
    if (len == 0) return NULL;

    Frame frame;

    auto buf = alloc_array(wchar_t, len);
    if (GetFinalPathNameByHandleW(f, buf, len, FILE_NAME_NORMALIZED) == 0) {
        frame.restore();
        print("GetFinalPathNameByHandleW: %s", get_last_error());
        return NULL;
    }

    memmove(buf, buf + 4, sizeof(wchar_t) * (len - 4 + 1));
    return to_utf8(buf);
}

bool get_win32_file_info(ccstr path, BY_HANDLE_FILE_INFORMATION* out) {
    SCOPED_FRAME();

    HANDLE h = CreateFileW(to_wide(path), 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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

bool Process::run(ccstr _cmd) {
    // this is by far the stupidest bug ever
    // https://devblogs.microsoft.com/oldnewthing/20200306-00/?p=103538
    static CRITICAL_SECTION create_process_lock;
    static bool initialized = false;

    if (!initialized) {
        InitializeCriticalSection(&create_process_lock);
        initialized = true;
    }

    EnterCriticalSection(&create_process_lock);
    defer { LeaveCriticalSection(&create_process_lock); };

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
    STARTUPINFOW si = { 0 };

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

        wchar_t *wargs = NULL;
        if (skip_shell) {
            wargs = to_wide(cmd);
        } else {
            // auto args = our_sprintf("cmd /S /K \"start cmd /S /K \"%s\"\"", cmd);
            auto args = our_sprintf("cmd /S /C %s\"%s\"", keep_open_after_exit ? "/K " : "", cmd);
            print("Process::run: %s", args);
            wargs = to_wide(args);
        }

        auto wdir = to_wide(dir);
        if (!CreateProcessW(NULL, wargs, NULL, NULL, TRUE, create_new_console ? CREATE_NEW_CONSOLE : 0, NULL, wdir, &si, &pi)) {
            error("CreateProcessW: %s", get_last_error());
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

Process_Status Process::status() {
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
                error("process error: %s", get_last_error());
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
    return ReadFile(stdout_r, ch, 1, &n, NULL) && (n == 1);
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
    WIN32_FIND_DATAW find_data;

    auto find = FindFirstFileW(to_wide(path_join(folder, "*")), &find_data);
    if (find == INVALID_HANDLE_VALUE) return false;
    defer { FindClose(find); };

    do {
        auto filename = to_utf8(find_data.cFileName);

        if (streq(filename, ".")) continue;
        if (streq(filename, "..")) continue;

        Dir_Entry info;
        info.type = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? DIRENT_DIR : DIRENT_FILE);
        strcpy_safe(info.name, _countof(info.name), filename);

        cb(&info);
    } while (FindNextFileW(find, &find_data));

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
    InitializeCriticalSection(&lock);
}

void Lock::cleanup() {
    DeleteCriticalSection(&lock);
}

bool Lock::try_enter() {
    return TryEnterCriticalSection(&lock);
}

bool Lock::enter() {
    EnterCriticalSection(&lock);
    return true;
}

bool Lock::leave() {
    LeaveCriticalSection(&lock);
    return true;
}

void File::cleanup() {
    CloseHandle(h);
}

HANDLE create_win32_file_handle(ccstr path, int access, File_Open_Mode open_mode) {
    auto get_win32_open_mode = [&]() {
        switch (open_mode) {
            case FILE_OPEN_EXISTING: return OPEN_EXISTING;
            case FILE_CREATE_NEW: return CREATE_ALWAYS;
        }
        return 0;
    };

    DWORD win32_mode = 0;
    if (access & FILE_MODE_READ) win32_mode |= GENERIC_READ;
    if (access & FILE_MODE_WRITE) win32_mode |= GENERIC_WRITE | GENERIC_READ;

    SCOPED_FRAME();
    return CreateFileW(to_wide(path), win32_mode, 0, NULL, get_win32_open_mode(), FILE_ATTRIBUTE_NORMAL, NULL);
}

File_Result File::init(ccstr path, int access, File_Open_Mode open_mode) {
    h = create_win32_file_handle(path, access, open_mode);
    if (h != INVALID_HANDLE_VALUE) return FILE_RESULT_SUCCESS;
    if (GetLastError() == ERROR_FILE_EXISTS) return FILE_RESULT_ALREADY_EXISTS;

    print("File::init: error opening %s: %s", path, get_last_error());
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
        if (FAILED(SHCreateItemFromParsingName(to_wide(opts->starting_folder), NULL, IID_IShellItem, (void**)&default_folder)))
            return false;
        if (FAILED(dialog->SetDefaultFolder(default_folder)))
            return false;
    }

    if (FAILED(dialog->Show(NULL))) return false;

    IShellItem *item;
    hr = dialog->GetResult(&item);
    if (FAILED(hr)) return false;
    defer { item->Release(); };

    PWSTR wpath;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &wpath))) return false;
    defer { CoTaskMemFree(wpath); };

    strcpy_safe(opts->buf, opts->bufsize, to_utf8(wpath));
    return true;
}

bool copy_file(ccstr src, ccstr dest, bool overwrite) {
    SCOPED_FRAME();
    return CopyFileW(to_wide(src), to_wide(dest), !overwrite);
}

bool ensure_directory_exists(ccstr path) {
    SCOPED_FRAME();

    auto newpath = normalize_path_sep(path);
    auto res = SHCreateDirectoryExW(NULL, to_wide(newpath), NULL);

    switch (res) {
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
    case ERROR_SUCCESS:
        return true;
    }

    error("ensure_directory_exists: %s", get_specific_error(res));
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
        auto wpath = to_wide(path);
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
    HANDLE f = CreateFileW(to_wide(path), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return GET_FILE_SIZE_ERROR;
    defer { CloseHandle(f); };
    return _get_file_size(f);
}

ccstr rel_to_abs_path(ccstr path) {
    auto wpath = to_wide(path);

    auto len = GetFullPathNameW(wpath, 0, NULL, NULL);
    if (len == 0) return NULL;

    Frame frame;
    auto ret = alloc_array(wchar_t, len);
    if (GetFullPathNameW(wpath, len, ret, NULL) != len) {
        frame.restore();
        return NULL;
    }

    return to_utf8(ret);
}

const s32 FS_WATCHER_BUFSIZE = 1024 * 32;

bool Fs_Watcher::init(ccstr _path) {
    ptr0(this);

    path = _path;
    dir_handle = CreateFileW(to_wide(path), FILE_LIST_DIRECTORY, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
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
        strcpy_safe(dest, destsize, to_utf8(info->FileName, info->FileNameLength / sizeof(WCHAR)));
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

bool File_Mapping::create_actual_file_mapping(bool write, LARGE_INTEGER size) {
    if (size.HighPart > 0) {
        // TODO: handle this. Everywhere else in the code is set up to handle
        // 64-bit sizes, but because MapViewOfFile only lets you map a 32-bit
        // sized portion at a time, we need to write the unmap/remap logic.
        panic("File to be mapped is too big.");
    }

    bool ok = false;

    mapping = CreateFileMapping(file, 0, write ? PAGE_READWRITE : PAGE_READONLY, size.HighPart, size.LowPart, NULL);
    if (mapping == NULL) {
        error("CreateFileMapping: %s", get_last_error());
        return false;
    }

    defer {
        if (!ok) {
            CloseHandle(mapping);
            mapping = NULL;
        }
    };

    data = (u8*)MapViewOfFile(mapping, write ? FILE_MAP_WRITE : FILE_MAP_READ, 0, 0, size.LowPart);
    if (data == NULL) return false;

    len = size.LowPart;

    ok = true;
    return true;
}

bool File_Mapping::init(ccstr path) {
    File_Mapping_Opts opts = {0};
    opts.write = false;
    opts.open_mode = FILE_OPEN_EXISTING;
    return init(path, &opts);
}

bool File_Mapping::init(ccstr path, File_Mapping_Opts *_opts) {
    bool ok = false;

    memcpy(&opts, _opts, sizeof(opts));

    file = create_win32_file_handle(path, opts.write ? FILE_MODE_WRITE : FILE_MODE_READ, opts.open_mode);
    if (file == INVALID_HANDLE_VALUE) return false;

    defer {
        if (!ok) {
            CloseHandle(file);
            file = NULL;
        }
    };

    LARGE_INTEGER size = {0};
    if (opts.initial_size > 0) {
        size.QuadPart = opts.initial_size;
    } else {
        if (!GetFileSizeEx(file, &size)) return false;
    }

    if (!create_actual_file_mapping(opts.write, size))
        return false;

    ok = true;
    return true;
}

bool File_Mapping::resize(i64 newlen) {
    if (opts.write) {
        if (!flush(len)) return false;

        UnmapViewOfFile(data); data = NULL;
        CloseHandle(mapping); mapping = NULL;
    }

    LARGE_INTEGER li;
    li.QuadPart = newlen;
    return create_actual_file_mapping(opts.write, li);
}

bool File_Mapping::flush(i64 bytes_to_flush) {
    if (!FlushViewOfFile(data, bytes_to_flush)) return false;
}

bool File_Mapping::finish_writing(i64 final_size) {
    if (!flush(final_size)) return false;

    UnmapViewOfFile(data); data = NULL;
    CloseHandle(mapping); mapping = NULL;

    LARGE_INTEGER li;
    li.QuadPart = final_size;

    PLONG hi = li.HighPart == 0 ? NULL : &li.HighPart;

    if (SetFilePointer(file, li.LowPart, hi, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        return false;

    if (!SetEndOfFile(file)) {
        error("SetEndOfFile: %s", get_last_error());
        return false;
    }

    return true;
}

void File_Mapping::cleanup() {
    if (data != NULL)
        UnmapViewOfFile(data);
    if (mapping != NULL)
        CloseHandle(mapping);
    if (file != NULL)
        CloseHandle(file);
}

ccstr get_path_relative_to(ccstr full, ccstr base) {
    Frame frame;

    auto full2 = to_wide(normalize_path_sep(full));
    auto base2 = to_wide(normalize_path_sep(base));
    auto buf = alloc_array(wchar_t, MAX_PATH+1);

    if (!PathRelativePathToW(buf, base2, FILE_ATTRIBUTE_DIRECTORY, full2, 0)) {
        frame.restore();
        return NULL;
    }

    auto ret = to_utf8(buf);
    if (ret[0] == '.' && is_sep(ret[1])) ret += 2; // remove "./" or ".\\" at beginning
    return ret;
}

ccstr get_canon_path(ccstr path) {
    auto len = strlen(path);

    Frame frame;
    auto ret = alloc_array(wchar_t, len+1);
    if (FAILED(PathCchCanonicalizeEx(ret, len+1, to_wide(path), PATHCCH_ALLOW_LONG_PATHS))) {
        frame.restore();
        return NULL;
    }

    return to_utf8(ret);
}

bool move_file_atomically(ccstr src, ccstr dest) {
    SCOPED_FRAME();

    auto wsrc = to_wide(src);
    auto wdest = to_wide(dest);

    // the file has to exist
    auto h = CreateFileW(wdest, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);

    return ReplaceFileW(wdest, wsrc, NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL);
}

int charset_to_win32_enum(Charset_Type t) {
    switch (t) {
    case CS_ENGLISH: return ANSI_CHARSET;
    case CS_CHINESE_SIM: return GB2312_CHARSET;
    case CS_CHINESE_TRAD: return CHINESEBIG5_CHARSET;
    case CS_KOREAN: return HANGUL_CHARSET;
    case CS_JAPANESE: return SHIFTJIS_CHARSET;
    case CS_CYRILLIC: return RUSSIAN_CHARSET;
    }
    return DEFAULT_CHARSET;
}

void *read_font_data_from_name(s32 *len, ccstr name, Charset_Type charset) {
    auto hfont = CreateFontW(
        0, 0, 0, 0, 0, 0, 0, 0,
        charset_to_win32_enum(charset), OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
        to_wide(name)
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

void *read_font_data_from_first_found(s32 *plen, Charset_Type charset, ...) {
    va_list vl;
    va_start(vl, plen);

    void *font_data = NULL;
    s32 len = 0;

    ccstr curr = NULL;
    while ((curr = va_arg(vl, ccstr)) != NULL) {
        Frame frame;
        font_data = read_font_data_from_name(&len, curr, charset);
        if (font_data != NULL) break;
        frame.restore();
    }

    va_end(vl);

    if (font_data != NULL)
        *plen = len;
    return font_data;
}

Ask_User_Result message_box(ccstr text, ccstr title, int flags) {
    SCOPED_FRAME();

    auto wnd = (HWND)get_native_window_handle();
    auto ret = MessageBoxW(wnd, to_wide(text), to_wide(title), flags);

    switch (ret) {
    case IDYES: return ASKUSER_YES;
    case IDNO: return ASKUSER_NO;
    case IDCANCEL: return ASKUSER_CANCEL;
    }
    return ASKUSER_ERROR;
}

Ask_User_Result ask_user_yes_no_cancel(ccstr text, ccstr title) {
    return message_box(text, title, MB_YESNOCANCEL | MB_ICONEXCLAMATION | MB_TOPMOST);
}

Ask_User_Result ask_user_yes_no(ccstr text, ccstr title) {
    return message_box(text, title, MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
}

void tell_user(ccstr text, ccstr title) {
    message_box(text, title, MB_OK | MB_ICONWARNING | MB_TOPMOST);
}

ccstr get_executable_path() {
    for (int len = MAX_PATH;; len *= 1.5) {
        Frame frame;

        auto ret = alloc_array(wchar_t, len+1);
        auto result = GetModuleFileNameW(NULL, ret, len+1);
        if (result == 0) {
            frame.restore();
            return NULL;
        }

        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            frame.restore();
            continue;
        }

        return to_utf8(ret);
    }
}

bool set_run_on_computer_startup(ccstr key, ccstr exepath) {
    HKEY hkey = NULL;
    auto subkey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";

    auto res = RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hkey, NULL);
    if (res != ERROR_SUCCESS) {
        error("RegCreateKeyEx: %s", get_specific_error(res));
        return false;
    }
    defer { RegCloseKey(hkey); };

    auto wpath = to_wide(exepath);
    return RegSetValueExW(hkey, to_wide(key), 0, REG_SZ, (BYTE*)wpath, wcslen(wpath)+1) == ERROR_SUCCESS;
}

bool xplat_chdir(ccstr dir) {
    return SetCurrentDirectory(dir);
}

#endif
