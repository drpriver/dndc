# I release this file into the public domain.
# - D.
import argparse
import os
from collections import defaultdict
from typing import NamedTuple, Any, List, DefaultDict, Set, Dict, Literal, TextIO, Optional
import json
from multiprocessing import Pool
import sys

import subprocess
import re
from enum import Enum

from . import clang
from .clang import cindex
libclang_locations = [
    # Mac
    '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/',
    # this is sketch, for linux
    '/usr/lib/llvm-10/lib',
    '/usr/lib/llvm-11/lib',
    '/usr/lib/llvm-12/lib',
    # windows
    r'C:\Program Files\LLVM\bin',
    ]
for loc in libclang_locations:
    if os.path.isdir(loc):
        cindex.Config.set_library_path(loc)
        break
else:
    raise ImportError('Unable to find libclang')
CursorKind = cindex.CursorKind
Diagnostic = cindex.Diagnostic

class Kinds(str, Enum):
    type = 'type'
    func = 'func'
    glob = 'global'
    macro = 'macro'
    enum = 'enum'
    anonenum = 'anonenum'

class Ident(NamedTuple):
    kind: Kinds
    text: str
    filename: str = ''
    lineno: int = -1

    @property
    def id(self) -> str:
        result = (f'{self.kind}-{self.text}').lower().replace('_', '-')
        return result
    @property
    def re(self) -> str:
        return r'\b' + self.text.replace(' ', r'\s') + r'\b'


def clang_default_include() -> List[str]:
    if os.name == 'nt':
        # XXX: machine specific path - nonportable
        return [r'-isystemC:\Program Files\LLVM\lib\clang\11.0.0\include']
    sub = subprocess.Popen(['clang', '-v', '-x', 'c', '-'],
                           stdin=subprocess.PIPE, stderr=subprocess.PIPE)
    _, out = sub.communicate(b'')
    reg = re.compile('.*/include$')
    includes = [line.strip() for line in out.decode('utf-8').split('\n') if reg.search(line)]
    return ['-isystem' + i for i in includes]
    sysname = os.uname().sysname
    if sysname == 'Darwin':
        MACOSX_platform = [p for p in includes if 'MacOSX.platform' in p]
        if MACOSX_platform:
            return MACOSX_platform[0]
        # XXX: os specific index (maybe machine specific) index
        return includes[2]
    elif sysname == 'Linux':
        # XXX: os specific index (maybe machine specific) index
        return includes[2]
    else:
        raise NotImplementedError
def clangxx_default_include() -> List[str]:
    if os.name == 'nt':
        # XXX: machine specific path - nonportable
        return [r'C:\Program Files\LLVM\lib\clang\11.0.0\include']
    sub = subprocess.Popen(['clang++', '-v', '-x', 'c++', '-std=gnu++17', '-'],
                           stdin=subprocess.PIPE, stderr=subprocess.PIPE)
    _, out = sub.communicate(b'')
    reg = re.compile(r'^.*/include(/[a-zA-Z\.\+0-9]+)*$')
    # reg = re.compile(r'.*/include(/[a-z\.\+]*)*$')
    includes = [line.strip() for line in out.decode('utf-8').split('\n') if reg.search(line)]
    return ['-isystem' + i for i in includes]
    sysname = os.uname().sysname
    if sysname == 'Darwin':
        MACOSX_platform = [p for p in includes if 'MacOSX.platform' in p]
        if MACOSX_platform:
            return MACOSX_platform[0]
        # XXX: os specific index (maybe machine specific) index
        return includes[2]
    elif sysname == 'Linux':
        # XXX: os specific index (maybe machine specific) index
        return includes[2]
    else:
        raise NotImplementedError

CLANG_DEFAULT_INCLUDES = clang_default_include()
CLANGXX_DEFAULT_INCLUDES = clangxx_default_include()
CWD = os.getcwd()

def normpath(p:str, paths={}) -> str: # default argument used as a cache.
    result = paths.get(p)
    if result:
        return result
    if p.startswith('/'):
        paths[p] = p
        return p
    abspath = os.path.abspath(os.path.normpath(os.path.realpath(p)))
    paths[p] = abspath
    return abspath

def fix_args(args:List[str], source_file:str) -> List[str]:
    """
    libclang is nuts and calling it to parse a translation unit with certain arguments will actually
    generate files.
    So, you need to remove certain arguments from the compiler arguments.
    """
    fixed = []
    skip = False
    for a in args:
        if skip:
            skip = False
            continue
        if a in {"-c", "-MP", "-MD", "-MMD", "--fcolor-diagnostics"}:
            continue
        if a in {"-MF", "-MT", "-MQ", "-o", "--serialize-diagnostics", "-Xclang"}:
            skip = True
            continue
        if '=' in a:
            head, tail = a.split('=')
            # compiledb fucks strings that are defined as macros.
            # You can unfuck them here.
            if head in set():
                a = '{}="{}"'.format(head, tail)
        fixed.append(a)
    if 'clang' in fixed:
        fixed.remove('clang')
    if 'clang++' in fixed:
        fixed.remove('clang++')
    fixed.remove(source_file)
    return fixed

def escape(s:str) -> str:
    return s.replace('<', '&lt;').replace('>', '&gt;')

def same_path(a:str, b:str) -> bool:
    # os.path.relpath can throw on windows
    try:
        return os.path.relpath(a) == os.path.relpath(b)
    except:
        return False

HEAD='''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=yes">
<style>
* {
  box-sizing: border-box;
}
html {
  background-color: #272822;
  color: #D2D39A;
  height: 100%;
  width: 100%;
}
body {
  height: 100%;
  width: 100%;
  display: grid;
  grid-template-columns: max-content auto;
  gird-column-gap: 3em;
  margin: 0;
}
.h {
  color: #eee;
}
.param {
  color: #73AA04;
}
.keyword {
  color: #fafc8d;
}
.comment {
  color: #5AC1E5;
}
.literal {
  color: #ee5866;
}
.underline {
  color: #888;
}
a {
  color: #D2D39A;
  font-family: ui-monospace, "Cascadia Mono", Consolas, mono;
}
#toc {
  overflow-y: auto;
  height: 100%;
  font-size: 12px;
  padding-right: 2em;
  padding-left: 1em;
  padding-top: 1ex;
}
pre {
  font-family: ui-monospace, "Cascadia Mono", Consolas, mono;
  font-size: 14px;
  overflow-y: auto;
  height: 100%;
  margin: 0;
  padding-bottom: 80vh;
  padding-top: 2ex;
  padding-left: 2em;
}
.type {
  color: #87FFAF;
}
.func {
  color: #FAC185;
}
.global {
  color: #ccc;
}
.enum, .anonenum {
  color: #FFC137;
}
.macro {
  color: #c89ad2;
}
.preproc {
  color: #a89ad2;
}
a.type, a.macro, a.func, a.enum, a.anonenum, a.global, a.literal {
  text-decoration: none;
}
a.type:hover, a.macro:hover, a.func:hover, a.enum:hover, a.anonenum:hover, a.global:hover, a.literal:hover {
  text-decoration: underline;
}
</style>
</head>
<body>'''
class DocWriter:
    backticpat = re.compile(r'`(.+?)`')
    def __init__(self, idents:Dict[str, Ident], sourcefile:str) -> None:
        self.name_to_ident = idents
        self.idents = list(idents.values())
        self.lines: List = ['']
        self.prev = (1, 1)
        self.sourcefile = sourcefile
        self.include = False
        self.include_buff = []

    def do_token(self, token:cindex.Token) -> None:
        if token.location.line != self.prev[0]:
            self.include = False
            for _ in range(max(0, token.location.line-self.prev[0])):
                self.lines.append('')
                self.prev = (token.location.line, 1)
        if token.location.column != self.prev[1]:
            self.lines[-1] += ' '*(token.location.column - self.prev[1])
        self.lines[-1] += self.tagit(token)
        self.prev = self.prev[0], token.location.column + len(token.spelling)
    def href(self, ident:Ident) -> str:
        if same_path(self.sourcefile, ident.filename):
            path = ''
        else:
            path = os.path.join('..', os.path.basename(os.path.dirname(ident.filename)), os.path.basename(ident.filename))
            path += '.html'
        href = f'{path}#{ident.id}'
        return href
    def repl(self, match:re.Match)-> str:
        text = match[1]
        ident = self.name_to_ident.get(text)
        if ident is None:
            if 0: print(self.sourcefile+':', match[0], ' matches nothing', file=sys.stderr)
            return match[0]
        if ident.filename:
            href = self.href(ident)
            return f'<a class="{ident.kind}" href="{href}">{text}</a>'
        return f'<span class="{ident.kind}">{text}</span>'
    def tagit(self, token:cindex.Token) -> str:
        spell = token.spelling
        kind = token.kind
        text = escape(spell)
        if spell in {'#', 'ifdef', 'undef', 'define', 'include', 'import', 'ifndef', 'elif', 'endif', 'pragma'}:
            if spell == 'include':
                self.include = True
            return f'<span class="preproc">{text}</span>'
        if token.location.column == 2 and spell in {'else', 'if'}:
            return f'<span class="preproc">{text}</span>'
        if kind == cindex.TokenKind.COMMENT:
            if spell[2:].strip().startswith('-'):
                return f'<span class="comment">//</span><span class="underline">{spell[2:]}</span>'
            if spell[2:].strip().endswith(':'):
                if spell[2:].strip() in {'Arguments:', 'Returns:', 'Example:'}:
                    return f'<span class="comment">//</span><span class="h">{spell[2:]}</span>'
                if ' ' not in spell[2:].strip():
                    return f'<span class="comment">//</span><span class="param">{spell[2:]}</span>'
            text = re.sub(self.backticpat, self.repl, text)
            return f'<span class="comment">{text}</span>'
        ident = self.name_to_ident.get(spell)
        if ident is None:
            if kind == cindex.TokenKind.KEYWORD:
                return f'<span class="keyword">{text}</span>'
            if kind == cindex.TokenKind.LITERAL:
                if self.include:
                    href = text[1:-1]+'.html"'
                    if '/' in href:
                        href = '"../'+href
                    else:
                        href = '"' + href
                    self.include = False
                    return f'<a href={href} class="literal">{text}</a>'
                return f'<span class="literal">{text}</span>'
            if self.include:
                self.include_buff.append(text)
                if text != '&gt;':
                    return ''
                else:
                    b = ''.join(self.include_buff)
                    self.include_buff.clear()
                    self.include = False
                    return f'<span class="literal">{b}</span>'
            return text
        if ident.lineno == token.location.line:
            return f'<span class="{ident.kind}" id="{ident.id}">{text}</span>'
        if ident.filename:
            href = self.href(ident)
            return f'<a class="{ident.kind}" href="{href}">{text}</a>'
        return f'<span class="{ident.kind}">{text}</span>'
    def _write_head(self, outfp:TextIO) -> None:
        print(HEAD, file=outfp)
        print('<div id="toc">', file=outfp)
        print('<a href="cdocindex.html">Index</a>', file=outfp)
        print('<ul>', file=outfp)
        doing_enum = False
        for ident in sorted([i for i in self.idents if same_path(i.filename, self.sourcefile)], key=lambda x: x.lineno):
            if ident.kind == Kinds.enum:
                if not doing_enum:
                    print('  <ul>', file=outfp)
                    doing_enum = True
                print(f'  <li><a class="{ident.kind}" href="#{ident.id}">{ident.text}</a></li>', file=outfp)
            else:
                if doing_enum:
                    print('  </ul>', file=outfp)
                    doing_enum = False
                print(f'<li><a class="{ident.kind}" href="#{ident.id}">{ident.text}</a></li>', file=outfp)
        print('</ul>', file=outfp)
        print('</div>', file=outfp)
        print('<pre>', file=outfp)
    def _write_tail(self, outfp) -> None:
        print('</pre>', file=outfp)
        print('</body>', file=outfp)
        print('</html>', file=outfp)
    def write(self, outfp:TextIO) -> None:
        self._write_head(outfp)
        for line in self.lines:
            print(line, file=outfp)
        self._write_tail(outfp)
        outfp.flush()
    def write_deps(self, outname:str, outfp:TextIO) -> None:
        print(f'{outname}:', file=outfp, end='')
        deps = sorted(set(os.path.relpath(i.filename).replace(' ', r'\ ') for i in self.idents if i.filename))
        for d in deps:
            print(d, '', end='', file=outfp)
        print('', file=outfp)
        for d in deps:
            print(d+':', file=outfp)

def do_tags(arguments:List[str], source_file:str, compiler:str) -> DocWriter:
    identifiers = {} # type: Dict[str, Ident]
    def t(s:str) -> None:
        identifiers[s] = Ident(Kinds.type, s)
    def f(s:str) -> None:
        identifiers[s] = Ident(Kinds.func, s)
    for x in ['int', 'char', 'size_t', 'unsigned', 'void', 'ssize_t', 'long', 'short', 'enum',
            'signed', 'struct', 'union', 'const', 'FILE', 'int8_t', 'uint8_t', 'int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'bool', '_Bool', 'float', 'double', 'static', 'inline', 'ptrdiff_t', 'intptr_t', 'uintptr_t']:
        t(x)
    for x in ['printf', 'fprintf', 'memcpy', 'memset', 'memcmp', 'memchr', 'memmem']:
        f(x)
    clang_args=(CLANG_DEFAULT_INCLUDES if compiler == 'clang' else CLANGXX_DEFAULT_INCLUDES)+arguments
    try:
        tu = cindex.TranslationUnit.from_source(
                os.path.abspath(source_file),
                args=clang_args,
                options=cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD,
            )
    except Exception as e:
        raise Exception(str(e) + f' source_file: {source_file}')
    errors = [d for d in tu.diagnostics if d.severity in (Diagnostic.Error, Diagnostic.Fatal)]
    if errors:
        raise Exception("File '{}' failed clang's parsing and type-checking:\n {}\n\nargs was {}".format(tu.spelling, '\n'.join(['{}:{}:{}: {}'.format(d.location.file, d.location.line, d.location.column, d.spelling) for d in errors]), clang_args))

    source_files = [normpath(tu.spelling)]

    for c in tu.cursor.get_children():
        do_cursor(c, source_files, identifiers)
    writer = DocWriter(identifiers, source_file)
    prev = (1, 1)
    for token in tu.get_tokens(locations=(cindex.SourceLocation.from_offset(tu, tu.get_file(source_file),0), cindex.SourceLocation.from_offset(tu, tu.get_file(source_file), len(open(source_file).read())))):
        writer.do_token(token)
    return writer

def do_cursor(cursor, source_files:List[str], identifiers:dict) -> None:
    if not should_tag_file(cursor.location.file, source_files):
        return

    if good_tag(cursor):
        kind = {
                CursorKind.ENUM_DECL          : Kinds.type,
                CursorKind.UNION_DECL         : Kinds.type,
                CursorKind.TYPEDEF_DECL       : Kinds.type,
                CursorKind.STRUCT_DECL        : Kinds.type,
                CursorKind.FUNCTION_DECL      : Kinds.func,
                CursorKind.ENUM_CONSTANT_DECL : Kinds.enum,
                CursorKind.VAR_DECL           : Kinds.glob,
                CursorKind.MACRO_DEFINITION   : Kinds.macro,
                CursorKind.OBJC_INTERFACE_DECL: Kinds.type,
            }[cursor.kind]
        if cursor.kind == CursorKind.ENUM_CONSTANT_DECL:
            if not cursor.semantic_parent.spelling:
                kind = Kinds.anonenum

        identifiers[cursor.spelling] = Ident(kind, cursor.spelling, normpath(cursor.location.file.name), cursor.location.line)

    if should_tag_children(cursor):
        for c in cursor.get_children():
            do_cursor(c, source_files, identifiers)

def good_tag(cursor) -> bool:
    spelling = cursor.spelling
    if not spelling:
        return False
    if cursor.kind != CursorKind.VAR_DECL and not is_definition(cursor):
        return False
    if cursor.kind == CursorKind.TYPEDEF_DECL:
        if spelling == cursor.underlying_typedef_type.spelling:
            return False
    if cursor.kind == CursorKind.FIELD_DECL:
        return False
    if cursor.kind == CursorKind.VAR_DECL:
        if cursor.semantic_parent.kind != CursorKind.TRANSLATION_UNIT:
            return False
    if len(spelling) < 3 and spelling not in {'SV', 'LS', 'SS', 'MS', 'TS'}:
        return False
    if spelling[0] == '_':
        return False
    if spelling[-1] == '_':
        return False
    if spelling.endswith('_H'):
        return False
    if '__' in spelling:
        return False
    return True


def is_definition(cursor):
    #TODO: examine this more closely
    return (
        (cursor.is_definition() and not cursor.kind in [
            CursorKind.CXX_ACCESS_SPEC_DECL,
            CursorKind.TEMPLATE_TYPE_PARAMETER,
            CursorKind.UNEXPOSED_DECL,
            ]) or
        cursor.kind in [
            CursorKind.FUNCTION_DECL,
            CursorKind.CXX_METHOD,
            CursorKind.FUNCTION_TEMPLATE,
            CursorKind.MACRO_DEFINITION,
            CursorKind.OBJC_INTERFACE_DECL
            ])

def should_tag_file(f, source_files:List[str]) -> bool:
    if not f:
        return False
    name = normpath(f.name)
    if '..' in os.path.relpath(name): return False
    return True

def should_tag_children(cursor) -> bool:
    return cursor.kind.value in {
        CursorKind.NAMESPACE.value,
        CursorKind.STRUCT_DECL.value,
        CursorKind.UNION_DECL.value,
        CursorKind.ENUM_DECL.value,
        CursorKind.CLASS_DECL.value,
        CursorKind.CLASS_TEMPLATE.value,
        CursorKind.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION.value,
        CursorKind.UNEXPOSED_DECL.value,
    }

def run(file:str, doc_folder:Optional[str], dep_file:Optional[str], cflags:List[str]) -> None:
    file = os.path.relpath(file)
    args = ['-x', 'c', file]
    if cflags:
        args.extend(cflags)
    args = fix_args(args, file)
    writer = do_tags(args, file, 'clang')
    if not doc_folder:
        writer.write(sys.stdout)
        return
    p = os.path.join(doc_folder, file) + '.html'
    d = os.path.dirname(p)
    os.makedirs(d, exist_ok=True)
    with open(p, 'w') as fp:
        writer.write(fp)
    if dep_file is not None:
        parts = dep_file.split('/')
        dep_file = parts[0] + '/' + '_'.join(parts[1:])
        print(dep_file)
        with open(dep_file, 'w') as fp:
            writer.write_deps(p, fp)

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('file')
    parser.add_argument('-o', '--doc-folder')
    parser.add_argument('-d', '--dep-file')
    parser.add_argument('--cflags', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    run(**vars(args))

if __name__ == '__main__':
    main()
