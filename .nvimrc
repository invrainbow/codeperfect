let &makeprg = 'build.bat'
nnoremap <Leader>b :Make<CR>
autocmd BufWritePre * %s/\s\+$//e
let g:go_fmt_autosave = 0
