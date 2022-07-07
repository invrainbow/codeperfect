import sys
import subprocess

with open('binaries.c', 'wb') as f:
    for it in sys.argv[1:]:
        cmd = f'xxd -i {it}'
        f.write(subprocess.check_output(cmd))
