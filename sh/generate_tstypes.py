#!/usr/bin/env python3.9

PARSER_FILES = (
    ('src/tree-sitter-go/src/parser.c', 'ts'),
    ('src/tree-sitter-go-work/src/parser.c', 'tsgw'),
    ('src/tree-sitter-go-mod/src/parser.c', 'tsgm'),
)

#  ----------------------
#  actual logic of script
#  ----------------------

import re

gf = None


def write(s=''):
    gf.write(s + "\n")


def handle_parserc_file(filepath, prefix):
    with open(filepath) as f:
        content = f.read()

    has_fields = False
    for line in content.splitlines():
        parts = line.split()
        if len(parts) == 3:
            if parts[0] == '#define' and parts[1] == 'FIELD_COUNT':
                if parts[2] != '0':
                    has_fields = True

    ast_prefix = prefix.upper() + '_'
    field_prefix = (prefix + 'f').upper() + '_'

    def transform_ast_line(it):
        it = it.replace("__", "_").strip().lower()
        if it.startswith("anon_sym_dot = "):
            it = it.replace("anon_sym_dot", "anon_anon_dot")
        for prefix in ("anon_", "aux_", "alias_", "sym_"):
            it = it.removeprefix(prefix)
        return ast_prefix + it.upper()

    def transform_field_line(it):
        return it.strip().lower().replace("field_", field_prefix).upper()

    blocks = [it.splitlines() for it in content.split("\n\n")]
    enums = [it[1:-1] for it in blocks if it[0] == "enum {"]

    ast_types = [transform_ast_line(it) for it in enums[0]]
    field_types = []
    if has_fields:
        field_types = [transform_field_line(it) for it in enums[1]]

    write("enum " + prefix.capitalize() + "_Ast_Type {")
    write(f"    {prefix.upper()}_ERROR = ((TSSymbol)-1),")
    for line in ast_types:
        write(f"    {line}")
    write("};")
    write()

    if has_fields:
        write("enum " + prefix.capitalize() + "_Field_Type {")
        for line in field_types:
            write(f"    {line}")
        write("};")
        write()


def main():
    global gf
    with open("src/tstypes.hpp", "w") as file:
        gf = file

        write("// generated by sh/generate_tstypes.py")
        write("#pragma once")
        write('#include "tree_sitter_crap.hpp"')
        write()

        for filepath, prefix in PARSER_FILES:
            handle_parserc_file(filepath, prefix)

        gf = None


if __name__ == '__main__':
    main()
