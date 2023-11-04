import sys
import subprocess
import os

with open('src/binaries.c', 'wb') as f:
    os.chdir('src')
    for it in sys.argv[1:]:
        filename = os.path.basename(it)
        cmd = f'xxd -i {filename}'
        f.write(subprocess.check_output(cmd, shell=True))
