set clipboard=unnamed
set ignorecase
set autoindent
set noexpandtab
set startofline
set scrolloff=100
set nowrap

" called from ide when opening/creating new file to reset undo tree
function! IDE__ClearUndo(bufId)
    let oldlevels = &undolevels
    call nvim_buf_set_option(a:bufId, 'undolevels', -1)
    call nvim_buf_set_lines(a:bufId, 0, 0, 0, [])
    call nvim_buf_set_option(a:bufId, 'undolevels', oldlevels)
    unlet oldlevels
endfunction

function! NotifyIDE(cmd, ...) abort
    call rpcnotify(g:channel_id, 'custom_notification', a:cmd, a:000)
endfunction

function s:reveal(where, resetCursor)
    call NotifyIDE('reveal_line', a:where, a:resetCursor)
endfunction

nnoremap z<CR> <Cmd>call <SID>reveal(0, 1)<CR>
xnoremap z<CR> <Cmd>call <SID>reveal(0, 1)<CR>
nnoremap zt <Cmd>call <SID>reveal(0, 0)<CR>
xnoremap zt <Cmd>call <SID>reveal(0, 0)<CR>
nnoremap z. <Cmd>call <SID>reveal(1, 1)<CR>
xnoremap z. <Cmd>call <SID>reveal(1, 1)<CR>
nnoremap zz <Cmd>call <SID>reveal(1, 0)<CR>
xnoremap zz <Cmd>call <SID>reveal(1, 0)<CR>
nnoremap z- <Cmd>call <SID>reveal(2, 1)<CR>
xnoremap z- <Cmd>call <SID>reveal(2, 1)<CR>
nnoremap zb <Cmd>call <SID>reveal(2, 0)<CR>
xnoremap zb <Cmd>call <SID>reveal(2, 0)<CR>

function s:moveCursor(to)
    " register jumplist in vim
    normal! m'
    call NotifyIDE('move_cursor', a:to)
endfunction

nnoremap H <Cmd>call <SID>moveCursor(0)<CR>
xnoremap H <Cmd>call <SID>moveCursor(0)<CR>
nnoremap M <Cmd>call <SID>moveCursor(1)<CR>
xnoremap M <Cmd>call <SID>moveCursor(1)<CR>
nnoremap L <Cmd>call <SID>moveCursor(2)<CR>
xnoremap L <Cmd>call <SID>moveCursor(2)<CR>

function s:jump(direction)
    call NotifyIDE('jump', a:direction)
endfunction

nnoremap <C-o> <Cmd>call <SID>jump(0)<CR>
nnoremap <C-i> <Cmd>call <SID>jump(1)<CR>
nnoremap <Tab> <Cmd>call <SID>jump(1)<CR>
nnoremap : <nop>
nnoremap Q <nop>
nnoremap K <nop>

scriptencoding utf-8

set shortmess=filnxtToOFI
set nowrap
set mouse=a
set cmdheight=1
set wildmode=list
set wildchar=<C-e>

set nobackup
set nowb
set noswapfile
set noautoread
set scrolloff=100
set conceallevel=0
set nocursorline

set hidden
set bufhidden=hide
set noautowrite
set norelativenumber
set nonumber
set list
syntax on
set signcolumn=no

set statusline=
set laststatus=0
set noruler
set nomodeline
set modelines=0
set nofoldenable
set foldmethod=manual

" Turn on auto-indenting
set autoindent
set smartindent

set inccommand=

" lazyredraw breaks the movement
set nolazyredraw

function s:forceLocalOptions()
    setlocal nowrap
    setlocal conceallevel=0
    setlocal scrolloff=100
    setlocal hidden
    setlocal bufhidden=hide
    setlocal noautowrite
    setlocal nonumber
    setlocal norelativenumber
    setlocal list
    setlocal nofoldenable
    setlocal foldmethod=manual
    setlocal nolazyredraw
endfunction

augroup IDE
    autocmd!
    autocmd BufEnter,FileType * call <SID>forceLocalOptions()
augroup END
