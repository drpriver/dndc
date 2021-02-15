"""
I gave up on finding the windows equivelants to basic unix
commands, so instead I just implement them in a python script.
"""
import argparse
import os
import shutil
import glob
import sys
from typing import List, Any

def expand(paths: List[str]) -> List[str]:
    real_paths = []
    for p in paths:
        if '*' in p:
            real_paths.extend(glob.glob(p))
        else:
            real_paths.append(p)
    return real_paths

def touch(files: List[str]) -> None:
    files = expand(files)
    for f in files:
        if os.path.exists(f):
            os.utime(f)
        else:
            open(f, 'w').close()

def rm(files_or_d:List[str], r:bool, f:bool) -> None:
    files_or_d = expand(files_or_d)
    for p in files_or_d:
        if os.path.isdir(p):
            if r:
                shutil.rmtree(p)
            elif f:
                pass
            else:
                raise ValueError(repr(p)+' is a directory')
        elif os.path.isfile(p):
            os.unlink(p)
        elif f:
            pass
        else:
            raise ValueError(repr(p) + ' does not exist')


def mkdir(dirs:List[str], p:bool) -> None:
    dirs = expand(dirs)
    for d in dirs:
        os.makedirs(d, exist_ok=p)

def mv(src: str, dst:str) -> None:
    shutil.move(src, dst)

def cp(src_dsts:List[str]) -> None:
    src_dsts = expand(src_dsts)
    dst = src_dsts[-1]
    srcs = src_dsts[:-1]
    for src in srcs:
        shutil.copy(src, dst)

def main() -> None:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    touch_parser = subparsers.add_parser('touch')
    touch_parser.add_argument('files', nargs='+')
    touch_parser.set_defaults(func=touch)

    rm_parser = subparsers.add_parser('rm')
    rm_parser.add_argument('files_or_d', nargs='+')
    rm_parser.add_argument('-r', action='store_true')
    rm_parser.add_argument('-f', action='store_true')
    rm_parser.set_defaults(func=rm)

    mkdir_parser = subparsers.add_parser('mkdir')
    mkdir_parser.add_argument('dirs', nargs='+')
    mkdir_parser.add_argument('-p', action='store_true')
    mkdir_parser.set_defaults(func=mkdir)

    mv_parser = subparsers.add_parser('mv')
    mv_parser.add_argument('src')
    mv_parser.add_argument('dst')
    mv_parser.set_defaults(func=mv)

    cp_parser = subparsers.add_parser('cp')
    cp_parser.add_argument('src_dsts', nargs='+')
    cp_parser.set_defaults(func=cp)

    args = parser.parse_args()
    func = args.func
    del args.func
    try:
        func(**vars(args))
    except Exception as e:
        print(e, file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
