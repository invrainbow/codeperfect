if has('win32')
    let &makeprg = '.\build_windows.bat'
elseif has('macunix')
    let &makeprg = './build_macos.sh'
end

nnoremap <Leader>b :Make<CR>

" turns `void Go_Index::foo() {` into `void foo()`;
nnoremap <leader>f 0f(bhhhbd2f:f{xs;<esc>>>
