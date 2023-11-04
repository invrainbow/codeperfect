import sys
import subprocess
import os

with open('src/binaries.c', 'wb') as f:
    for it in sys.argv[1:]:
        cmd = f'xxd -i {it}'
        f.write(subprocess.check_output(cmd, shell=True))
