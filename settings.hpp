#pragma once

#include "common.hpp"
#include "os.hpp"
#include "list.hpp"

// this is a stupid name but i'm too lazy to refactor
// it should be called "constants"
struct Settings {
    float status_padding_x = 6;
    float status_padding_y = 3;
    float line_number_margin_left = 4;
    float line_number_margin_right = 10;
    float autocomplete_menu_padding = 4;
    float autocomplete_menu_margin_y = 4;
    float autocomplete_item_padding_x = 6;
    float autocomplete_item_padding_y = 2;
    float tabs_offset = 50;
    float parameter_hint_margin_y = 6;
    float parameter_hint_padding_x = 4;
    float parameter_hint_padding_y = 4;
    float editor_margin_x = 5;
    float editor_margin_y = 5;
    float line_height = 1.2;
    int open_file_max_results = 20;
};

struct Options {
	int scrolloff = 2;
    int tabsize = 4;
};

struct Build_Profile {
    char label[256];
    char cmd[256];
};

enum Debug_Type {
    DEBUG_TEST_PACKAGE,
    DEBUG_TEST_CURRENT_FUNCTION,
    DEBUG_RUN_PACKAGE,
    DEBUG_RUN_BINARY,
    // DEBUG_ATTACH,
};

struct Debug_Profile {
    Debug_Type type;
    bool is_builtin;

    char label[256];

    union {
        struct {
            char binary_path[256];
        } run_binary;

        struct {
            bool use_current_package;
            char package_path[256];
        } test_package;

        struct {
            bool use_current_package;
            char package_path[256];
        } run_package;
    };

    char args[256];
};

// TODO: disable editing build/debug profiles when build/debugger is running
struct Project_Settings {
    Build_Profile build_profiles[16];
    Debug_Profile debug_profiles[16];
    int build_profiles_len;
    int debug_profiles_len;

    int active_build_profile;
    int active_debug_profile;

    Build_Profile *get_active_build_profile();
    Debug_Profile *get_active_debug_profile();

    void copy(Project_Settings *other);
    void load_defaults();
    void read(ccstr file);
    void write(ccstr file);
};

extern Settings settings;
extern Options options;
extern Project_Settings project_settings;
