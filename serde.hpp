#pragma once

#include "common.hpp"
#include "os.hpp"

enum Serde_Type {
    SERDE_INT,
    SERDE_FLOAT,
    SERDE_BOOL,
    SERDE_STRING,
    SERDE_ARRAY,
    SERDE_FIXSTR,
    _SERDE_BUILTINS_COUNT,
    _SERDE_CUSTOM_START = 1024,
    SERDE_SETTINGS, // deprecated
    SERDE_OPTIONS,
    SERDE_BUILD_PROFILE,
    SERDE_DEBUG_PROFILE,
    SERDE_PROJECT_SETTINGS,
};

typedef i32    serde_int;
typedef bool   serde_bool;
typedef float  serde_float;
typedef ccstr  serde_string;
typedef char   serde_char;

enum Serde_Io_Type {
    SERDE_IO_FILEMAPPING,
    SERDE_IO_FILE,
};

struct Serde {
    Serde_Io_Type iotype;
    bool ok;

    union {
        File *file;
        struct {
            File_Mapping *file_mapping;
            int offset;
        };
    };

    bool init(ccstr filename, bool write);

    void init(File *_file);
    void init(File_Mapping *fm, int starting_offset = 0);

    void readn(char *buf, int n);
    serde_int read_int();
    serde_bool read_bool();
    serde_float read_float();
    serde_string read_string();
    bool read_type_field(void* out, int type, int field_id);
    void read_type(void* out, int type);
    void read_fixstr(char *out, int maxlen);

    void writen(char* buf, int n);
    void write_int(serde_int val);
    void write_bool(serde_bool val);
    void write_float(serde_float val);
    void write_string(serde_string val);
    void write_field(int field_id, void* val, int type);
    void write_type(void* val, int type);
    void write_fixstr_field(int field_id, serde_char *s, int len);

    // =======
    // Templates. I think I read somewhere that templates have to be in the
    // header? I don't know, C++ is dumb.
    // =======

    template<typename T>
    T read_primitive() {
        T ret; ptr0(&ret);
        readn((char*)(&ret), sizeof(T));
        return ret;
    }

    template<typename T>
    void read_out(T *out) {
        auto val = read_primitive<T>();
        if (out) *out = val;
    }

    template<typename T>
    void write_primitive(T val) {
        writen((char*)(&val), sizeof(T));
    }

    template<typename T>
    void write_array_field(int field_id, List<T> *val, int type) {
        write_int(field_id);
        if (!ok) return;

        write_int(SERDE_ARRAY);
        if (!ok) return;

        write_int(val->len);
        if (!ok) return;

        For (*val) {
            write_type(&it, type);
            if (!ok) break;
        }
    }

    template<typename T>
    List<T> *read_array(int type) {
        auto t = read_int();
        if (t != SERDE_ARRAY) {
            ok = false;
            return NULL;
        }

        auto len = read_int();
        if (!ok) return NULL;

        auto ret = alloc_list<T>();
        for (u32 i = 0; i < len; i++) {
            T obj;
            read_type(&obj, type);
            if (!ok) return NULL;

            ret->append(&obj);
        }
        return ret;
    }
};
