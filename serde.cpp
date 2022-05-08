#include "serde.hpp"
#include "settings.hpp"

void Serde::init(File *_file) {
    ptr0(this);
    iotype = SERDE_IO_FILE;
    file = _file;
}

void Serde::init(File_Mapping *fm, int starting_offset) {
    ptr0(this);
    iotype = SERDE_IO_FILEMAPPING;
    offset = starting_offset;
    file_mapping = fm;
}

// TODO: refactor
void Serde::readn(char *buf, int n) {
    switch (iotype) {
    case SERDE_IO_FILE:
        ok = file->read(buf, n);
        break;
    case SERDE_IO_FILEMAPPING:
        if (offset + n > file_mapping->len) {
            ok = false;
            break;
        }
        memcpy(buf, file_mapping->data, n);
        offset += n;
        ok = true;
        break;
    default:
        ok = false;
        break;
    }
}

serde_int Serde::read_int() { return read_primitive<serde_int>(); }
serde_bool Serde::read_bool() { return read_primitive<serde_bool>(); }
serde_float Serde::read_float() { return read_primitive<serde_float>(); }

serde_string Serde::read_string() {
    auto len = read_int();
    if (!ok) return NULL;

    auto ret = alloc_array(char, len+1);
    readn(ret, len);
    if (!ok) return NULL;

    ret[len] = '\0';
    return ret;
}

void Serde::read_fixstr(char *out, int maxlen) {
    auto t = read_int();
    if (!ok) return;

    if (t != SERDE_FIXSTR) {
        ok = false;
        return;
    }

    auto len = read_int();
    if (!ok) return;

    if (maxlen < len) {
        readn(out, maxlen);
        if (!ok) return;

        // read remaining characters
        for (int i = 0; i < len - maxlen; i++) {
            char junk;
            readn(&junk, 1);
            if (!ok) return;
        }
    } else {
        readn(out, len);
        if (!ok) return;
    }
}

// TODO: error handling?
void Serde::read_type(void* out, int type) {
    int t = read_int();
    if (!ok) return;

    if (t != type) {
        ok = false;
        return;
    }

    switch (type) {
    case SERDE_INT: *(serde_int*)out = read_int(); return;
    case SERDE_BOOL: *(serde_bool*)out = read_bool(); return;
    case SERDE_FLOAT: *(serde_float*)out = read_float(); return;
    case SERDE_STRING: *(serde_string*)out = read_string(); return;
    }

    while (true) {
        auto field_id = read_int();
        if (!ok) return;
        if (!field_id) break;

        read_type_field(out, type, field_id);
        if (!ok) break;
    }
}

void Serde::write_fixstr_field(int field_id, serde_char *s, int len) {
    write_int(field_id);
    if (!ok) return;

    write_int(SERDE_FIXSTR);
    if (!ok) return;

    write_int(len);
    if (!ok) return;

    writen(s, len);
    if (!ok) return;
}

void Serde::write_field(int field_id, void* val, int type) {
    write_int(field_id);
    if (!ok) return;

    write_type(val, type);
}

void Serde::writen(char* buf, int n) {
    switch (iotype) {
    case SERDE_IO_FILE:
        ok = file->write(buf, n);
        break;
    case SERDE_IO_FILEMAPPING:
        ok = false; // not implemented
        break;
    }
}

void Serde::write_int(serde_int val) { write_primitive<serde_int>(val); }
void Serde::write_bool(serde_bool val) { write_primitive<serde_bool>(val); }
void Serde::write_float(serde_float val) { write_primitive<serde_float>(val); }

void Serde::write_string(serde_string val) {
    auto len = strlen(val);
    write_int(len);
    if (!ok) return;

    writen((char*)val, len);
}

// ====

void Serde::write_type(void* val, int type) {
    write_int(type);
    if (!ok) return;

    switch (type) {
    case SERDE_INT: write_int(*(serde_int*)val); return;
    case SERDE_BOOL: write_bool(*(serde_bool*)val); return;
    case SERDE_FLOAT: write_float(*(serde_float*)val); return;
    case SERDE_STRING: write_string(*(serde_string*)val); return;

#define WRITE(x, y, z) write_field(x, &o->y, z)
#define WRITE_FIXSTR(x, y) write_fixstr_field(x, o->y, _countof(o->y))

    case SERDE_OPTIONS: {
        auto o = (Options*)val;
        WRITE(1, scrolloff, SERDE_INT);
        WRITE(2, tabsize, SERDE_INT);
        WRITE(3, enable_vim_mode, SERDE_BOOL);
        write_int(0);
        break;
    }

    case SERDE_BUILD_PROFILE: {
        auto o = (Build_Profile*)val;
        WRITE_FIXSTR(1, label);
        WRITE_FIXSTR(2, cmd);
        write_int(0);
        break;
    }

    case SERDE_DEBUG_PROFILE: {
        auto o = (Debug_Profile*)val;
        WRITE(1, type, SERDE_INT);
        WRITE(2, is_builtin, SERDE_BOOL);
        WRITE_FIXSTR(3, label);

        switch (o->type) {
        case DEBUG_RUN_BINARY:
            WRITE_FIXSTR(4, run_binary.binary_path);
            break;
        case DEBUG_TEST_PACKAGE:
            WRITE(5, test_package.use_current_package, SERDE_BOOL);
            WRITE_FIXSTR(6, test_package.package_path);
            break;
        case DEBUG_RUN_PACKAGE:
            WRITE(7, run_package.use_current_package, SERDE_BOOL);
            WRITE_FIXSTR(8, run_package.package_path);
            break;
        }

        WRITE_FIXSTR(9, args);
        write_int(0);
        break;
    }

    case SERDE_PROJECT_SETTINGS: {
        auto o = (Project_Settings*)val;
        write_array_field(1, o->build_profiles, SERDE_BUILD_PROFILE);
        write_array_field(2, o->debug_profiles, SERDE_DEBUG_PROFILE);
        WRITE(3, active_build_profile, SERDE_INT);
        WRITE(4, active_debug_profile, SERDE_INT);
        write_int(0);
        break;
    }

    }
}

#define FIELD(id, field, type) \
    case id: \
        read_type(&o->field, type); \
        break

#define FIELD_ARR(id, field, ctype, type) \
    case id: \
        o->field = read_array<ctype>(type); \
        break

#define FIELD_FIXSTR(id, field) \
    case id: \
        read_fixstr(o->field, _countof(o->field)); \
        break

void Serde::read_type_field(void* out, int type, int field_id) {
    switch (type) {
    case SERDE_OPTIONS: {
        auto o = (Options*)out;
        switch (field_id) {
        FIELD(1, scrolloff, SERDE_INT);
        FIELD(2, tabsize, SERDE_INT);
        FIELD(3, enable_vim_mode, SERDE_BOOL);
        }
        break;
    }

    case SERDE_BUILD_PROFILE: {
        auto o = (Build_Profile*)out;
        switch (field_id) {
        FIELD_FIXSTR(1, label);
        FIELD_FIXSTR(2, cmd);
        }
        break;
    }

    case SERDE_DEBUG_PROFILE: {
        auto o = (Debug_Profile*)out;
        switch (field_id) {
        FIELD(1, type, SERDE_INT);
        FIELD(2, is_builtin, SERDE_BOOL);
        FIELD_FIXSTR(3, label);
        FIELD_FIXSTR(4, run_binary.binary_path);
        FIELD(5, test_package.use_current_package, SERDE_BOOL);
        FIELD_FIXSTR(6, test_package.package_path);
        FIELD(7, run_package.use_current_package, SERDE_BOOL);
        FIELD_FIXSTR(8, run_package.package_path);
        FIELD_FIXSTR(9, args);
        }
        break;
    }

    case SERDE_PROJECT_SETTINGS: {
        auto o = (Project_Settings*)out;
        switch (field_id) {
        FIELD_ARR(1, build_profiles, Build_Profile, SERDE_BUILD_PROFILE);
        FIELD_ARR(2, debug_profiles, Debug_Profile, SERDE_DEBUG_PROFILE);
        FIELD(3, active_build_profile, SERDE_INT);
        FIELD(4, active_debug_profile, SERDE_INT);
        }
        break;
    }

    }
}

#undef FIELD
#undef FIELD_ARR
#undef FIELD_FIXSTR
