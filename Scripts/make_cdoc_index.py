import argparse
import glob
import os
from typing import List

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('docdir')
    args = parser.parse_args()
    run(**vars(args))

def run(docdir:str) -> None:
    all_htmls = [] # type: List[str]
    for p in sorted(os.listdir(docdir)):
        if not os.path.isdir(os.path.join(docdir, p)): continue
        htmls = glob.glob(os.path.join(docdir, p, '*'))
        if not htmls: continue
        htmls = [x for x in htmls if 'cdoc' not in x]
        htmls.sort()
        write_index(htmls, os.path.join(docdir, p), True)
        htmls.insert(0, os.path.join(docdir, p, 'cdocindex.html'))
        all_htmls.extend(htmls)
    write_index(all_htmls, docdir)

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
a {
  color: #D2D39A;
  font-family: ui-monospace, "Cascadia Mono", Consolas, mono;
}
#directory {
    margin: auto;
    width: max-content;
}
</style>
</head>
<body>
<ul id="directory">
'''
TAIL = '''
</ul>
</body>
</html>
'''

def write_index(htmls:List[str], dir:str, up=False) -> None:
    index = os.path.join(dir, 'cdocindex.html')
    with open(index, 'w') as fp:
        print(HEAD, file=fp)
        if up:
            print(f'<li><a href="../cdocindex.html">Up One Level</a></li>', file=fp)
        for h in htmls:
            p = os.path.relpath(h, dir)
            print(f'<li><a href="{p}">{p[:-5]}</a></li>', file=fp)
        print(TAIL, file=fp)

if __name__ == '__main__':
    main()
