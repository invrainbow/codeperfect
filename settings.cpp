#include "settings.hpp"
#include "utils.hpp"

Settings settings;
Options options;
Project_Settings project_settings;

// --- main code

void Project_Settings::load_defaults() {
    ptr0(this);
    build_profiles = alloc_list<Build_Profile>();
    debug_profiles = alloc_list<Debug_Profile>();

    // This is designed so that debug/build profiles is never empty, and
    // active_debug_profile/active_build_profile always points to a valid profile.

    active_debug_profile = 1; // can't select "test function under cursor" as default profile

    {
        auto bp = build_profiles->append();
        cp_strcpy_fixed(bp->label, "Project");
        cp_strcpy_fixed(bp->cmd, "go build");
    }

    {
        auto dp = debug_profiles->append();
        dp->type = DEBUG_TEST_CURRENT_FUNCTION;
        dp->is_builtin = true;
        cp_strcpy_fixed(dp->label, "Test Function Under Cursor");
    }

    {
        auto dp = debug_profiles->append();
        dp->type = DEBUG_RUN_PACKAGE;
        cp_strcpy_fixed(dp->label, "Run Package");
        dp->run_package.use_current_package = true;
    }

    {
        auto dp = debug_profiles->append();
        dp->type = DEBUG_RUN_BINARY;
        cp_strcpy_fixed(dp->label, "Run Binary");
    }

    {
        auto dp = debug_profiles->append();
        dp->type = DEBUG_TEST_PACKAGE;
        cp_strcpy_fixed(dp->label, "Test Package");
        dp->test_package.use_current_package = true;
    }
}
