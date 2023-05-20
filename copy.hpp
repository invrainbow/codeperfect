#pragma once

#include "mem.hpp"

// -----
// stupid c++ template shit

template<typename T>
T *clone(T *old) {
    auto ret = new_object(T);
    memcpy(ret, old, sizeof(T));
    return ret;
}

template <typename T>
T* copy_object(T *old) {
    return !old ? NULL : old->copy();
}

template <typename T>
List<T> *copy_list(List<T> *arr, fn<T*(T* it)> copy_func) {
    if (!arr) return NULL;

    auto new_arr = new_object(List<T>);
    new_arr->init(LIST_POOL, max(arr->len, 1));
    For (arr) new_arr->append(copy_func(&it));
    return new_arr;
}

template <typename T>
List<T> *copy_listp(List<T> *arr) {
    if (!arr) return NULL;

    auto new_arr = new_object(List<T>);
    new_arr->init(LIST_POOL, max(arr->len, 1));
    For (arr) new_arr->append(copy_object(it));
    return new_arr;
}

template <typename T>
List<T> *copy_list(List<T> *arr) {
    auto copy_func = [&](T *it) -> T* { return copy_object(it); };
    return copy_list<T>(arr, copy_func);
}

// specialization of copy_list for ccstr
List<ccstr> *copy_string_list(List<ccstr> *arr);

template <typename T>
List<T> *copy_raw_list(List<T> *arr) {
    if (!arr) return NULL;

    auto new_arr = new_object(List<T>);
    new_arr->init(LIST_POOL, max(arr->len, 1));
    new_arr->concat(arr);
    return new_arr;
}

template <typename T>
List<T> *copy_list(List<T*> *arr) {
    auto copy_func = [&](T **it) -> T* { return copy_object(*it); };
    return copy_list<T>(arr, copy_func);
}
