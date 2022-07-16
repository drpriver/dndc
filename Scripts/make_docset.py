from __future__ import annotations
import argparse
import shutil
import os
import sqlite3
import re

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('files', nargs='+')
    parser.add_argument('-o', '--outdir', required=True)
    args = parser.parse_args()
    run(**vars(args))

def run(files:list[str], outdir:str) -> None:
    shutil.rmtree(outdir, ignore_errors=True)
    contents = os.path.join(outdir, 'Contents')
    resources = os.path.join(contents, 'Resources')
    documents = os.path.join(resources, 'Documents')
    os.makedirs(documents)
    plist = '''
    <?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0">
    <dict>
        <key>CFBundleIdentifier</key>
        <string>dndc</string>
        <key>CFBundleName</key>
        <string>Dndc</string>
        <key>DocSetPlatformFamily</key>
        <string>dndc</string>
        <key>isDashDocset</key>
        <true/>
    </dict>
    </plist>
    '''
    with open(os.path.join(contents, 'Info.plist'), 'w', newline='', encoding='utf-8') as fp:
        fp.write(plist)
    dbname = os.path.join(resources, 'docSet.dsidx')
    conn = sqlite3.connect(dbname)
    with conn:
        conn.execute("PRAGMA encoding = 'UTF-8';")
        conn.execute('CREATE TABLE searchIndex(id INTEGER PRIMARY KEY, name TEXT, type TEXT, path TEXT);')
        conn.execute('CREATE UNIQUE INDEX anchor ON searchIndex (name, type, path);')

    pat = re.compile(r'<li>\s*<a(\sclass="(.*?)")?\s.*?href="#(.*?)".*?>(.*?)</a>', flags=re.MULTILINE)
    relpat = re.compile(r'href="(\.\..*?)"')
    for file in files:
        with open(file) as fp:
            text = fp.read()
        for match in re.finditer(pat, text):
            cls = match[2]
            href = match[3]
            name = match[4]
            if not cls:
                cls = 'func'
            cls = {
                'func':'Function',
                'type':'Type',
            }.get(cls)
            if not cls: continue
            with conn:
                conn.execute('INSERT OR IGNORE INTO searchIndex(name, type, path) VALUES (?,?,?)', (name, cls, f'{os.path.basename(file)}#{href}'))
        def remove_rel(m:re.Match) -> str:
            inner = os.path.basename(m[1])
            return f'href="{inner}"'
        text = re.sub(relpat, remove_rel, text)
        text = text.replace('<pre>', '<div class="pre">')
        text = text.replace('</pre>', '</div>')
        text = text.replace('pre {', 'div.pre {')

        text = text.replace('</html>', '''
        <style>
        div.pre {
            white-space: pre;
        }
        html {
            -webkit-filter: none !important;
            filter: none !important;
        }
        </style>
        </html>''')
        with open(os.path.join(documents, os.path.basename(file)), 'w', encoding='utf-8', newline='') as fp:
            fp.write(text)
    
    conn.close()


if __name__ == '__main__':
    main()
