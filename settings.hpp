#pragma once

#include "common.hpp"
#include "os.hpp"
#include "list.hpp"

// this is a stupid name but i'm too lazy to refactor
// it should be called "constants"
struct Settings {
    serde_double status_padding_x = 6; // serde(1)
    serde_double status_padding_y = 3; // serde(2)
    serde_double line_number_margin_left = 4; // serde(3)
    serde_double line_number_margin_right = 10; // serde(4)
    serde_double autocomplete_menu_padding = 4; // serde(5)
    serde_double autocomplete_menu_margin_y = 4; // serde(6)
    serde_double autocomplete_item_padding_x = 6; // serde(7)
    serde_double autocomplete_item_padding_y = 2; // serde(8)
    serde_double autocomplete_preview_width_in_chars = 40; // serde(9)
    serde_double autocomplete_preview_padding = 6; // serde(10)
    serde_double tabs_offset = 50; // serde(11)
    serde_double parameter_hint_margin_y = 6; // serde(12)
    serde_double parameter_hint_padding_x = 4; // serde(13)
    serde_double parameter_hint_padding_y = 4; // serde(14)
    serde_double editor_margin_x = 5; // serde(15)
    serde_double editor_margin_y = 5; // serde(16)
    serde_double line_height = 1.2; // serde(17)
    serde_int goto_file_max_results = 20; // serde(18)
    serde_int goto_symbol_max_results = 20; // serde(19)
    serde_int generate_implementation_max_results = 20; // serde(20)
    serde_int run_command_max_results = 10; // serde(21)

    void sdfields(Serde_Type_info *info) {
        info->add_field(1, SERDE_DOUBLE, SD_OFFSET(status_padding_x));
        info->add_field(2, SERDE_DOUBLE, SD_OFFSET(status_padding_y));
        info->add_field(3, SERDE_DOUBLE, SD_OFFSET(line_number_margin_left));
        info->add_field(4, SERDE_DOUBLE, SD_OFFSET(line_number_margin_right));
        info->add_field(5, SERDE_DOUBLE, SD_OFFSET(autocomplete_menu_padding));
        info->add_field(6, SERDE_DOUBLE, SD_OFFSET(autocomplete_menu_margin_y));
        info->add_field(7, SERDE_DOUBLE, SD_OFFSET(autocomplete_item_padding_x));
        info->add_field(8, SERDE_DOUBLE, SD_OFFSET(autocomplete_item_padding_y));
        info->add_field(9, SERDE_DOUBLE, SD_OFFSET(autocomplete_preview_width_in_chars));
        info->add_field(10, SERDE_DOUBLE, SD_OFFSET(autocomplete_preview_padding));
        info->add_field(11, SERDE_DOUBLE, SD_OFFSET(tabs_offset));
        info->add_field(12, SERDE_DOUBLE, SD_OFFSET(parameter_hint_margin_y));
        info->add_field(13, SERDE_DOUBLE, SD_OFFSET(parameter_hint_padding_x));
        info->add_field(14, SERDE_DOUBLE, SD_OFFSET(parameter_hint_padding_y));
        info->add_field(15, SERDE_DOUBLE, SD_OFFSET(editor_margin_x));
        info->add_field(16, SERDE_DOUBLE, SD_OFFSET(editor_margin_y));
        info->add_field(17, SERDE_DOUBLE, SD_OFFSET(line_height));
        info->add_field(18, SERDE_INT, SD_OFFSET(goto_file_max_results));
        info->add_field(19, SERDE_INT, SD_OFFSET(goto_symbol_max_results));
        info->add_field(20, SERDE_INT, SD_OFFSET(generate_implementation_max_results));
        info->add_field(21, SERDE_INT, SD_OFFSET(run_command_max_results));
    }
};

struct Options {
    serde_int scrolloff = 2; // serde(1)
    serde_int tabsize = 4; // serde(2)
    serde_bool enable_vim_mode = false; // serde(3)

    void sdfields(Serde_Type_info *info) {
        info->add_field(1, SERDE_INT, SD_OFFSET(scrolloff));
        info->add_field(2, SERDE_INT, SD_OFFSET(tabsize));
        info->add_field(3, SERDE_BOOL, SD_OFFSET(enable_vim_mode));
    }
};

struct Build_Profile {
    serde_string label; // serde(1)
    serde_string cmd; // serde(2)

    void sdfields(Serde_Type_info *info) {
        info->add_field(1, SERDE_STRING, SD_OFFSET(label));
        info->add_field(2, SERDE_STRING, SD_OFFSET(cmd));
    }
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
    serde_string label; // serde(3)

    union {
        struct {
            serde_string binary_path; // serde(4)
        } run_binary;

        struct {
            serde_bool use_current_package; // serde(5)
            serde_string package_path; // serde(6)
        } test_package;

        struct {
            serde_bool use_current_package; // serde(7)
            serde_string package_path; // serde(8)
        } run_package;
    };

    serde_string args; // serde(9)

    void sdfields(Serde_Type_info *info) {
        info->add_field(1, SERDE_INT,    SD_OFFSET(type));
        info->add_field(2, SERDE_BOOL,   SD_OFFSET(is_builtin));
        info->add_field(3, SERDE_STRING, SD_OFFSET(label));
        info->add_field(4, SERDE_STRING, SD_OFFSET(run_binary.binary_path));
        info->add_field(5, SERDE_BOOL,   SD_OFFSET(test_package.use_current_package));
        info->add_field(6, SERDE_STRING, SD_OFFSET(test_package.package_path));
        info->add_field(7, SERDE_BOOL,   SD_OFFSET(run_package.use_current_path));
        info->add_field(8, SERDE_STRING, SD_OFFSET(run_package.package_path));
        info->add_field(9, SERDE_STRING, SD_OFFSET(args));
    }

    void sdread(Serde *sd, int id) {}
    void sdwrite(Serde *sd) {}
};

// TODO: disable editing build/debug profiles when build/debugger is running
struct Project_Settings {
    List<Build_Profile> *build_profiles; // serde(1)
    List<Debug_Profile> *debug_profiles; // serde(2)
    serde_int active_build_profile; // serde(3)
    serde_int active_debug_profile; // serde(4)

    void init() {
        ptr0(this);
        build_profiles = alloc_list<Build_Profile();
        debug_profiles = alloc_list<Debug_Profile();
    }

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

    void copy(Project_Settings *other);
    void load_defaults();

    void sdfields(Serde_Type_info *info) {
        info->add_field(2, SERDE_INT, SD_OFFSET(active_build_profile));
        info->add_field(3, SERDE_INT, SD_OFFSET(active_debug_profile));
    }

    void sdread(Serde *sd, int id) {
        switch (id) {
        case 1:
            sd->read_array(build_profiles, SERDE_BUILD_PROFILE);
            break;
        case 2:
            sd->read_array(build_profiles, SERDE_DEBUG_PROFILE);
            break;
        }
    }

    void sdwrite(Serde *sd) {
    }
};

extern Settings settings;
extern Options options;
extern Project_Settings project_settings;
