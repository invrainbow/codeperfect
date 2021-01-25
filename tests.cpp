#include "tests.hpp"

#include "debugger.hpp"
#include "common.hpp"
#include "world.hpp"
#include "nvim.hpp"

bool run_tests() {
    return false;
    /*
    return false;

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
    */

    world.init(true);

    Process proc;
    proc.init();
    proc.use_stdin = true;
    proc.dir = "w:\\";
    proc.run("go run buildparser.go");

    proc.writestr("C:\\Users\\Brandon\\go\\pkg\\mod\\golang.org\\x\\text@v0.3.5\\width\\gen.go\n");
    char b1;
    if (!proc.read1(&b1)) error("error");

    proc.writestr("C:\\Users\\Brandon\\go\\pkg\\mod\\golang.org\\x\\text@v0.3.5\\width\\width.go\n");
    char b2;
    if (!proc.read1(&b2)) error("error");

    print("gen.go = %d, width.go = %d", b1, b2);
    system("pause");
    return true;
}
