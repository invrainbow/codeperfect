import sys
import subprocess


def sh(cmd):
    return subprocess.check_output(cmd, shell=True).decode('utf-8')


flavor = sys.argv[1]
lines = str(sys.stdin.read()).strip().splitlines()

if flavor == '-windows':
    lines, last = lines[:-1], lines[-1]
    base = int(last.split()[2], 16)

    for i, line in enumerate(lines):
        parts = line.split()
        try:
            int(parts[0])
            int(parts[1], 16)
        except:
            continue

        if not parts[1].startswith('0x'): continue

        lines = lines[i:]
        break

    for line in lines:
        addr = line.split()[1]
        addr = hex(int(addr, 16) - base)
        print(f'ln ide.exe+{addr}')

elif flavor == '-mac':
    lines, last = lines[:-1], lines[-1]
    base = int(last.split()[2], 16)

    for i, line in enumerate(lines):
        parts = line.split()
        try:
            int(parts[0])
            int(parts[1], 16)
        except:
            continue

        if not parts[1].startswith('0x'): continue

        lines = lines[i:]
        break

    binary = sys.argv[2]

    for i, line in enumerate(lines):
        addr = line.split()[1]
        cmd = f'atos -o {binary} -l {hex(base)} {addr}'
        output = sh(cmd).strip()
        print(f'{i} {addr} {output}')
