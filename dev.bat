@echo off
subst w: c:/users/brandon/ide >NUL
timeout 1 >NUL
wt -d w: cmd /k "shell.bat & nvim" ; -d w: cmd /k "shell.bat & devenv ide.exe"
rem wt -d w: cmd /k "shell.bat & nvim" ; -d w: cmd /k "shell.bat & remedybg ide.exe"
