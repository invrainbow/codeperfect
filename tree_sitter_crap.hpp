#pragma once

#include "tree-sitter/tree_sitter/api.h"

extern "C" TSLanguage *tree_sitter_go();
extern "C" void init_treesitter_go_trie();
