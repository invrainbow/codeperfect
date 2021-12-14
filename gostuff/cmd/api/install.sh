#!/bin/bash
set -e

OS_NAME=darwin
if [[ "$(sysctl -n machdep.cpu.brand_string)" == "Apple M1"* ]]; then
    OS_NAME=darwin_arm
fi

if [[ -z "$(which go)" ]]; then
    error_message="Go 1.13+ is needed to use CodePerfect. Please install it first, then rerun the install script."

    read -p "Go is not installed. Do you want to install it using Homebrew? (y/n) " -r
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if [[ -z "$(which brew)" ]]; then
            read -p "Homebrew is not installed. Do you want to install it? (y/n) " -r
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                if [ "$OS_NAME" = "darwin_arm" ]; then
                    arch -arm64 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
                else
                    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
                fi
                echo "Homebrew installed."
            else
                echo "$error_message"
                exit 1
            fi
        fi

        echo "Installing Go..."
        if [ "$OS_NAME" = "darwin_arm" ]; then
            arch -arm64 brew install go
        else
            brew install go
        fi
    else
        echo "$error_message"
        exit 1
    fi
fi

CODEPERFECT_CODE="{{.Code}}"
API_BASE="{{.APIBase}}"

download_codeperfect() {
    TMPDIR=$(mktemp -d)
    pushd "${TMPDIR}" > /dev/null 2>&1

    DOWNLOAD_URL="${API_BASE}/download?code=${CODEPERFECT_CODE}&os=${OS_NAME}&noredirect=1"
    BINARY_URL="$(curl -s "${DOWNLOAD_URL}")"

    if [[ ! "$BINARY_URL" == "https://"* ]]; then
        echo " error"
        echo "$BINARY_URL"  # download url contains error, print it out
        exit 1
    fi

    unzip-file() {
        curl -s -o codeperfect.zip "${BINARY_URL}"
        unzip codeperfect.zip
        rm -rf /Applications/CodePerfect.app
        mv CodePerfect.app /Applications
    }

    unzip-file > /dev/null 2>&1
    popd > /dev/null 2>&1

    rm -rf $TMPDIR
}

download_license() {
    curl -s -o ~/.cplicense "${API_BASE}/license?code=${CODEPERFECT_CODE}"
}

create_config() {
    echo "{" > ~/.cpconfig
    echo "  \"gopath\": \"$(go env GOPATH go)\"," >> ~/.cpconfig
    echo "  \"go_binary_path\": \"$(which go)\"," >> ~/.cpconfig
    echo "  \"goroot\": \"$(go env GOROOT)\"," >> ~/.cpconfig
    echo "  \"gomodcache\": \"$(go env GOMODCACHE)\"" >> ~/.cpconfig
    echo "}" >> ~/.cpconfig
}

echo -n "Downloading CodePerfect..."
download_codeperfect 
echo " done!"

echo -n "Downloading license..."
download_license > /dev/null 2>&1
echo " done!"

echo -n "Configuring..."
create_config > /dev/null 2>&1
echo " done!"

echo ""
echo "CodePerfect.app is available to use in your Applications folder."

open /Applications/CodePerfect.app
