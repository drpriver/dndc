" This doesn't do semantics.
syn match DnDLink "\[.\{-}\]"
syn match DnDBullet "^\s*[\*\-+]"
syn match DnDNodeLine "^.*::.*$" contains=DnDType,DnDTitle
syn match DnDType "::.*$" contained
syn match DnDTitle "^.*\(::\)\@=" contained
syn match DnDNodeLine "^::.*$" contains=DnDType
syn match DnDType "::.*$" contained
syn match DnDNumberedList "^\s*\d\+\.\(.*::\)\@!"
hi link DnDLink Type
hi DnDTitle ctermfg=12 guifg=Blue
hi DnDBullet ctermfg=7 guifg=#444444
hi DnDNumberedList ctermfg=7 guifg=#444444
hi DnDType ctermfg=15 guifg=#aaaaaa
" Highlight js and css using the other vim syntaxes.  I didn't write an
" indentation based ending rule as that seemed hard and requires a lot of
" lookahead. The current workaround is to end js sections with //endjs and
" css sections with /*endcss*/. This is a total hack though and if I was
" more willing to learn vim commands it wouldn't be necessary.
if exists("b:current_syntax")
    unlet b:current_syntax
endif
syntax include @javascript syntax/javascript.vim
syntax region jsSnip matchgroup=Snip2 start="\s*::script$" end="//endjs" contains=@javascript
syntax region jsSnip matchgroup=Snip3 start="\s*.*::js$" end="//endjs" end="@import" contains=@javascript
if exists("b:current_syntax")
    unlet b:current_syntax
endif
syntax include @css syntax/css.vim
syntax region jsSnip matchgroup=Snip2 start="\s*::css$" end="/\*endcss\*/" contains=@css
hi link Snip SpecialComment
hi link Snip2 SpecialComment
hi link Snip3 SpecialComment
