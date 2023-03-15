// Tells rest of application how to copy certain types.

#include "copy.hpp"
#include "settings.hpp"
#include "go.hpp"
#include "dbg.hpp"
#include "editor.hpp"

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
List<ccstr> *copy_string_list(List<ccstr> *arr) {
    if (!arr) return NULL;

    auto new_arr = new_object(List<ccstr>);
    new_arr->init(LIST_POOL, max(arr->len, 1));
    For (arr) new_arr->append(cp_strdup(it));
    return new_arr;
}

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
    ret->filepath = cp_strdup(filepath);
    ret->decl = !decl ? NULL : decl->copy_decl();
    ret->package_name = cp_strdup(package_name);
    return ret;
}

Go_Symbol* Go_Symbol::copy() {
    auto ret = clone(this);
    ret->pkgname = cp_strdup(pkgname);
    ret->filepath = cp_strdup(filepath);
    ret->name = cp_strdup(name);
    ret->decl = !decl ? NULL : decl->copy_decl();
    return ret;
}

Find_References_File* Find_References_File::copy() {
    auto ret = clone(this);
    ret->filepath = cp_strdup(filepath);
    ret->results = copy_list(results);
    return ret;
}

Goresult *Goresult::copy_decl() {
    auto ret = clone(this);

    auto ctx = clone(ret->ctx);
    ctx->import_path = cp_strdup(ret->ctx->import_path);
    ctx->filename = cp_strdup(ret->ctx->filename);
    ret->ctx = ctx;

    ret->decl = copy_object(decl);
    return ret;
}

Goresult *Goresult::copy_gotype() {
    auto ret = clone(this);

    auto newctx = clone(ctx);
    newctx->import_path = cp_strdup(ctx->import_path);
    newctx->filename = cp_strdup(ctx->filename);
    ret->ctx = newctx;

    ret->gotype = copy_object(gotype);
    return ret;
}

AC_Result *AC_Result::copy() {
    auto ret = clone(this);

    ret->name = cp_strdup(name);
    switch (type) {
    case ACR_DECLARATION:
        ret->declaration_godecl = copy_object(declaration_godecl);
        ret->declaration_evaluated_gotype = copy_object(declaration_evaluated_gotype);
        ret->declaration_import_path = cp_strdup(declaration_import_path);
        ret->declaration_filename = cp_strdup(declaration_filename);
        ret->declaration_package = cp_strdup(declaration_package);
        break;
    case ACR_IMPORT:
        ret->import_path = cp_strdup(import_path);
        break;
    }

    return ret;
}

Godecl *Godecl::copy() {
    auto ret = clone(this);

    ret->name = cp_strdup(name);
    if (type == GODECL_IMPORT) {
        ret->import_path = cp_strdup(import_path);
    } else {
        ret->gotype = copy_object(gotype);
        switch (type) {
        case GODECL_FUNC:
        case GODECL_TYPE:
            ret->type_params = copy_list(type_params);
            break;
        case GODECL_METHOD_RECEIVER_TYPE_PARAM:
            ret->base = copy_object(base);
            break;
        }
    }

    return ret;
}

Go_Struct_Spec *Go_Struct_Spec::copy() {
    auto ret = clone(this);
    ret->tag = cp_strdup(tag);
    ret->field = copy_object(field);
    return ret;
}

Go_Interface_Spec *Go_Interface_Spec::copy() {
    auto ret = clone(this);
    ret->field = copy_object(field);
    return ret;
}

Go_Reference *Go_Reference::copy() {
    auto ret = clone(this);
    if (is_sel) {
        ret->x = copy_object(x);
        ret->sel = cp_strdup(sel);
    } else {
        ret->name = cp_strdup(name);
    }
    return ret;
}

Go_Ctx *Go_Ctx::copy() {
    auto ret = clone(this);

    ret->import_path = cp_strdup(import_path);
    ret->filename = cp_strdup(filename);
    return ret;
}

Gotype *Gotype::copy() {
    auto ret = clone(this);
    switch (type) {
    case GOTYPE_OVERRIDE_CTX:
        ret->override_ctx_base = copy_object(override_ctx_base);
        ret->override_ctx_ctx = copy_object(override_ctx_ctx);
        break;
    case GOTYPE_GENERIC:
        ret->generic_base = copy_object(generic_base);
        ret->generic_args = copy_listp(generic_args);
        break;
    case GOTYPE_LAZY_INSTANCE:
        ret->lazy_instance_base = copy_object(lazy_instance_base);
        ret->lazy_instance_args = copy_listp(lazy_instance_args);
        break;
    case GOTYPE_CONSTRAINT:
        ret->constraint_terms = copy_listp(constraint_terms);
        break;
    case GOTYPE_CONSTRAINT_UNDERLYING:
        ret->constraint_underlying_base = copy_object(constraint_underlying_base);
        break;
    case GOTYPE_ID:
        ret->id_name = cp_strdup(id_name);
        break;
    case GOTYPE_SEL:
        ret->sel_name = cp_strdup(sel_name);
        ret->sel_sel = cp_strdup(sel_sel);
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
        ret->multi_types = copy_listp(multi_types);
        break;
    case GOTYPE_ASSERTION:
        ret->assertion_base = copy_object(assertion_base);
        break;
    case GOTYPE_RECEIVE:
        ret->receive_base = copy_object(receive_base);
        break;
    case GOTYPE_RANGE:
        ret->range_base = copy_object(range_base);
        break;
    case GOTYPE_BUILTIN:
        ret->builtin_underlying_base = copy_object(builtin_underlying_base);
        break;
    case GOTYPE_LAZY_INDEX:
        ret->lazy_index_base = copy_object(lazy_index_base);
        ret->lazy_index_key = copy_object(lazy_index_key);
        break;
    case GOTYPE_LAZY_CALL:
        ret->lazy_call_base = copy_object(lazy_call_base);
        ret->lazy_call_args = copy_listp(lazy_call_args);
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
        ret->lazy_id_name = cp_strdup(lazy_id_name);
        break;
    case GOTYPE_LAZY_SEL:
        ret->lazy_sel_base = copy_object(lazy_sel_base);
        ret->lazy_sel_sel = cp_strdup(lazy_sel_sel);
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
    ret->import_path = cp_strdup(import_path);
    ret->package_name = cp_strdup(package_name);

    auto new_files = new_object(List<Go_File>);
    new_files->init(LIST_POOL, max(ret->files->len, 1));
    For (ret->files) {
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
    ret->package_name = cp_strdup(package_name);
    ret->import_path = cp_strdup(import_path);
    ret->decl = copy_object(decl);
    return ret;
}

Go_Scope_Op *Go_Scope_Op::copy() {
    auto ret = clone(this);
    if (type == GSOP_DECL)
        ret->decl = copy_object(decl);
    return ret;
}

Go_File *Go_File::copy() {
    auto ret = clone(this);

    ptr0(&ret->pool);
    ret->pool.init();

    SCOPED_MEM(&ret->pool);

    ret->filename = cp_strdup(filename);
    ret->scope_ops = copy_list(scope_ops);
    ret->decls = copy_list(decls);
    ret->imports = copy_list(imports);
    ret->references = copy_list(references);
    return ret;
}

Go_Index *Go_Index::copy() {
    auto ret = clone(this);
    ret->workspace = copy_object(workspace);
    ret->packages = copy_list(packages);
    return ret;
}

Jump_To_Definition_Result* Jump_To_Definition_Result::copy() {
    auto ret = clone(this);
    ret->file = cp_strdup(file);
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

Find_References_Result *Find_References_Result::copy() {
    auto ret = clone(this);
    ret->reference = copy_object(reference);
    ret->toplevel_name = cp_strdup(toplevel_name);
    return ret;
}

Dlv_Var *Dlv_Var::copy() {
    auto ret = clone(this);
    ret->name = cp_strdup(name);
    ret->type = cp_strdup(type);
    ret->real_type = cp_strdup(real_type);
    ret->value = cp_strdup(value);
    ret->unreadable_description = cp_strdup(unreadable_description);
    ret->children = copy_listp(children);
    return ret;
}

Dlv_Frame *Dlv_Frame::copy() {
    auto ret = clone(this);
    ret->locals = copy_listp(locals);
    ret->args = copy_listp(args);
    ret->filepath = cp_strdup(filepath);
    ret->func_name = cp_strdup(func_name);
    return ret;
}

Dlv_Goroutine *Dlv_Goroutine::copy() {
    auto ret = clone(this);
    ret->frames = copy_list(frames);
    ret->curr_file = cp_strdup(curr_file);
    ret->curr_func_name = cp_strdup(curr_func_name);
    return ret;
}

Debugger_State *Debugger_State::copy() {
    auto ret = clone(this);
    ret->goroutines = copy_list(goroutines);
    return ret;
}

Go_Work_Module *Go_Work_Module::copy() {
    auto ret = clone(this);
    ret->import_path = cp_strdup(import_path);
    ret->resolved_path = cp_strdup(resolved_path);
    return ret;
}

Go_Workspace *Go_Workspace::copy() {
    auto ret = clone(this);
    ret->modules = copy_list(modules);
    return ret;
}

/*
Work_Trie_Node *Work_Trie_Node::copy() {
    auto ret = clone(this);
    ret->name = cp_strdup(name);
    ret->value = cp_strdup(value);

    // i think this just recursively works
    ret->children = copy_object(children);
    ret->next = copy_object(next);

    return ret;
}
*/

Vim_Dotrepeat_Input *Vim_Dotrepeat_Input::copy() {
    auto ret = clone(this);
    ret->commands = copy_list(commands);
    ret->input_chars = copy_raw_list(input_chars);
    return ret;
}

Vim_Command *Vim_Command::copy() {
    auto ret = clone(this);
    ret->op = copy_raw_list(op);
    ret->motion = copy_raw_list(motion);
    return ret;
}
