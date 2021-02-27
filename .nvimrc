let &makeprg = 'build.bat'
nnoremap <Leader>b :Make<CR>
" autocmd BufWritePre * %s/\s\+$//e
let g:go_fmt_autosave = 0

" turns `void Go_Index::foo() {` into `void foo();
nnoremap <leader>f 0f(bhhhbd2f:f{xs;<esc>>>
