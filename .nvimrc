let &makeprg = 'build.bat'
nnoremap <Leader>b :Make<CR>
autocmd BufWritePre * %s/\s\+$//e
