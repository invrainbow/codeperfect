// Tells rest of application how to copy certain types.

#include "copy.hpp"
#include "settings.hpp"
#include "go.hpp"

// -----
// stupid c++ template shit

template<typename T>
T *clone(T *old) {
    auto ret = alloc_object(T);
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

    auto new_arr = alloc_object(List<T>);
    new_arr->init(LIST_POOL, max(arr->len, 1));
    For (*arr) new_arr->append(copy_func(&it));
    return new_arr;
}

template <typename T>
List<T> *copy_list(List<T> *arr) {
    auto copy_func = [&](T *it) -> T* { return copy_object(it); };
    return copy_list<T>(arr, copy_func);
}

template <typename T>
List<T> *copy_list(List<T*> *arr) {
    auto copy_func = [&](T **it) -> T* { return copy_object(*it); };
    return copy_list<T>(arr, copy_func);
}


// -----
// actual code that tells us how to copy objects

Call_Hier_Node* Call_Hier_Node::copy() {
    auto ret = clone(this);
    ret->decl = copy_object(decl);
    ret->ref = copy_object(ref);
    ret->children = copy_list(children);
    return ret;
}

Find_Decl* Find_Decl::copy() {
    auto ret = clone(this);
    ret->filepath = our_strcpy(filepath);
    ret->decl = !decl ? NULL : decl->copy_decl();
    ret->package_name = our_strcpy(package_name);
    return ret;
}

Go_Symbol* Go_Symbol::copy() {
    auto ret = clone(this);
    ret->pkgname = our_strcpy(pkgname);
    ret->filepath = our_strcpy(filepath);
    ret->name = our_strcpy(name);
    ret->decl = !decl ? NULL : decl->copy_decl();
    return ret;
}

Find_References_File* Find_References_File::copy() {
    auto ret = clone(this);
    ret->filepath = our_strcpy(filepath);
    ret->references = copy_list(references);
    return ret;
}

Goresult *Goresult::copy_decl() {
    auto ret = clone(this);

    auto ctx = clone(ret->ctx);
    ctx->import_path = our_strcpy(ret->ctx->import_path);
    ctx->filename = our_strcpy(ret->ctx->filename);
    ret->ctx = ctx;

    ret->decl = copy_object(decl);
    return ret;
}

Goresult *Goresult::copy_gotype() {
    auto ret = clone(this);

    auto newctx = clone(ctx);
    newctx->import_path = our_strcpy(ctx->import_path);
    newctx->filename = our_strcpy(ctx->filename);
    ret->ctx = newctx;

    ret->gotype = copy_object(gotype);
    return ret;
}

AC_Result *AC_Result::copy() {
    auto ret = clone(this);

    ret->name = our_strcpy(name);
    switch (type) {
    case ACR_DECLARATION:
        ret->declaration_godecl = copy_object(declaration_godecl);
        ret->declaration_evaluated_gotype = copy_object(declaration_evaluated_gotype);
        ret->declaration_import_path = our_strcpy(declaration_import_path);
        ret->declaration_filename = our_strcpy(declaration_filename);
        ret->declaration_package = our_strcpy(declaration_package);
        break;
    case ACR_IMPORT:
        ret->import_path = our_strcpy(import_path);
        break;
    }

    return ret;
}

Godecl *Godecl::copy() {
    auto ret = clone(this);

    ret->name = our_strcpy(name);
    if (type == GODECL_IMPORT)
        ret->import_path = our_strcpy(import_path);
    else
        ret->gotype = copy_object(gotype);
    return ret;
}

Go_Struct_Spec *Go_Struct_Spec::copy() {
    auto ret = clone(this);
    ret->tag = our_strcpy(tag);
    ret->field = copy_object(field);
    return ret;
}

Go_Reference *Go_Reference::copy() {
    auto ret = clone(this);
    if (is_sel) {
        ret->x = copy_object(x);
        ret->sel = our_strcpy(sel);
    } else {
        ret->name = our_strcpy(name);
    }
    return ret;
}

Gotype *Gotype::copy() {
    auto ret = clone(this);
    switch (type) {
    case GOTYPE_ID:
        ret->id_name = our_strcpy(id_name);
        break;
    case GOTYPE_SEL:
        ret->sel_name = our_strcpy(sel_name);
        ret->sel_sel = our_strcpy(sel_sel);
        break;
    case GOTYPE_MAP:
        ret->map_key = copy_object(map_key);
        ret->map_value = copy_object(map_value);
        break;
    case GOTYPE_STRUCT:
        ret->struct_specs = copy_list(struct_specs);
        break;
    case GOTYPE_INTERFACE:
        ret->interface_specs = copy_list(interface_specs);
        break;
    case GOTYPE_POINTER: ret->pointer_base = copy_object(pointer_base); break;
    case GOTYPE_SLICE: ret->slice_base = copy_object(slice_base); break;
    case GOTYPE_ARRAY: ret->array_base = copy_object(array_base); break;
    case GOTYPE_CHAN: ret->chan_base = copy_object(chan_base); break;
    case GOTYPE_FUNC:
        ret->func_sig.params = copy_list(func_sig.params);
        ret->func_sig.result = copy_list(func_sig.result);
        ret->func_recv = copy_object(ret->func_recv);
        break;
    case GOTYPE_MULTI:
        {
            Gotype *tmp = NULL;
            auto copy_func = [&](Gotype** it) {
                tmp = copy_object(*it);
                return &tmp;
            };
            ret->multi_types = copy_list<Gotype*>(multi_types, copy_func);
        }
        break;
    case GOTYPE_VARIADIC:
        ret->variadic_base = copy_object(variadic_base);
        break;
    case GOTYPE_ASSERTION:
        ret->assertion_base = copy_object(assertion_base);
        break;
    case GOTYPE_RANGE:
        ret->range_base = copy_object(range_base);
        break;
    case GOTYPE_BUILTIN:
        ret->builtin_underlying_type = copy_object(builtin_underlying_type);
        break;
    case GOTYPE_LAZY_INDEX:
        ret->lazy_index_base = copy_object(lazy_index_base);
        break;
    case GOTYPE_LAZY_CALL:
        ret->lazy_call_base = copy_object(lazy_call_base);
        break;
    case GOTYPE_LAZY_DEREFERENCE:
        ret->lazy_dereference_base = copy_object(lazy_dereference_base);
        break;
    case GOTYPE_LAZY_REFERENCE:
        ret->lazy_reference_base = copy_object(lazy_reference_base);
        break;
    case GOTYPE_LAZY_ARROW:
        ret->lazy_arrow_base = copy_object(lazy_arrow_base);
        break;
    case GOTYPE_LAZY_ID:
        ret->lazy_id_name = our_strcpy(lazy_id_name);
        break;
    case GOTYPE_LAZY_SEL:
        ret->lazy_sel_base = copy_object(lazy_sel_base);
        ret->lazy_sel_sel = our_strcpy(lazy_sel_sel);
        break;
    case GOTYPE_LAZY_ONE_OF_MULTI:
        ret->lazy_one_of_multi_base = copy_object(lazy_one_of_multi_base);
        break;
    case GOTYPE_LAZY_RANGE:
        ret->lazy_range_base = copy_object(lazy_range_base);
        break;
    }
    return ret;
}

Go_Package *Go_Package::copy() {
    auto ret = clone(this);
    ret->import_path = our_strcpy(import_path);
    ret->package_name = our_strcpy(package_name);

    auto new_files = alloc_object(List<Go_File>);
    new_files->init(LIST_POOL, max(ret->files->len, 1));
    For (*ret->files) {
        auto gofile = new_files->append();
        memcpy(gofile, &it, sizeof(Go_File));

        Pool new_pool;
        new_pool.init("file pool", 512);

        {
            SCOPED_MEM(&new_pool);
            gofile->scope_ops = copy_list(gofile->scope_ops);
            gofile->decls = copy_list(gofile->decls);
            gofile->imports = copy_list(gofile->imports);
            gofile->references = copy_list(gofile->references);
        }

        gofile->cleanup();
        gofile->pool = new_pool;
    }
    ret->files = new_files;

    return ret;
}

Go_Import *Go_Import::copy() {
    auto ret = clone(this);
    ret->package_name = our_strcpy(package_name);
    ret->import_path = our_strcpy(import_path);
    ret->decl = copy_object(decl);
    return ret;
}

Go_Scope_Op *Go_Scope_Op::copy() {
    auto ret = clone(this);
    if (type == GSOP_DECL)
        ret->decl = copy_object(decl);
    return ret;
}

/*
Go_File *Go_File::copy() {
    auto ret = clone(this);
    ret->filename = our_strcpy(filename);
    ret->scope_ops = copy_list(scope_ops);
    ret->decls = copy_list(decls);
    ret->imports = copy_list(imports);
    return ret;
}
*/

Go_Index *Go_Index::copy() {
    auto ret = clone(this);
    ret->current_path = our_strcpy(current_path);
    ret->current_import_path = our_strcpy(current_import_path);
    ret->packages = copy_list(packages);
    return ret;
}

Jump_To_Definition_Result* Jump_To_Definition_Result::copy() {
    auto ret = clone(this);
    ret->file = our_strcpy(file);
    ret->decl = !decl ? NULL : decl->copy_decl();
    return ret;
}

Debug_Profile *Debug_Profile::copy() { return clone(this); }
Build_Profile *Build_Profile::copy() { return clone(this); }

Project_Settings *Project_Settings::copy() {
    auto ret = clone(this);
    ret->build_profiles = copy_list(build_profiles);
    ret->debug_profiles = copy_list(debug_profiles);
    return ret;
}
