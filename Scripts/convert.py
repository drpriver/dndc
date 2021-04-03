import argparse
import os
import re
from typing import List, Tuple

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('files', nargs='*')
    parser.add_argument('--strip_only', action='store_true')
    parser.add_argument('--extensions', default=('.h', '.c', '.hpp', '.cpp', '.d', '.m', '.py', '.pyi', '.pyw', '.dave', '.mak', 'Makefile', '.dasm', 'CMakeLists.txt'), nargs='*')
    args = parser.parse_args()
    run(**vars(args))

def run(files:List[str], extensions:List[str], strip_only:bool=False) -> None:
    if not files:
        files = find_files('.', tuple(extensions))
    for f in files:
        changed = 0
        with open(f, 'rb') as rp:
            lines = []
            for line in rp:
                line = line.decode('utf-8')
                oldline = line
                line = line.rstrip()
                if not strip_only:
                    line = convert(line)
                lines.append(line)
                changed += ((line+'\n') != oldline)
        # only rewrite files we actually are changing
        if changed:
            with open(f, 'wb') as wp:
                for line in lines:
                    wp.write(line.encode('utf8')+b'\n')
                wp.flush()

def find_files(d:str, extensions:Tuple[str, ...]) -> List[str]:
    result: List[str] = []
    if 'PythonEmbed' in d:
        return result
    for f in os.listdir(d):
        if f.startswith('.'):
            continue
        p = os.path.join(d, f)
        if os.path.isdir(p):
            result += find_files(p, extensions)
            continue
        if f.endswith(extensions):
            result.append(p)
    return result

def convert(original:str, compiled_replacements=[]) -> str:
    replacements = [
        ]
    if replacements and not compiled_replacements:
        for pattern, repl in replacements:
            comp = re.compile(pattern)
            compiled_replacements.append((comp, repl))
    for pattern, repl in compiled_replacements:
        original = re.sub(pattern, repl, original)

    return original


if __name__ == '__main__':
    main()
