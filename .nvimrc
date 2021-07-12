if has('win32')
    let &makeprg = '.\build_windows.bat'
elseif has('macunix')
    let &makeprg = './build_macos.sh'
end

nnoremap <Leader>b :Make<CR>

" turns `void Go_Index::foo() {` into `void foo()`;
nnoremap <leader>f 0f(bhhhbd2f:f{xs;<esc>>>

" stupid macro for converting treesitter enum
function ConvertTSEnum()
    %s/^anon_//g
    %s/^aux_//g
    %s/^alias_//g
    %s/^sym_//g
    %s/^/TS_/g
    %s/__/_/g
    normal ggVGU
endfunction
