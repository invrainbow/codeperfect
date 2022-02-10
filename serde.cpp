#include "serde.hpp"

List<Serde_Type_Info> *serde_types = NULL;

Serde_Type_Info* get_type_info() {
    For (serde_types)
        if (it.type == type)
            return &it;

    auto ret = serde_types.append();
    ret->type = type;
    ret->fields = alloc_list<Serde_Field>();
    return ret;
}

template<typename T>
void add_type_fields(Serde_Type type) {
    T t;
    t.sdfields(get_type_info(type));
}

void init_serde() {
    add_type_fields<Settings>(SERDE_SETTINGS);
    add_type_fields<Options>(SERDE_OPTIONS);
    add_type_fields<Build_Profile>(SERDE_BUILD_PROFILE);
    add_type_fields<Debug_Profile>(SERDE_DEBUG_PROFILE);
    add_type_fields<Project_Settings>(SERDE_PROJECT_SETTINGS);
}
