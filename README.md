# CodePerfect 95

blah blah, cleaning this up.

### Windows setup

Install choco, then run:

    choco install visualstudio2019community --add Microsoft.VisualStudio.Workload.NativeDesktop
    choco install neovim --pre
    choco install nuget

Make sure nuget is using nuget.org as source. Open %APPDATA\NuGet\NuGet.Config
and paste

    <?xml version="1.0" encoding="utf-8"?>
    <configuration>
        <packageSources>
            <add key="nuget.org" value="https://api.nuget.org/v3/index.json" />
        </packageSources>
    </configuration>

Install packages

    nuget restore -PackagesDirectory packages

Create a bin folder, put a copy of nvim.exe inside.

### macOS setup

Install brew. Install dependencies:

    ./macos_install_dependencies.sh

Build:

    ./build_macos.sh

Run:

    bin/ide

### other info

list of threads:

 * build
 * dbg: pipe to stdout
 * dbg: background thread
 * search: search
 * search: replace
 * go
 * neovim
