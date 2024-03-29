#!/usr/bin/env python3

# Enums must not contain any blank lines between the "enum X {" and "}"
import os
import pathlib


fhpp = None
fcpp = None
write_hpp = lambda s: fhpp.write(s + "\n")
write_cpp = lambda s: fcpp.write(s + "\n")


def main():
    write_hpp("// generated by sh/generate_enums.py")
    write_hpp("#pragma once")

    write_cpp('#include "enums.hpp"')
    write_cpp("")

    for _file in os.listdir("src"):
        file = os.path.join("src", _file)

        if os.path.isdir(file):

            continue
        if not file.endswith(".hpp"):
            continue
        if file == "enums.hpp":
            continue

        with open(file) as f:
            blocks = f.read().split("\n\n")
            first = True

            for block in blocks:
                lines = block.splitlines()
                if not lines:
                    continue
                if not lines[0].startswith("enum "):
                    continue

                enum_name = lines[0].split()[1]
                if not all(ch.isalnum() or ch == "_" for ch in enum_name):
                    continue

                idents = []
                for line in lines[1:-1]:
                    line = line.strip()
                    for i, ch in enumerate(line):
                        if not ch.isalnum() and ch != "_":
                            if i > 0:
                                idents.append(line[:i])
                            break

                if not idents:
                    continue

                if first:
                    write_hpp("")
                    write_hpp(f'#include "{file}"')
                    first = False

                write_hpp("ccstr %s_str(%s type);" % (enum_name.lower(), enum_name))

                write_cpp("ccstr %s_str(%s type) {" % (enum_name.lower(), enum_name))
                write_cpp("    switch (type) {")
                for ident in idents:
                    write_cpp(f"    define_str_case({ident});")
                write_cpp("    }")
                write_cpp("    return NULL;")
                write_cpp("}")
                write_cpp("")


if __name__ == "__main__":
    try:
        with open("src/enums.hpp", "w") as fhpp:
            with open("src/enums.cpp", "w") as fcpp:
                main()
    except:
        pathlib.Path('src/enums.hpp').unlink(missing_ok=True)
        pathlib.Path('src/enums.cpp').unlink(missing_ok=True)
