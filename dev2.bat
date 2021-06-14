@echo off
subst w: c:\users\brandon\dev\ide >NUL
timeout 1 >NUL
wt -d w: cmd /k "shell.bat & nvim" ; -d w: cmd /k "shell.bat & devenv bin\ide.exe"
rem wt -d w: cmd /k "shell.bat & nvim" ; -d w: cmd /k "shell.bat & remedybg bin\ide.exe"
