set clipboard=unnamed
set ignorecase
set autoindent
set noexpandtab

" called from ide when opening/creating new file to reset undo tree
function! IDEClearUndo(bufId)
    let oldlevels = &undolevels
    call nvim_buf_set_option(a:bufId, 'undolevels', -1)
    call nvim_buf_set_lines(a:bufId, 0, 0, 0, [])
    call nvim_buf_set_option(a:bufId, 'undolevels', oldlevels)
    unlet oldlevels
endfunction
