import argparse
import os
from collections import defaultdict
from typing import NamedTuple, Any, List, DefaultDict, Set, Dict, Literal
import json
from multiprocessing import Pool

import subprocess
import re

from . import clang
from .clang import cindex
libclang_locations = [
    '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/',
    '/usr/lib/llvm-10/lib',
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

class IdentInfo(NamedTuple):
    line     : str
    name     : str
    location : int
    offset   : int
    kind     : Any
    filename : str

    def as_tag(self) -> str:
        try:
            fn = os.path.relpath(self.filename)
        except ValueError: # this can throw on windows if drives differ
            fn = self.filename
        if '..' in fn:
            fn = self.filename
        line = self.line.replace('\\', '\\\\')
        return '{}\t{}\t/^{}$'.format(self.name, fn, line)

class Identer:
    tags  : List[str]
    funcs : List[str]
    enums : List[str]
    types : List[str]
    globs : List[str]
    macros: List[str]
    def __init__(self) -> None:
        self.infos: DefaultDict[str, Set[IdentInfo]] = defaultdict(set)
        self.file_lines: Dict[str, List[str]] = dict()

    def tag(self, cursor) -> None:
        filename = normpath(cursor.location.file.name)
        if filename not in self.file_lines:
            with open(filename) as f:
                self.file_lines[filename] = f.readlines()
        t = self.infos[filename]
        line = self.file_lines[filename][cursor.location.line-1].rstrip()
        info = IdentInfo(line=line, name=cursor.spelling, location=cursor.location.line, offset=cursor.location.offset, kind=cursor.kind, filename=filename)
        t.add(info)

    def analyze(self) -> None:
        things = ['tags', 'funcs', 'enums', 'types', 'globs', 'macros']
        for thing in things:
            setattr(self, thing, [])
        self.tags: List[str] # note that this is a declaration
        typekinds = {
                CursorKind.ENUM_DECL          : 'types',
                CursorKind.UNION_DECL         : 'types',
                CursorKind.TYPEDEF_DECL       : 'types',
                CursorKind.STRUCT_DECL        : 'types',
                CursorKind.FUNCTION_DECL      : 'funcs',
                CursorKind.ENUM_CONSTANT_DECL : 'enums',
                CursorKind.VAR_DECL           : 'globs',
                CursorKind.MACRO_DEFINITION   : 'macros',
                CursorKind.OBJC_INTERFACE_DECL: 'types',
                }
        for tags in self.infos.values():
            for t in tags:
                self.tags.append(t.as_tag())
                which = typekinds.get(t.kind)
                if not which:
                    continue
                getattr(self, which).append(t.name)
        for thing in things:
            th: List = getattr(self, thing)
            th = list(set(th))
            th.sort()
            setattr(self, thing, th)

def clang_default_include() -> str:
    if os.name == 'nt':
        # XXX: machine specific path - nonportable
        return r'C:\Program Files\LLVM\lib\clang\11.0.0\include'
    sub = subprocess.Popen(['clang', '-v', '-x', 'c', '-'],
                           stdin=subprocess.PIPE, stderr=subprocess.PIPE)
    _, out = sub.communicate(b'')
    reg = re.compile('.*/include$')
    includes = [line.strip() for line in out.decode('utf-8').split('\n') if reg.search(line)]
    sysname = os.uname().sysname
    if sysname == 'Darwin':
        # XXX: os specific index (maybe machine specific) index
        return includes[2]
    elif sysname == 'Linux':
        # XXX: os specific index (maybe machine specific) index
        return includes[2]
    else:
        raise NotImplementedError

CLANG_DEFAULT_INCLUDES = clang_default_include()
CWD = os.getcwd()

def normpath(p:str, paths={}) -> str:
    result = paths.get(p)
    if result:
        return result
    if p.startswith('/'):
        paths[p] = p
        return p
    abspath = os.path.abspath(os.path.normpath(os.path.realpath(p)))
    paths[p] = abspath
    return abspath

def fix_args(args:List[str], source_file) -> List[str]:
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
            # compiledb fucks strings that are defined as macros
            if head in {'-DBENCHMARKINPUTPATH', '-DBENCHMARKDIRECTORY', '-DBENCHMARKOUTPUTPATH',}:
                a = '{}="{}"'.format(head, tail)
        fixed.append(a)
    fixed.remove('clang')
    fixed.remove(source_file)
    return fixed

def do_tags(arguments:List[str], identer:Identer, source_file:str) -> None:
    clang_args=['-isystem'+CLANG_DEFAULT_INCLUDES]+arguments
    tu = cindex.TranslationUnit.from_source(
            os.path.abspath(source_file),
            args=clang_args,
            options=cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD,
            )
    errors = [d for d in tu.diagnostics if d.severity in (Diagnostic.Error, Diagnostic.Fatal)]
    if errors:
        raise Exception("File '{}' failed clang's parsing and type-checking:\n {}\n\nargs was {}".format(tu.spelling, '\n'.join(['{}:{}:{}: {}'.format(d.location.file, d.location.line, d.location.column, d.spelling) for d in errors]), clang_args))

    source_files = [normpath(tu.spelling)]

    for c in tu.cursor.get_children():
        do_cursor(c, identer, source_files)

def do_cursor(cursor, identer:Identer, source_files:List[str]) -> None:
    if not should_tag_file(cursor.location.file, source_files):
        return

    if good_tag(cursor):
        identer.tag(cursor)

    if should_tag_children(cursor):
        for c in cursor.get_children():
            do_cursor(c, identer, source_files)

def good_tag(cursor) -> bool:
    spelling = cursor.spelling
    if not spelling:
        return False
    if not is_definition(cursor):
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

def should_exclude(name:str, excludes={}) -> bool:
    result = excludes.get(name)
    if result is not None:
        return result

    if 'System' in name:
        excludes[name] = True
        return True
    # if 'DCraw' not in name and 'SDL' not in name:# and 'python' not in name:
        # excludes[name] = True
        # return True
    excludes[name] = False
    return False

def should_tag_file(f:str, source_files:List[str]) -> bool:
    if not f:
        return False
    name = normpath(f.name)
    if should_exclude(name):
        return False
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

def write_tags(tags:List[str]) -> None:
    with open('tags', 'w') as f:
        print('!_TAG_FILE_FORMAT\t1\t/basic format; no extension fields/', file=f)
        print('!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/', file=f)
        for t in tags:
            print(t, file=f)

def add_extra(filename:str, stuff:List[str]) -> None:
    with open(filename, 'r') as fp:
        for l in fp:
            l = l.strip()
            if not l:
                continue
            stuff.append(l)

def get_proj_dirs() -> List[str]:
    EXCLUDED = {'Bin', 'Objs', 'Depends', 'venv', 'vendored', 'TestCases', 'frozen', 'Release'}
    dirs = [d for d in os.listdir('.') if os.path.isdir(d) and d not in EXCLUDED and not d.startswith(('.', '_')) and not d.lower().startswith('build') and '.app' not in d]
    subdirs = []
    for d in dirs:
        for subdir in os.listdir(d):
            if subdir in EXCLUDED:
                continue
            if subdir.startswith(('.', '_')):
                continue
            subdir = os.path.join(d, subdir)
            if os.path.isdir(subdir):
                subdirs.append(subdir)
    dirs = dirs + subdirs
    return dirs

def write_vim(funcs:List[str], enums:List[str], types:List[str], globs:List[str], macros:List[str]) -> None:
    # remove some special macros
    macros_ = set(macros)
    macros_.difference_update([
        'auto',
        'bool', 'true', 'false', 'YES', 'NO',
        'Nonnull', 'Nullable', 'NullUnspec',
        'or', 'and', 'not',
        'Raise', 'attempt', 'unwrap',
        ])
    macros_.difference_update(funcs)
    macros_.difference_update(enums)
    macros_.difference_update(types)
    macros_.difference_update(globs)
    macros = sorted(macros_)
    dirs = get_proj_dirs()
    with open('.vimrc', 'w') as fp:
        fp.write('set tags+=dtags\n')
        fp.write('set grepprg=git\ --no-pager\ grep\ --no-color\ -n\ $*\n')
        fp.write('set grepformat=%f:%l:%m,%m\ %f\ match%ts,%f\n')
        fp.write('set path=.,,{}\n'.format(','.join(dirs)))
        fp.write('hi Error ctermfg=none ctermbg=none guifg=fg guibg=bg\n')
        fp.write('hi cConstant ctermfg=2 guifg=DarkGreen\n')
        fp.write('hi Constant ctermfg=2 guifg=DarkGreen\n')
        fp.write('hi cFunction ctermfg=109 guifg=#664499\n')
        fp.write('hi cEnum cterm=bold gui=bold\n')
        fp.write('hi cPreProc ctermfg=5\n')
        fp.write('au BufEnter *.c,*.h,*.m,*.mm syn keyword cFunction ')
        fp.write(' '.join(funcs))
        fp.write('\n')
        fp.write('\n')
        fp.write('au BufEnter *.c,*.h,*.m,*.mm syn keyword cType ')
        fp.write(' '.join(types))
        fp.write('\n')
        fp.write('\n')
        fp.write('au BufEnter *.c,*.h,*.m,*.mm syn keyword cEnum ')
        fp.write(' '.join(enums))
        fp.write('\n')
        fp.write('\n')
        fp.write('au BufEnter *.c,*.h,*.m,*.mm syn keyword cPreProc ')
        fp.write(' '.join(macros))
        fp.write('\n')
        fp.write('\n')
        fp.flush()


class IdenterResult(NamedTuple):
    tags  : List[str]
    funcs : List[str]
    enums : List[str]
    types : List[str]
    globs : List[str]
    macros: List[str]

def merge_identers(identers:List[Identer]) -> IdenterResult:
    merged:Dict[str, List[str]] = {
        'tags'  : [],
        'funcs' : [],
        'enums' : [],
        'types' : [],
        'globs' : [],
        'macros': [],
        }
    for k, v in merged.items():
        for identer in identers:
            v.extend(getattr(identer, k))
        merged[k] = sorted(set(v))
    return IdenterResult(**merged)

def do_command(args) -> Identer:
    identer = Identer()
    arguments, f = args
    do_tags(arguments, identer, f)
    identer.analyze()
    return identer

def run(pool_size:int) -> None:
    compile_commands = json.load(open('compile_commands.json'))

    pool_arguments = []
    for command in compile_commands:
        if 'Test' in command['file']:
            continue
        f = command['file']
        arguments = fix_args(command['arguments'], f)
        pool_arguments.append((arguments, f))

    if pool_size:
        with Pool(pool_size) as p:
            identers = p.map(do_command, pool_arguments)
    else:
        identers = list(map(do_command, pool_arguments))

    result = merge_identers(identers)

    write_tags(result.tags)
    write_vim(result.funcs, result.enums, result.types, result.globs, result.macros)

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--pool_size', '-p', type=int, default=12)
    args = parser.parse_args()
    run(**vars(args))

if __name__ == '__main__':
    main()
