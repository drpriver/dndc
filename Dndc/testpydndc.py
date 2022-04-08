from unittest import TestCase, main, TextTestRunner
import argparse
import sys
import os
import textwrap
from typing import Optional, List, TYPE_CHECKING, TextIO
if TYPE_CHECKING:
    import pydndc
else:
    pydndc = None

HEADER ="""
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=yes">
""".lstrip()
FOOTER = "</body>\n</html>\n"

def dedent(text):
    return textwrap.dedent(text[1:])


# unittest is kinda crappy due to it imitating JUnit,
# but I don't want a third-party dependency for testing.

class TestHtmlGen(TestCase):
    def testone(self) -> None:
        input = (
            "::md\n"
            "   * Hello World\n"
            "   * This is amazing!\n"
            "::js\n"
            "  ctx.root.add_child('hello');\n"
        )
        expected = (
            HEADER
            +dedent('''
            <title>This</title>
            </head>
            <body>
            <div>
            <ul>
            <li>
            Hello World
            </li>
            <li>
            This is amazing!
            </li>
            </ul>
            </div>
            hello
            ''')
            + FOOTER
        )
        output = pydndc.htmlgen(input)[0]
        self.assertEqual(output, expected)

    def testfragment(self) -> None:
        input = (
        "::img #noinline\n"
        "   SomeImg.png\n"
        "   width=600\n"
        "   height = 800\n"
        "   alt = \"Hello World!\"\n"
        )
        expected = (
        "<div>\n"
        "<img src=\"SomeImg.png\" width=\"600\" height=\"800\" alt=\"&quot;Hello World!&quot;\">\n"
        "</div>\n"
        )
        output = pydndc.htmlgen(input, flags=pydndc.Flags.FRAGMENT_ONLY)[0]
        self.assertEqual(output, expected)
    def testfragment2(self) -> None:
        input = (
        "Hello::title\n"
        "::md\n"
        "   * Hello World\n"
        "   * This is amazing!\n"
        "::js\n"
        "  ctx.root.add_child('hello')\n"
        "::css\n"
        "  p { color: blue;}\n"
        )
        expected = (
        "<style>\n"
        "p { color: blue;}\n"
        "</style>\n"
        "<h1 id=\"hello\">Hello</h1>\n"
        "<div>\n"
        "<ul>\n"
        "<li>\n"
        "Hello World\n"
        "</li>\n"
        "<li>\n"
        "This is amazing!\n"
        "</li>\n"
        "</ul>\n"
        "</div>\n"
        "hello\n"
        )
        output = pydndc.htmlgen(input, flags=pydndc.Flags.FRAGMENT_ONLY)[0]
        self.assertEqual(output, expected)
    def test_multiline_table(self) -> None:
        input = (
        "::table\n"
        "  d8|thing\n"
        "  1|this is a multiline\n"
        "    table\n"
        "  2| This is a singleline cell\n"
        "  3| This\n"
        "     is\n"
        "     another\n"
        "     multiline\n"
        "     table\n"
        )
        expected = (
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">\n"
        "<title>This</title>\n"
        "</head>\n"
        "<body>\n"
        "<div>\n"
        "<table>\n"
        "<thead>\n"
        "<tr>\n"
        "<th>d8\n"
        "</th>\n"
        "<th>thing\n"
        "</th>\n"
        "</tr>\n"
        "</thead>\n"
        "<tbody>\n"
        "<tr>\n"
        "<td>1\n"
        "</td>\n"
        "<td>this is a multiline\n"
        "table\n"
        "</td>\n"
        "</tr>\n"
        "<tr>\n"
        "<td>2\n"
        "</td>\n"
        "<td>This is a singleline cell\n"
        "</td>\n"
        "</tr>\n"
        "<tr>\n"
        "<td>3\n"
        "</td>\n"
        "<td>This\n"
        "is\n"
        "another\n"
        "multiline\n"
        "table\n"
        "</td>\n"
        "</tr>\n"
        "</tbody></table>\n"
        "</div>\n"
        "</body>\n"
        "</html>\n"
        )
        output = pydndc.htmlgen(input)[0]
        self.assertEqual(output, expected)

class TestReformat(TestCase):
    def test_format_table(self) -> None:
        input = (
        "::table\n"
        "  d8|thing\n"
        "  1|this is a multiline\n"
        "    table\n"
        "  2| This is a singleline cell\n"
        "  3| This\n"
        "     is\n"
        "     another\n"
        "     multiline\n"
        "     table\n"
        "  4 | This is a really long text table. As you can see, it is much longer than it really needs to be. But whatever. Long things are long. Long live the long thing! So why not. Be long!\n"
        )
        expected = (
        "::table\n"
        "  d8 | thing\n"
        "  1  | this is a multiline table\n"
        "  2  | This is a singleline cell\n"
        "  3  | This is another multiline table\n"
        "  4  | This is a really long text table. As you can see, it is much longer than\n"
        "       it really needs to be. But whatever. Long things are long. Long live the\n"
        "       long thing! So why not. Be long!\n"
        )
        output = pydndc.reformat(input)
        self.assertEqual(output, expected)

class TestExamples(TestCase):
    def test_no_errors(self) -> None:
        examples = [
        "Examples/Calendar/calendar.dnd",
        "Examples/KrugsBasement/krugs-basement.dnd",
        "Examples/Rules/characters.dnd",
        "Examples/Rules/index.dnd",
        "Examples/Rules/mechanics.dnd",
        "Examples/Rules/religion.dnd",
        "Examples/Rules/rules.dnd",
        "Examples/Wiki/Inner/hello.dnd",
        "Examples/Wiki/flat.dnd",
        "Examples/Wiki/index.dnd",
        "Examples/Wiki/lorem.dnd",
        "Examples/Wiki/wiki.dnd",
        "Examples/index.dnd",
        "Documentation/OVERVIEW.dnd",
        "Documentation/REFERENCE.dnd",
        "PyGdndc/jsdoc.dnd",
        "PyGdndc/changelog.dnd",
        "PyGdndc/Manual.dnd",
        ]
        for example in examples:
            with open(example, 'r', encoding='utf-8') as fp:
                text = fp.read()
            _ = pydndc.htmlgen(text, base_dir=os.path.dirname(example))
            if 'calendar.dnd' in example: continue
            if 'OVERVIEW' in example: continue
            _ = pydndc.expand(text, base_dir=os.path.dirname(example), error_reporter=lambda *args:(print(example),print(*args)))
            _ = pydndc.reformat(text)
            _ = pydndc.analyze_syntax_for_highlight(text)

class TestFileCache(TestCase):
    def test_contents(self) -> None:
        input = (
            "::img\n"
            "  Makefile\n"
        )
        cache = pydndc.FileCache()
        output = pydndc.htmlgen(input, file_cache=cache)
        self.assertListEqual(output[1], ['Makefile'])
        self.assertListEqual(cache.paths(), ['Makefile'])
    def test_store(self) -> None:
        cache = pydndc.FileCache()
        stored = cache.store('hello', 'hi')
        self.assertEqual(stored, True)
        self.assertListEqual(cache.paths(), ['hello'])
        input = (
            "::import\n"
            "  hello\n"
        )
        expected = (
            "<p>\n"
            "hi\n"
            "</p>\n"
        )
        output = pydndc.htmlgen(input, flags=pydndc.Flags.FRAGMENT_ONLY, file_cache=cache)
        self.assertEqual(output[0], expected)
        cache.remove('hello')
        self.assertListEqual(cache.paths(), [])
        with self.assertRaises(Exception):
            output = pydndc.htmlgen(input, flags=pydndc.Flags.FRAGMENT_ONLY, file_cache=cache)
        cache.store('1', '1')
        cache.store('2', '2')
        self.assertListEqual(cache.paths(), ['1', '2'])
        cache.clear()
        self.assertListEqual(cache.paths(), [])

class TestExpand(TestCase):
    def test_dummy(self) -> None:
        input = (
        "::table\n"
        "  d8 | thing\n"
        "  1 | this is a multiline table\n"
        "  2 | This is a singleline cell\n"
        "  3 | This is another multiline table\n"
        "  4 | This is a really long text table. As you can see, it is much longer than\n"
        "      it really needs to be. But whatever. Long things are long. Long live the\n"
        "      long thing! So why not. Be long!\n"
        "\n"
        )
        output = pydndc.expand(input, error_reporter=print)
        self.assertEqual(input, output)
    def test_actual(self) -> None:
        input = (
        "::js #import\n"
        "  script\n"
        )
        cache = pydndc.FileCache()
        cache.store('script',
                "ctx.root.add_child('hello');\n"
                "console.log('hi')\n")
        def testout(kind, file, line, col, mess):
            self.assertEqual('"hi"', mess)
        expected = "hello\n"
        output = pydndc.expand(input, error_reporter=testout, file_cache=cache)
        self.assertEqual(output, expected)



class TestJsVars(TestCase):
    def test_jsargs(self) -> None:
        # Have C call our test assertion ;)
        # A bit fragile, but easiest way to smuggle data back out of js.
        def testout(kind, file, line, col, mess):
            # print(kind, file, line, col, mess)
            expected = [
                None,
                '"world"',
                '"goodbye"',
                "[1, 2, 3]",
                "1",
                "2",
                "3",
                "[object Object]",
                "3",
            ]
            self.assertEqual(expected[line], mess)
        input = (
            "::js\n"
            "  console.log(Args.hello);\n"
            "  console.log(Args.goodbye);\n"
            "  console.log(Args.data);\n"
            "  console.log(Args.data[0]);\n"
            "  console.log(Args.data[1]);\n"
            "  console.log(Args.data[2]);\n"
            "  console.log(Args.y);\n"
            "  console.log(Args['3']);\n"
        )
        d = dict(hello="world", goodbye='goodbye', data=[1,2,3], y={})
        d[3] = 3
        _ = pydndc.htmlgen(input,
                jsargs=d,
                error_reporter=testout)
        d = '{hello:"world", goodbye:"goodbye", data:[1, 2, 3], y:{}, "3":3}'
        _ = pydndc.htmlgen(input,
                jsargs=d,
                error_reporter=testout)

def mymain() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('-C', '--change-directory')
    parser.add_argument('-e', '--extension-directory')
    parser.add_argument('-t', '--tee')
    argz = sys.argv[0]
    args, remainder = parser.parse_known_args()
    argv = [argz] + remainder
    run(**vars(args), argv=argv)

class Tee:
    """
    Simple wrapper class to forward output to multiple file-like objects.
    """
    def __init__(self, *fps):
        self.fps = fps
    def write(self, text:str, /) -> int:
        for fp in self.fps:
            result = fp.write(text)
        return result
    def flush(self) -> None:
        for fp in self.fps:
            fp.flush()
    def close(self) -> None:
        for fp in self.fps:
            fp.close()

def run(
    argv:List[str],
    change_directory:Optional[str] = None,
    extension_directory:Optional[str]=None,
    tee:Optional[str]=None,
) -> None:
    global pydndc
    if change_directory:
        os.chdir(change_directory)
    if extension_directory:
        # Get the realpath as otherwise it throws a win32 error of "The parameter is incorrect" when you go to import.
        extension_directory = os.path.realpath(extension_directory)
        sys.path.insert(0, extension_directory)
    try:
        import pydndc
    except ModuleNotFoundError:
        print('Unable to find the built pydndc extension. If you have built it, then specify what directory it is in with --extension-directory', file=sys.stderr)
        sys.exit(1)
    if tee:
        with open(tee, 'w', encoding='utf-8') as fp:
            # ignore that we don't actually implement all of TextTIO
            runner = TextTestRunner(Tee(fp, sys.stderr)) # type: ignore
            main(argv=argv, testRunner=runner)
    else:
        main(argv=argv)


if __name__ == '__main__':
    mymain()
