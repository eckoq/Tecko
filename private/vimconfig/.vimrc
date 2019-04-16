syntax enable
colorscheme monokai
set tags=tags;/
set omnifunc=syntaxcomplete#Complete
set t_Co=256
set nocompatible
syntax on
set cursorline
set cursorcolumn
set enc=utf-8
set fileencodings=utf-8
set termencoding=utf-8
set encoding=utf-8
set nobackup
set autoindent
set cindent
set shiftwidth=4
set tabstop=4
set expandtab
set softtabstop=4
set listchars=tab:->,trail:-
set fileformats=unix
set backspace=indent,eol,start
set history=50
set showcmd
set number
set incsearch
set hlsearch
set wildmenu
filetype plugin indent on
set cino=:0,l1,g0,p0,t0

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
let Tlist_Use_Right_Window=1
let Tlist_File_Fold_Auto_Close=1
let Tlist_Exit_OnlyWindow=1
""""""""""""""""

""""""""""""""""
"MINBUF插件
""""""""""""""""
let g:miniBufExplMapWindowNavVim = 1 
let g:miniBufExplMapWindowNavArrows = 1 
let g:miniBufExplMapCTabSwitchBufs = 1 
let g:miniBufExplModSelTarget = 1
""""""""""""""""
