lines = open('sh/codepoints').read().splitlines()

lo = float('inf')
hi = float('-inf')
ret = []

for line in lines:
    name, cp = line.split()
    cp = int(cp, 16)

    if cp < lo:
        lo = cp
    if cp > hi:
        hi = cp

    cp = chr(cp).encode('utf-8')
    cp = repr(cp)[1:].replace("'", '"')

    name = 'ICON_MD_' + name.upper()
    ret.append(f"#define {name} {cp}")

ret.append(f"#define ICON_MIN_MD {lo}")
ret.append(f"#define ICON_MAX_MD {hi}")

contents = '\n'.join(ret)
open('icons.h', 'w').write(contents)
print(contents)

