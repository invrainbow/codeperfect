Setup
=====

Install choco.

Run:

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

We need to manually build libgit2, because it doesn't come with precompiled
binaries. Install cmake:

    choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System'

Download libgit2 from their Releases on github. cd in and run:

    mkdir build
    cd build
    cmake .. -DBUILD_SHARED_LIBS=OFF -DBUILD_CLAR=OFF
    cmake --build .

Create packages/libgit2. Where `${path_to_libgit2}` is the libgit2 folder you
downloaded from github, copy `${path_to_libgit2}/build/Debug` into
packages/libgit/lib, and `${path_to_libgit2}/include` into
packages/libgit/includelib.

Create a bin folder, put a copy of nvim.exe inside.
