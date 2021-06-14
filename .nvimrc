let &makeprg = 'build.bat'
nnoremap <Leader>b :Make<CR>

" turns `void Go_Index::foo() {` into `void foo()`;
nnoremap <leader>f 0f(bhhhbd2f:f{xs;<esc>>>
