import argparse
import os
import sys
from typing import Optional, Any, List

from . import pydndc

def main() -> None:
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-v', '--version', action='version', version='%(prog)s {version}'.format(version=pydndc.__version__))
    parser.add_argument('source', help='source file (.dnd file) to read from. This is not adjusted by --base-directory')
    parser.add_argument('-o', '--output', help='output_path (.html file) to write to. If not given, writes to stdout')
    parser.add_argument('-d', '--depends-path', help='Where to write a make-style dependency file')
    parser.add_argument('-C', '--base-directory', help='Paths in source files will be relative to the given directory. Note: this does not affect the source argument. Defaults to the base directory of source.')
    parser.add_argument('--fragment', action='store_true', help='Produce an html fragment instead of a full html document')
    parser.add_argument('--dont-read', action='store_true', help="Don't read any files other than the source argument")
    parser.add_argument('--dont-import', action='store_true', help="Don't resolve import blocks.")
    parser.add_argument('--no-js', action='store_true', help="Don't execute js blocks.")
    parser.add_argument('--untrusted', '--untrusted-input', action='store_true', help='The dnd file is form an untrusted source, so only perform a safe subset of the full capability of the dnd document format.')
    parser.add_argument('--args', nargs='*', help='The following arguments will be available as a javascript array of strings named "Args"')
    parser.add_argument('--jsargs', help='A json string literal that will be exposed to javascript as Args.')
    parser.add_argument('--json-file-args', help='A path to a json file that will be read and exposed to javascript as Args.')
    parser.add_argument('--dont-write', action='store_true', help="Don't write any output. Useful for testing.")
    parser.add_argument('--dont-inline', action='store_true', help='Use links instead of inlining images, scripts and styles.')
    parser.add_argument('--print-stats', action='store_true', help='Log informative statistics during execition')
    parser.add_argument('--suppress-warnings', action='store_true', help="Don't report non-fatal errors.")
    parser.add_argument('--strip-spaces', action='store_true', help='String trailing and leading whitespace from all output lines (for html output)')
    parser.add_argument('--format', action='store_true', help='Instead of rendering to html, render to .dnd with trailing spaces removed, text wrapped to 80 columns, etc. Scripts and imports are not resolved.')
    parser.add_argument('--expand', action='store_true', help='Render to .dnd, after scripts and imports are resolved. Some documents may not be representable in this way. This does not format the output.')
    parser.add_argument('--md', '--markdown', action='store_true', help='Render to markdown instead of html.')
    parser.add_argument('--allow-js-write', action='store_true')

    args = parser.parse_args()
    run(**vars(args))

def run(
        format:bool, 
        expand:bool, 
        md:bool,
        source:str, 
        output:Optional[str], 
        depends_path:Optional[str], 
        base_directory:Optional[str], 
        fragment:bool, 
        dont_read:bool, 
        dont_import:bool, 
        no_js:bool, 
        untrusted:bool, 
        args:Optional[List[str]], 
        jsargs:Optional[str], 
        json_file_args:Optional[str], 
        dont_write:bool, 
        dont_inline:bool, 
        print_stats:bool, 
        suppress_warnings:bool, 
        strip_spaces:bool,
        allow_js_write:bool=False,
    ) -> None:
    jsstuff: Any
    if jsargs is not None:
        jsstuff = jsargs
    elif args is not None:
        jsstuff = args
    elif json_file_args:
        with open(json_file_args, 'r') as fp:
            jsstuff = fp.read()
    else:
        jsstuff = None
    flags = pydndc.Flags(0)
    if dont_read:
        flags |= pydndc.Flags.DONT_READ
    if dont_import:
        flags |= pydndc.Flags.DONT_IMPORT
    if fragment:
        flags |= pydndc.Flags.FRAGMENT_ONLY
    if no_js:
        flags |= pydndc.Flags.NO_COMPILETIME_JS
    if untrusted:
        flags |= pydndc.Flags.INPUT_IS_UNTRUSTED
    if dont_inline:
        flags |= pydndc.Flags.DONT_INLINE_IMAGES
    if suppress_warnings:
        flags |= pydndc.Flags.SUPPRESS_WARNINGS
    if strip_spaces:
        flags |= pydndc.Flags.STRIP_WHITESPACE
    if allow_js_write:
        flags |= pydndc.Flags.ENABLE_JS_WRITE

    with open(source, 'r') as fp:
        source_text = fp.read()

    if not base_directory:
        base_directory = os.path.dirname(source)

    if format:
        outs = pydndc.reformat(source_text, logger=pydndc.stderr_logger)
    elif expand:
        outs = pydndc.expand(source_text, base_dir=base_directory, logger=pydndc.stderr_logger, flags=flags, jsargs=jsstuff)
    elif md:
        outs = pydndc.to_markdown(source_text, base_dir=base_directory, logger=pydndc.stderr_logger, flags=flags, jsargs=jsstuff)
    else:
        outs = pydndc.htmlgen(source_text, base_dir=base_directory, jsargs=jsstuff, logger=pydndc.stderr_logger, flags=flags)
    if dont_write:
        return
    if output:
        with open(output, 'w') as fp:
            fp.write(outs)
    else:
        sys.stdout.write(outs)
        sys.stdout.flush()


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print(e)
        sys.exit(1)
    else:
        sys.exit(0)

