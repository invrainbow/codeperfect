@echo off

go build -ldflags "-s -w" -o ../autoupdate.exe github.com/invrainbow/ide/helpers/autoupdate
go build -ldflags "-s -w" -o ../static_helper.exe github.com/invrainbow/ide/helpers/static_helper
go build -ldflags "-s -w" -o ../license_check.exe github.com/invrainbow/ide/helpers/license_check
