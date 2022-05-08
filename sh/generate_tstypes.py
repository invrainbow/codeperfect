#!/usr/bin/env python3

import re
import sys


def read_shit():
    ast_types = None
    field_types = None

    def transform_ast_line(it):
        it = it.strip()
        it = re.sub(r'\banon_sym_DOT\b', 'dontchangeme', it)
        it = re.sub(r'^anon_', '', it)
        it = re.sub(r'^aux_', '', it)
        it = re.sub(r'^alias_', '', it)
        it = re.sub(r'^sym_', '', it)
        it = it.replace(r'dontchangeme', 'anon_dot')
        it = 'TS_' + it
        it = it.replace('__', '_')
        it = it.upper()
        return '    ' + it

    def transform_field_line(it):
        it = it.strip()
        it = re.sub(r'^field_', '', it)
        it = 'TSF_' + it
        it = it.upper()
        return '    ' + it

    first_enum = True

    with open('tree-sitter-go/src/parser.c') as f:
        blocks = f.read().split('\n\n')

    for block in blocks:
        lines = block.splitlines()
        if not (lines[0] == 'enum {' and lines[-1] == '};'):
            continue

        if first_enum:
            first_enum = False
            ast_types = map(transform_ast_line, lines[1:-1])
        elif all(x.strip().startswith('field_') for x in lines[1:-1]):
            field_types = map(transform_field_line, lines[1:-1])
            break

    return ast_types, field_types


if __name__ == '__main__':
    ast_syms = []
    node_syms = []
    ast_types, node_types = read_shit()

    file = None

    def write(s):
        file.write(s)
        file.write('\n')

    file = open('tstypes.hpp', 'w')
    try:
        write('// generated by sh/generate_tstypes.py')
        write('')
        write('#pragma once')
        write('')
        write('#include "tree_sitter_crap.hpp"')
        write('')
        write('enum Ts_Ast_Type {')
        write('    TS_ERROR = ((TSSymbol)-1),')
        for line in ast_types:
            write(line)
            ast_syms.append(line.strip().split()[0])
        write('};')
        write('')
        write('enum Ts_Field_Type {')
        for line in node_types:
            write(line)
            node_syms.append(line.strip().split()[0])
        write('};')
        write('')
        write('ccstr ts_field_type_str(Ts_Field_Type type);')
        write('ccstr ts_ast_type_str(Ts_Ast_Type type);')
    finally:
        file.close()

    file = open('tstypes.cpp', 'w')
    try:
        write('#include "common.hpp"')
        write('#include "tstypes.hpp"')
        write('')
        write('ccstr ts_ast_type_str(Ts_Ast_Type type) {')
        write('    switch (type) {')
        for sym in ast_syms:
            write(f'    define_str_case({sym});')
        write('    }')
        write('    return NULL;')
        write('}')
        write('')
        write('ccstr ts_field_type_str(Ts_Ast_Type type) {')
        write('    switch (type) {')
        for sym in node_syms:
            write(f'    define_str_case({sym});')
        write('    }')
        write('    return NULL;')
        write('}')

    finally:
        file.close()


