let &makeprg = 'sh/build'

nnoremap <Leader>b :Make<CR>

" turns `void Go_Index::foo() {` into `void foo()`;
nnoremap <leader>f 0f(bhhhbd2f:f{xs;<esc>>>

" stupid macro for converting treesitter enum
function ConvertTSEnum()
    %!grep "\S"
    %s/^ *//g
    %s/\<anon_sym_DOT\>/dontchangeme/g
    %s/^anon_//g
    %s/^aux_//g
    %s/^alias_//g
    %s/^sym_//g
    %s/dontchangeme/anon_dot/g
    %s/^/TS_/g
    %s/__/_/g
    normal ggVGU
endfunction

autocmd BufRead,BufNewFile *.m,*.mm set cindent
