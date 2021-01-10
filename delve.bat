cd w:
:loop
dlv debug gotest.go --headless --listen=127.0.0.1:1234 --log
goto loop
