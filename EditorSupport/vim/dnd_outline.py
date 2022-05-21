"""
This provides an "outline" buffer on the left hand side. It does not
auto-update, but you can toggle it to get an update.

For python and dnd, we parse the ast directly.
For other languages, we use the local tag files.
"""
import vim
import os
import ast
import sys
from typing import List, NamedTuple, Optional, Union

class Loc(NamedTuple):
    depth: int
    header: str
    row: Union[int, str]
    def indent(self) -> str:
        bullet = '•‣⁃••••••••••••••••••••••••••••••••••'[self.depth]
        return ' '+self.depth*2*' '+bullet+' '+self.header

def is_supported() -> bool:
    return vim.eval('&ft') in {'dnd', 'python'} or os.path.isfile('tags')

current_outline: Optional['OutLine'] = None
disabled = True
initialized = False

def toggle() -> None:
    ft: str = vim.eval('&ft')
    global disabled
    global initialized
    if disabled:
        if not is_supported(): return
        disabled = False
        outline()
        if current_outline:
            vim.command(f'call win_gotoid({current_outline.thiswinid})')
        else:
            disabled = True
    else:
        disabled = True
        if current_outline:
            current_outline.close()
    if not initialized:
        vim.command('au BufRead,BufNewFile *.dnd python3 dnd_outline.outline()')
        initialized = True

def dnd_outline() -> List[Loc]:
    import pydndc # type: ignore
    def get_loc(n:pydndc.Node, depth:int) -> Optional[Loc]:
        if not n.id: return None
        if not n.header: return None
        return Loc(depth, n.header, n.location.row)

    def get_locs(n:pydndc.Node, depth:int) -> List[Loc]:
        result = [] # type: List[Loc]
        for child in n.children:
            loc = get_loc(child, depth)
            if loc is not None:
                result.append(loc)
            result.extend(get_locs(child, depth+1 if loc is not None else depth))
        return result
    ctx = pydndc.Context()
    ctx.logger = pydndc.stderr_logger
    cb: vim.Buffer = vim.current.buffer
    text = '\n'.join(cb)
    ctx.root.parse(text)
    locs = get_locs(ctx.root, 0)
    return locs

def tag_outline() -> List[Loc]:
    locs = [] # type: List[Loc]
    thisfile = vim.eval("expand('%:p')")
    with open('tags') as fp:
        for line in fp:
            if line.startswith('!'): continue
            if '#define' in line: continue
            parts = line.split('\t')
            file = os.path.abspath(parts[1])
            if file != thisfile: continue
            header = parts[0]
            cmd = parts[2].split('"')[0].replace('*', r'\*').rstrip()
            locs.append(Loc(0, header, cmd))
        text = '\n'.join(vim.current.buffer)
        locs.sort(key = lambda x:text.find(x.header))
    return locs

def py_outline() -> List[Loc]:
    data = '\n'.join(vim.current.buffer)
    def get_locs(node:Union[ast.ClassDef, ast.FunctionDef], depth:int) -> List[Loc]:
        locs = []
        locs.append(Loc(depth, node.name, node.lineno))
        for b in node.body:
            if isinstance(b, (ast.ClassDef, ast.FunctionDef)):
                locs.extend(get_locs(b, depth+1))
        return locs
    mod = ast.parse(data)
    locs = []
    for n in mod.body:
        if isinstance(n, (ast.ClassDef, ast.FunctionDef)):
            locs.extend(get_locs(n, 0))
    return locs

def outline() -> None:
    if disabled: return
    global current_outline
    bufid: int = vim.eval('bufnr()')
    winid: int = vim.eval('win_getid()')
    if current_outline:
        if current_outline.bufid == bufid:
            return
        current_outline.close()
    if not is_supported(): return
    try:
        ft = vim.eval('&ft')
        func = {'dnd':dnd_outline, 'python':py_outline}.get(ft, tag_outline)
        locs = func()
    except Exception as e:
        print(e)
        return
    current_outline = OutLine(locs, winid, bufid)


class OutLine:
    def __init__(self, locs:List[Loc], winid:int, bufid:int) -> None:
        self.locs = locs
        self.winid = winid
        self.bufid = bufid
        vim.command('silent! keepalt leftabove vert new [Outline]')
        vim.command('silent! setlocal buftype=nofile')
        vim.command('silent! setlocal modifiable')
        vim.command('silent! setlocal noswapfile')
        vim.command('silent! setlocal nowrap')
        vim.command('silent! setlocal cursorline')
        vim.command('silent! setlocal nobuflisted')
        vim.command('silent! setlocal sw=2')
        vim.command('silent! setlocal foldmethod=indent')
        vim.command('silent! nnoremap <script> <silent> <nowait> <buffer> <2-leftmouse> :call DndOutLineGoToNode()<CR>')
        vim.command('silent! nnoremap <script> <silent> <nowait> <buffer> <CR> :call DndOutLineGoToNode()<CR>')
        vim.command('silent! au WinClosed <buffer> python3 if dnd_outline.current_outline: dnd_outline.toggle()')

        names = [l.indent() for l in locs]
        vim.current.buffer[:] = names
        vim.command('silent! setlocal nomodifiable')
        vim.command('silent! setlocal winwidth=30')
        vim.command('silent! setlocal winfixwidth')
        vim.command('silent! setlocal nonumber')
        vim.command('silent! vert resize 32')
        self.thiswinid = vim.eval('win_getid()')
        vim.command(f'silent! call win_gotoid({winid})')

    def select(self) -> None:
        line = vim.eval('getcurpos()')[1]
        loc = self.locs[int(line)-1]
        vim.command(f'call win_gotoid({self.winid})')
        try:
            cmd = str(loc.row)
            # print('cmd: ', '`'+cmd+'`')
            vim.command(cmd)
            vim.command('silent! foldopen!')
            vim.command('normal j')
            vim.command('silent! foldopen!')
            vim.command('normal k')
            vim.command('normal zt')
        except:
            pass
        vim.command(f'call win_gotoid({self.thiswinid})')


    def close(self) -> None:
        global current_outline
        current_outline = None
        vim.command(f"call win_execute({self.thiswinid}, 'close')")
        vim.command(f'call win_gotoid({self.winid})')

# Add this to your .vimrc or whatever
r'''
python3 << endpy
import sys
import os
sys.path.append(os.path.expanduser('~/.vim'))
import dnd_outline
endpy

function! g:DndOutLineGoToNode()
    :python3 dnd_outline.current_outline.select()
endfunction

" [o]utline is the mnemonic
nnoremap <leader>o :python3 dnd_outline.toggle()<CR>
'''
