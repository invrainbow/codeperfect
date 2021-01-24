#include "tests.hpp"

#include "debugger.hpp"
#include "common.hpp"
#include "world.hpp"
#include "nvim.hpp"

bool run_tests() {
    // return false;

    world.init(true);
    Go_Index index;

    Resolved_Import *ri = NULL;
    auto import_path = "github.com/invrainbow/whetstone";
    auto hash = index.hash_package(import_path, &ri);
    print("package: %s", import_path);
    print("hash = %llx", hash);
    print("path = %s", ri->path);
    print("package name = %s", ri->package_name);
    print("location_type = %d", ri->location_type);

    system("pause");
    return true;
}
