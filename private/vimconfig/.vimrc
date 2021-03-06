syntax enable
colorscheme monokai
set tags=tags;/
set encoding=utf-8
set number
set showcmd
set nobackup

" 横竖直线
set cursorline
"set cursorcolumn

" 输入设置
set backspace=2

set tabstop=2
" makefile tabs
autocmd FileType make setlocal noexpandtab

set shiftwidth=2
set smarttab
set expandtab
" 缩进设置
set autoindent
set smartindent
set cindent
set cinoptions=g1,h1,i4,l1,m1,N-s,t0,W4,+2s,:2,(0
" 显示设置
" support gnu syntaxt
let c_gnu = 1
" show error for mixed tab-space
let c_space_errors = 1
"let c_no_tab_space_error = 1
" don't show gcc statement expression ({x, y;}) as error
let c_no_curly_error = 1
" 缩进格式修正：模板参数表，类初始化列表等
function! FixCppIndent()
    let l:cline_num = line('.')
    let l:cline = getline(l:cline_num)
    let l:pline_num = prevnonblank(l:cline_num - 1)
    let l:pline = getline(l:pline_num)
    while l:pline =~# '\(^\s*{\s*\|^\s*//\|^\s*/\*\|\*/\s*$\)'
        let l:pline_num = prevnonblank(l:pline_num - 1)
        let l:pline = getline(l:pline_num)
    endwhile
    let l:retv = cindent('.')
    let l:pindent = indent(l:pline_num)
    if l:pline =~# '^\s*template\s*<\s*$'
        let l:retv = l:pindent + &shiftwidth
    elseif l:pline =~# '^\s*template\s*<.*>\s*$'
        let l:retv = l:pindent
    elseif l:pline =~# '\s*typename\s*.*,\s*$'
        let l:retv = l:pindent
    elseif l:pline =~# '\s*typename\s*.*>\s*$'
        let l:retv = l:pindent - &shiftwidth
    elseif l:cline =~# '^\s*>\s*$'
        let l:retv = l:pindent - &shiftwidth
    elseif l:pline =~# '^\s\+: \S.*' " C++ initialize list
        let l:retv = l:pindent + 2
    elseif l:cline =~# '^\s*:\s*\w\+(' " C++ initialize list
        let l:retv = l:pindent + 4
    else
        echo "No match"
    endif
    return l:retv
endfunction
autocmd FileType cpp nested setlocal indentexpr=FixCppIndent()
" 保存时自动删除行尾多余的空白字符
function! RemoveTrailingSpace()
    if $VIM_HATE_SPACE_ERRORS != '0'
        normal m`
        silent! :%s/\s\+$//e
        normal ``
    endif
endfunction
autocmd BufWritePre * nested call RemoveTrailingSpace()
" 修复不一致的换行符，统一采用 Unix 换行符(\n)
function! FixInconsistFileFormat()
    if &fileformat == 'unix'
        silent! :%s/\r$//e
    endif
endfunction
autocmd BufWritePre * nested call FixInconsistFileFormat()
" 增加'CppLint'自定义命令，在冒号模式下输入:CppLint即可检查当前文件，也支持带文件名参数。
" 检查结果支持 vim 的  QuickFix 模式，在不离开 vim 的情况下跳转到出错行方便，修正。
function! CppLint(...)
    let l:args = join(a:000)
    if l:args == ""
        let l:args = expand("%")
        if l:args == ""
            let l:args = '*'
        endif
    endif
    let l:old_makeprg = &makeprg
    setlocal makeprg=cpplint.py
    execute "make " . l:args
    let &makeprg=old_makeprg
endfunction
command! -complete=file -nargs=* CppLint call CppLint('<args>')

""""""""""""""""
"YCM自动补全插件
""""""""""""""""
let g:ycm_global_ycm_extra_conf='~/.vim/plugin/YouCompleteMe/third_party/ycmd/examples/.ycm_extra_conf.py'
let g:ycm_show_diagnostics_ui = 0
"nnoremap <c-h> :YcmCompleter GoToDeclaration<CR>|
"nnoremap <c-j> :YcmCompleter GoToDefinition<CR>|
""""""""""""""""

""""""""""""""""
"TAGTLIST插件
""""""""""""""""
let Tlist_Inc_Winwidth=0
let Tlist_Use_Right_Window=0
let Tlist_File_Fold_Auto_Close=1
let Tlist_Exit_OnlyWindow=1
""""""""""""""""

""""""""""""""""
"MINBUF插件
""""""""""""""""
map <F3> :Tlist<CR>
let g:miniBufExplMapWindowNavVim = 1
let g:miniBufExplMapWindowNavArrows = 1
let g:miniBufExplMapCTabSwitchBufs = 1
let g:miniBufExplModSelTarget = 1
""""""""""""""""

""""""""""""""""
"NERDTree插件
""""""""""""""""
" 按下 F2 调出/隐藏 NERDTree
map <F2> :NERDTree<CR>
let g:NERDTreeWinPos ="right"
autocmd bufenter * if (winnr("$") == 1 && exists("b:NERDTreeType") &&b:NERDTreeType == "primary") | q | endif
""""""""""""""""
