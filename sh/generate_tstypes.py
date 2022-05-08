#!/usr/bin/env python3

import re
import sys


def transform_line(it):
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


def blah():
    with open('tree-sitter-go/src/parser.c') as f:
        for block in f.read().split('\n\n'):
            lines = block.splitlines()
            if lines[0] == 'enum {':
                return map(transform_line, lines[1:-1])


if __name__ == '__main__':
    syms = []
    file = None

    def write(s):
        file.write(s)
        file.write('\n')

    file = open('tstypes.hpp', 'w')
    try:
        write('#pragma once')
        write('')
        write('#include "tree_sitter_crap.hpp"')
        write('')
        write('// mirrors tree-sitter/src/go.h')
        write('// use sh/generate_tstypes.py to generate')
        write('enum Ts_Ast_Type {')
        write('    TS_ERROR = ((TSSymbol)-1),')
        for line in blah():
            write(line)
            syms.append(line.strip().split()[0])
        write('};')
    finally:
        file.close()

    file = open('tstypes.cpp', 'w')
    try:
        write('#include "common.hpp"')
        write('#include "tstypes.hpp"')
        write('')
        write('ccstr ts_ast_type_str(Ts_Ast_Type type) {')
        write('    switch (type) {')
        for sym in syms:
            write(f'    define_str_case({sym});')
        write('    }')
        write('    return NULL;')
        write('}')
    finally:
        file.close()


