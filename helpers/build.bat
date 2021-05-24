@echo off

go build -o ../autoupdate.exe github.com/invrainbow/ide/helpers/autoupdate
go build -o ../static_helper.exe github.com/invrainbow/ide/helpers/static_helper
go build -o ../license_check.exe github.com/invrainbow/ide/helpers/license_check
