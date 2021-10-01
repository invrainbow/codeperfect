#include "settings.hpp"
#include "utils.hpp"

Settings settings;
Options options;
Project_Settings project_settings;

// --- main code

void Project_Settings::copy(Project_Settings *other) {
    memcpy(this, other, sizeof(*this));
}

void Project_Settings::load_defaults() {
    ptr0(this);

    // This is designed so that debug/build profiles is never empty, and
    // active_debug_profile/active_build_profile always points to a valid file.

    active_debug_profile = 1; // can't select "test function under cursor" as default profile

    {
        auto bp = &build_profiles[build_profiles_len++];
        strcpy_safe(bp->label, _countof(bp->label), "Build Project");
        strcpy_safe(bp->cmd, _countof(bp->cmd), "go build <write your package here>");
    }

    {
        auto dp = &debug_profiles[debug_profiles_len++];
        dp->type = DEBUG_TEST_CURRENT_FUNCTION;
        dp->is_builtin = true;
        strcpy_safe(dp->label, _countof(dp->label), "Test Function Under Cursor");
    }

    {
        auto dp = &debug_profiles[debug_profiles_len++];
        dp->type = DEBUG_RUN_PACKAGE;
        strcpy_safe(dp->label, _countof(dp->label), "Run Package");
        dp->run_package.use_current_package = true;
    }

    {
        auto dp = &debug_profiles[debug_profiles_len++];
        dp->type = DEBUG_RUN_BINARY;
        strcpy_safe(dp->label, _countof(dp->label), "Run Binary");
    }

    {
        auto dp = &debug_profiles[debug_profiles_len++];
        dp->type = DEBUG_TEST_PACKAGE;
        strcpy_safe(dp->label, _countof(dp->label), "Test Package");
        dp->test_package.use_current_package = true;
    }
}

void Project_Settings::read(ccstr file) {
    File f;
    if (f.init(file, FILE_MODE_READ, FILE_OPEN_EXISTING) != FILE_RESULT_SUCCESS) {
        load_defaults();
        return;
    }
    defer { f.cleanup(); };

    f.read((char*)this, sizeof(*this));
}

void Project_Settings::write(ccstr file) {
    File f;
    if (f.init(file, FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS)
        return;
    defer { f.cleanup(); };

    f.write((char*)this, sizeof(*this));
}

Build_Profile *Project_Settings::get_active_build_profile() {
    return &build_profiles[active_build_profile];
}

Debug_Profile *Project_Settings::get_active_debug_profile() {
    return &debug_profiles[active_debug_profile];
}
