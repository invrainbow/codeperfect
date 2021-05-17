#include "settings.hpp"
#include "utils.hpp"

Settings settings;
Options options;
Project_Settings project_settings;

// --- utiltiies

char* read_line(File *f, int *len) {
    auto ret = alloc_list<char>();
    char ch;

    while (true) {
        if (!f->read(&ch, 1)) return NULL;
        if (ch == '\r') continue;
        if (ch == '\n') break;
        ret->append(ch);
    }

    *len = ret->len;
    ret->append('\0');
    return ret->items;
}

void read_settings(File *f, fn<void(ccstr key, int klen, ccstr val, int vlen)> cb) {
    while (true) {
        SCOPED_FRAME();

        int klen = 0, vlen = 0;
        auto key = read_line(f, &klen);
        auto value = read_line(f, &vlen);

        if (key == NULL || value == NULL) break;

        cb(key, klen, value, vlen);
    }
}

// destlen includes nul char, srclen doesn't
void _copy_settings_string(char *dest, int destlen, ccstr src, int srclen) {
    if (destlen >= srclen+1)
        strcpy_safe(dest, destlen, src);
}

#define copy_settings_string(x, y, z) _copy_settings_string(x, _countof(x), y, z)

void write_setting(File *f, ccstr key, ccstr value) {
    char newline = '\n';

    f->write((char*)key, strlen(key));
    f->write(&newline, 1);
    f->write((char*)value, strlen(value));
    f->write(&newline, 1);
}

// --- main code

void Project_Settings::copy(Project_Settings *other) {
    copy_settings_string(build_command, other->build_command, strlen(other->build_command));
    copy_settings_string(debug_binary_path, other->debug_binary_path, strlen(other->debug_binary_path));
}

void Project_Settings::read(ccstr file) {
    File f;
    if (f.init(file, FILE_MODE_READ, FILE_OPEN_EXISTING) != FILE_RESULT_SUCCESS)
        return;
    defer { f.cleanup(); };

    read_settings(&f, [&](ccstr key, int klen, ccstr val, int vlen) {
        if (streq(key, "build_command")) {
            copy_settings_string(build_command, val, vlen);
        } else if (streq(key, "debug_binary_path")) {
            copy_settings_string(debug_binary_path, val, vlen);
        }
    });
}

void Project_Settings::write(ccstr file) {
    File f;
    if (f.init(file, FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS)
        return;
    defer { f.cleanup(); };

    write_setting(&f, "build_command", build_command);
    write_setting(&f, "debug_binary_path", debug_binary_path);
}
