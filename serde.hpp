#pragma once

#if (__cplusplus >= 201100) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201100)
#   define OFFSETOF(_TYPE,_MEMBER)  offsetof(_TYPE, _MEMBER)
#else
#   define OFFSETOF(_TYPE,_MEMBER)  ((size_t)&(((_TYPE*)0)->_MEMBER))
#endif

// meant to be used inside sdfields
#define SERDE_OFFSET(x) OFFSETOF(decltype(this), x)

enum Serde_Type {
    SERDE_INT,
    SERDE_DOUBLE,
    SERDE_BOOL,
    SERDE_STRING,
    SERDE_ARRAY,
    _SERDE_BUILTINS_COUNT,

    _SERDE_CUSTOM_START = 1024,
    SERDE_SETTINGS,
    SERDE_OPTIONS,
    SERDE_BUILD_PROFILE,
    SERDE_DEBUG_PROFILE,
    SERDE_PROJECT_SETTINGS,
};

typedef i32    serde_int;
typedef bool   serde_bool;
typedef double serde_double;
typedef ccstr  serde_string;

struct Serde_Field {
    int id;
    int struct_offset;
    Serde_Type type;
};

struct Serde_Type_Info {
    Serde_Type type;
    List<Serde_Field> *fields;

    add_field(int id, Serde_Type 
};

extern List<Serde_Type_Info> *serde_types;

void init_serde();
Serde_Type_Info* get_type_info();

enum Serde_Read_Type {
    SRT_FILE_MAPPING,
    SRT_FILE,
};

struct Serde {
    Serde_Read_Type read_type;
    bool ok;

    union {
        File *file;
        struct {
            File_Mapping *file_mapping;
            int offset;
        };
    };

    void init() {
        ptr0(this);
        // TODO
    }

    /*
    def read_type():
        serde_type = read4()
        if serde_type == SERDE_ARRAY:
            len = read4()
            return [read() for _ in range(len)]
        elif serde_type in [SERDE_INT, SERDE_DOUBLE, SERDE_BOOL]:
            return readn()

    4 bytes: serde_type
    if it's an array
        4 bytes: array length
    n bytes: value
    */

    void readn(char *buf, int n) {
        switch (read_type) {
        case SRT_FILE:
            // TODO
            break;
        case SRT_FILE_MAPPING:
            // TODO
            break;
        }
    }

    template<typename T>
    T read_primitive() {
        T ret;
        readn((char*)(&ret), sizeof(T));
        return ret;
    }

    serde_int read_int() { return read_primitive<serde_int>(); }
    serde_bool read_bool() { return read_primitive<serde_bool>(); }
    serde_double read_double() { return read_primitive<serde_double>(); }

    serde_string read_string() {
        auto len = read_int();
        auto ret = alloc_array(char, len+1);
        readn(ret, len);
        ret[len] = '\0';
        return ret;
    }

    template<typename T>
    List<T> *read_array(int type) {
        auto len = read4();
        auto ret = alloc_list<T>();

        for (u32 i = 0; i < len; i++) {
            T obj;
            read_type(&obj, type);
            ret->append(&obj);
        }
        return ret;
    }

    template<typename T>
    void read_type(T *out, int type) {
        auto stored_type = read_int();
        if (!ok) return;

        if (stored_type != type) {
            ok = false;
            return;
        }

        auto type_info = get_type_info(type);

        while (true) {
            auto field_id = read_int();
            if (field_id == 0) break;;

            auto find_field = [&]() -> Serde_Field* {
                For (*type_info->fields)
                    if (it.id == field_id) 
                        return &it;
                return NULL;
            };

            auto field = find_field();
            if (field == NULL) {
                out->sdread(this, field_id);
            } else {
                switch (it.type) {
                case SERDE_INT:
                    *(serde_int*)((char*)out + it.struct_offset) = read_int();
                    break;
                case SERDE_DOUBLE:
                    *(serde_double*)((char*)out + it.struct_offset) = read_double();
                    break;
                case SERDE_BOOL:
                    *(serde_bool*)((char*)out + it.struct_offset) = read_bool();
                    break;
                case SERDE_STRING:
                    *(serde_string*)((char*)out + it.struct_offset) = read_string();
                    break;
                default:
                    our_panic("fields returned by sdfields must be primitives");
                }
            }
        }
    }
};
