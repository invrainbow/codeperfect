scriptencoding utf-8

set encoding=utf-8
set fileencoding=utf-8

set ttimeout
set ttimeoutlen=100
set timeoutlen=3000

" set clipboard=unnamed
set ignorecase
set autoindent
set noexpandtab
set startofline
set scrolloff=100
set nowrap

" called from ide when opening/creating new file to reset undo tree
function! CPClearUndo(bufId)
    let oldlevels = &undolevels
    call nvim_buf_set_option(a:bufId, 'undolevels', -1)
    call nvim_buf_set_lines(a:bufId, 0, 0, 0, [])
    call nvim_buf_set_option(a:bufId, 'undolevels', oldlevels)
    unlet oldlevels
endfunction

function! CPGetVisual(name)
    let m = mode()
    if m == "v" || m == "V" || m == "\<C-V>"
        let [row_s, col_s] = getpos("v")[1:2]
        let [row_e, col_e] = getpos(".")[1:2]
    else
        let [row_s, col_s] = getpos("'<")[1:2]
        let [row_e, col_e] = getpos("'>")[1:2]
    end
    call NotifyIDE("get_visual", a:name, row_s, col_s, row_e, col_e, bufnr("%"), m)
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

" function s:jump(direction)
"     call NotifyIDE('jump', a:direction)
" endfunction

nnoremap <C-o> <Cmd>call NotifyIDE('jump', 0)<CR>
nnoremap <C-i> <Cmd>call NotifyIDE('jump', 1)<CR>
nnoremap <Tab> <Cmd>call NotifyIDE('jump', 1)<CR>

nnoremap <C-d> <Cmd>call NotifyIDE('halfjump', 1)<CR>
nnoremap <C-u> <Cmd>call NotifyIDE('halfjump', 0)<CR>
nnoremap <C-b> <Cmd>call NotifyIDE('pagejump', 0)<CR>
nnoremap <C-f> <Cmd>call NotifyIDE('pagejump', 1)<CR>

nnoremap <C-e> <Cmd>call NotifyIDE('scrollview', 1)<CR>
nnoremap <C-y> <Cmd>call NotifyIDE('scrollview', 0)<CR>

nnoremap Q <nop>
nnoremap K <nop>

function s:goToDefinition()
    call NotifyIDE('goto_definition')
endfunction

nnoremap gd <Cmd>call <SID>goToDefinition()<CR>

nnoremap : <nop>
" nnoremap <F1> :

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
" set listchars=tab:❥♥
" set list
" syntax on
set signcolumn=no

set statusline=
set laststatus=0
set noruler

set nomodeline
set modelines=0

set nofoldenable
set foldmethod=manual

set inccommand=

" lazyredraw breaks the movement
set nolazyredraw

function! GoIndent(lnum)
    let l:prevlnum = prevnonblank(a:lnum-1)
    if l:prevlnum == 0
        return 0
    endif

    let l:prevl = substitute(getline(l:prevlnum), '//.*$', '', '')
    let l:thisl = substitute(getline(a:lnum), '//.*$', '', '')
    let l:previ = indent(l:prevlnum)

    let l:ind = l:previ
    if l:prevl =~ '[({]\s*$'
        let l:ind += shiftwidth()
    endif
    if l:prevl =~# '^\s*\(case .*\|default\):$'
        let l:ind += shiftwidth()
    endif
    if l:thisl =~ '^\s*[)}]'
        let l:ind -= shiftwidth()
    endif
    if l:thisl =~# '^\s*\(case .*\|default\):$'
        let l:ind -= shiftwidth()
    endif
    return l:ind
endfunction

function s:forceLocalOptions()
    " set filetype=go
    " filetype on
    " filetype indent on
    "
    setlocal autoindent
    setlocal indentexpr=GoIndent(v:lnum)
    setlocal indentkeys+=<:>,0=},0=)
endfunction

augroup IDE
    autocmd!
    autocmd BufEnter,FileType * call <SID>forceLocalOptions()
augroup END

" autocmd CursorMoved * call <SID>sendVisualSelection()

vnoremap < <gv
vnoremap > >gv
