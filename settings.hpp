#pragma once

#include "common.hpp"
#include "os.hpp"
#include "list.hpp"
#include "serde.hpp"

// this is a stupid name but i'm too lazy to refactor
// it should be called "constants"
struct Settings {
    int status_padding_x = 6;
    int status_padding_y = 3;
    int line_number_margin_left = 4;
    int line_number_margin_right = 10;
    int autocomplete_menu_padding = 4;
    int autocomplete_menu_margin_y = 4;
    int autocomplete_item_padding_x = 6;
    int autocomplete_item_padding_y = 2;
    int autocomplete_preview_width_in_chars = 40;
    int autocomplete_preview_padding = 6;
    int tabs_offset = 50;
    int parameter_hint_margin_y = 6;
    int parameter_hint_padding_x = 4;
    int parameter_hint_padding_y = 4;
    int editor_margin_x = 5;
    int editor_margin_y = 5;
    float line_height = 1.2;
    int goto_file_max_results = 50;
    int goto_symbol_max_results = 50;
    int generate_implementation_max_results = 50;
    int run_command_max_results = 10;
};

struct Options {
    serde_int scrolloff = 0; // serde(1)
    serde_int tabsize = 4; // serde(2)
    serde_bool enable_vim_mode = false; // serde(3)
    serde_bool format_on_save = true; // serde(4)
    serde_bool organize_imports_on_save = false; // serde(5)
    serde_int struct_tag_case_style = 0; // serde(6)
    serde_bool autocomplete_func_add_paren = true; // serde(7)
    serde_bool dbg_hide_system_goroutines = true; // serde(8)
};

struct Build_Profile {
    serde_char label[256]; // serde(1)
    serde_char cmd[256]; // serde(2)

    Build_Profile *copy();
};

enum Debug_Type {
    DEBUG_TEST_PACKAGE,
    DEBUG_TEST_CURRENT_FUNCTION,
    DEBUG_RUN_PACKAGE,
    DEBUG_RUN_BINARY,
    // DEBUG_ATTACH,
};

struct Debug_Profile {
    serde_int type; // serde(1)
    serde_bool is_builtin; // serde(2)
    serde_char label[256]; // serde(3)

    union {
        struct {
            serde_char binary_path[256]; // serde(4)
        } run_binary;

        struct {
            serde_bool use_current_package; // serde(5)
            serde_char package_path[256]; // serde(6)
        } test_package;

        struct {
            serde_bool use_current_package; // serde(7)
            serde_char package_path[256]; // serde(8)
        } run_package;
    };

    serde_char args[256]; // serde(9)

    Debug_Profile *copy();
};

// TODO: disable editing build/debug profiles when build/debugger is running
struct Project_Settings {
    List<Build_Profile> *build_profiles; // serde(1)
    List<Debug_Profile> *debug_profiles; // serde(2)
    serde_int active_build_profile; // serde(3)
    serde_int active_debug_profile; // serde(4)

    Build_Profile *get_active_build_profile() {
        if (active_build_profile < build_profiles->len)
            return &build_profiles->at(active_build_profile);
        return NULL;
    }

    Debug_Profile *get_active_debug_profile() {
        if (active_debug_profile < debug_profiles->len)
            return &debug_profiles->at(active_debug_profile);
        return NULL;
    }

    Project_Settings *copy();
    void load_defaults();
};

extern Settings settings;
extern Options options;
extern Project_Settings project_settings;
