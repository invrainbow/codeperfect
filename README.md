# CodePerfect 95

### Windows setup

No way it even compiles anymore, have to figure it out when we decide to support Windows again.

Old instructions:

> Install choco, then run:
>
>     choco install visualstudio2019community --add Microsoft.VisualStudio.Workload.NativeDesktop
>     choco install neovim --pre
>     choco install nuget
>
> Make sure nuget is using nuget.org as source. Open `%APPDATA\NuGet\NuGet.Config`
> and paste
>
>     <?xml version="1.0" encoding="utf-8"?>
>     <configuration>
>         <packageSources>
>             <add key="nuget.org" value="https://api.nuget.org/v3/index.json" />
>         </packageSources>
>     </configuration>
>
> Install packages
>
>     nuget restore -PackagesDirectory packages
>
> Create a bin folder, put a copy of nvim.exe inside.

### macOS setup

Install brew, then install dependencies:

    sh/macos_install_dependencies.sh

Build:

    sh/build_macos.sh

Run:

    cd build/bin
    ./ide

### Cutting a new release

On each machine, increment `CurrentVersion` in versions.go, then run
`sh/package_macos`. Then, update the app/update hashes and merge all into
master. Then re-deploy the API.
