#!/bin/bash
set -e

VERSION="$(grep CurrentVersion gostuff/versions/versions.go | rev | cut -d' ' -f 1 | rev)"

# build again
RELEASE=1 sh/build_macos.sh

# clean up bin folder
rm -f build/bin/.DS_Store

package_app() {
    APPNAME="CodePerfect.app"
    rm -rf "${APPNAME}"
    mkdir -p "${APPNAME}/Contents/MacOS"
    cp -R ../build/bin/ "${APPNAME}/Contents/MacOS/bin/"
    cp ../build/launcher "${APPNAME}/Contents/MacOS/CodePerfect"
    zip -r app.zip CodePerfect.app/
    aws s3 cp app.zip "s3://codeperfect95/app/darwin_v${VERSION}.zip"
}

package_update() {
    zip -j -r update.zip ../build/bin/ 
    aws s3 cp update.zip "s3://codeperfect95/update/darwin_v${VERSION}.zip"
}

mkdir scratch
pushd scratch
package_app
package_update
popd
rm -rf scratch
