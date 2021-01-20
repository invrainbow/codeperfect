@echo off
subst w: c:/users/brandon/ide >NUL
timeout 1 >NUL
wt -d w: cmd /k "shell.bat & nvim" ; -d w: cmd /k "shell.bat & debug.bat"
