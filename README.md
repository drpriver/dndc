<!-- This md file was generated from a dnd file. -->
<h2>Table of Contents</h2>

<h1>Dndc</h1>

See [the overview](Documentation/OVERVIEW.dnd), written in `.dnd` for a description of the file
format. Or checkout [the reference](Documentation/REFERENCE.dnd).



This is a compiler and associated tooling for the dnd document format. DND
stands for David's New Document.

<h2>Building</h2>
This project builds with clang (including apple clang) or gcc. It does not
compile with MSVC. MSVC has some weird bug with static constant initializers
that only manifests in release builds (strangely, debug builds do compile).
You can just use clang on windows anyway. Mingw is not supported.

<details><summary>Make</summary>

Building with the `Makefile` requires gnu make. The first time
you invoke `make`, it will create a makefile called
`opt.mak`. `opt.mak` allows you to configure things
like debug vs release build, the right compile flags for the python
extension if on windows and whether to build the qt-based gui editor.


You can invoke `make opt.mak` yourself to not use the default
options before the first build.


To compile everything + tests, do `make all`.


To compile and run the tests, do `make tests`.


To compile the documentation, do `make docs` and for C docs do
`make cdocs`.


You can list the make targets with `make list`.

</details>

<details><summary>Meson</summary>

You can also compile everything using meson. Meson currently doesn't build
the docs or dndc.wasm.

</details>

<details><summary>Cmake</summary>

Cmake is also supported. Cmake currently doesn't build the docs or
dndc.wasm.

</details>

<details><summary>setuptools</summary>

If you just want the python extension, you can build using the
`setup.py` script. Builds with gcc or clang.

</details>


<h2>Vendored Libraries</h2><dl>
<dt>quickjs</dt>
<dd>

The dnd document format allows for in-document scripting to manipulate the
document's ast using javascript. To support this, we include a vendored copy
of [quickjs](https://bellard.org/quickjs/), with some small modifications.


It is a future project to see if we can use system javascript engines
instead to reduce the filesize footprint of dndc. For example, javascript
core on macos.


</dd>

</dl>

<h2>Build Artifacts</h2><dl>
<dt>dndc</dt>
<dd>

The command-line, batch program that can compile dnd files into html, md or
to dnd. It can also be used to format the dnd file to a standard width.

</dd>

<dt>libdndc</dt>
<dd>

Both static and dynamic versions are supported. libquickjs is also built and
is required for linking. You can either transforms strings in a batch mode,
similar to the interface of [dndc](#dndc), or you can use the slightly more
low-level ast api to manipulate a dnd document.

</dd>

<dt>dndc.wasm</dt>
<dd>

A stripped down version of [libdndc](#libdndc) is available as a wasm target. Some of
the output formats are not available and in-document scripting with quickjs
is not enabled.


Compiling to wasm requires compiler support. Apple clang does not support
this, but other versions of clang do.

</dd>

<dt>pydndc</dt>
<dd>

[libdndc](#libdndc), but as a python extension. Exposes a pythonic api to libdndc.
This will embed quickjs in the python extension.

</dd>

<dt>DndEdit</dt>
<dd>

Mac only. A rich gui editor for dnd files, showing the resulting document in
a webview. Built with appkit.


This does not produce an app bundle. It creates a self-contained executable
with an embedded Info.plist and app icon. Patches to create an actual app
bundle instead are welcome.

</dd>

<dt>DndcEdit</dt>
<dd>

Qt 5 or 6 required. Also a rich gui editor for dnd files, like DndEdit. This
is built with Qt instead.

</dd>

<dt>PyDndEdit</dt>
<dd>

A python version of DndcEdit, using pydndc and PySide6's bindings to Qt.

</dd>

<dt>dndc-tag</dt>
<dd>

A command-line utility that will create tags from a list of files or the
current directory. These tags are formatted so vim and similar editors can
use them to jump around.

</dd>

<dt>dndc-browse</dt>
<dd>

A command-line utility that will start a local webserver that will serve dnd
files in the current directory as html. All documents are compiled upon
request, so you can edit documents and then immediately see the new document
in your browser.

</dd>

<dt>dndbr</dt>
<dd>

Mac Only. This is similar to dndc-browse, but is a gui app instead of a
command-line utility.


Similar to DndEdit, this does not produce an app bundle.

</dd>

</dl>

<h2>Testing</h2>
Tests are C programs that can be compiled and run individually. If you are
building with make, `make tests` will build and run all the tests.
If using meson, you can do `meson test -C <builddir>` or do
`ninja test` from the build directory. With Cmake, be aware that
`ninja test` does not build the tests, it only runs them. Cmake
just being Cmake I guess.


Once the tests are compiled, you can run them individually if you want. Run
the binary with `-h` to see a list of options. You can even run
only a subset of tests within each binary.


Some tests need their working directory to be the project root as they read
the examples or the testcases, etc. Each test has a `-C` argument
to set the current directory.


There is also a python test for the python extension, at
[Dndc/testpydndc.py](Dndc/testpydndc.py). That one needs to know where the compiled
python extension is and has an argument for that.



<h2>Type Checking</h2>
The python code should all type check with mypy. If it doesn't then that is a
bug.


<h2>Scripts</h2><dl>

There are some useful scripts in the `Scripts` folder for
development.


<dt>[tag_and_syntax.py]</dt>
<dd>

This script requires libclang and might only work on my machines. If it does
work, it will produce a tags file for vim and also a .vimrc file that will
setup semantic highlighting in vim.


This is invoked by `make tags`.

</dd>

<dt>[cdoc.py]</dt>
<dd>

This script also requires libclang. It converts C headers into a
syntax/semantic highlighted hyperlinked html page.


This and `make_cdoc_index.py` are invoked by `make
cdocs`.

</dd>

<dt>[make_cdoc_index.py]</dt>
<dd>

This script will add an index.html to all the folders generated by
`cdoc.py`.

</dd>

<dt>[win_utils.py]</dt>
<dd>

On windows the basic cli commands are different and weird. To make up for
this, this script implements some really basic ones. It implements
`rm`, `mkdir`, `mv`, `cp`, and
`install`.

</dd>

<dt>[convert.py]</dt>
<dd>

This script can be run in two modes. In either mode, it will recursively
find all interesting files if no files are given as an argument. If
`--strip_only` is passed, then it will strip trailing whitespace
from all of the files. If not, then it will do some regex replacements in
all of the files. This should probably take those as an argument, but it
doesn't, you have to edit the script.


This can be invoked by `make strip` or `make convert`.

</dd>

</dl>


<style>
@media (prefers-color-scheme: dark) {
  body {
    color-scheme: dark;
    --fg-default: #c9d1d9;
    --fg-muted: #8b949e;
    --fg-subtle: #484f58;
    --canvas-default: #0d1117;
    --canvas-subtle: #161b22;
    --border-default: #30363d;
    --border-muted: #21262d;
    --neutral-muted: rgba(110,118,129,0.4);
    --accent-fg: #58a6ff;
    --accent-emphasis: #1f6feb;
    --attention-subtle: rgba(187,128,9,0.15);
  }
}

@media (prefers-color-scheme: light) {
  body {
    color-scheme: light;
    --fg-default: #24292f;
    --fg-muted: #57606a;
    --fg-subtle: #6e7781;
    --canvas-default: #ffffff;
    --canvas-subtle: #f6f8fa;
    --border-default: #d0d7de;
    --border-muted: hsla(210,18%,87%,1);
    --neutral-muted: rgba(175,184,193,0.2);
    --accent-fg: #0969da;
    --accent-emphasis: #0969da;
    --attention-subtle: #fff8c5;
  }
}

body {
  -ms-text-size-adjust: 100%;
  -webkit-text-size-adjust: 100%;
  color: var(--fg-default);
  background-color: var(--canvas-default);
  font-family: -apple-system,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif,"Apple Color Emoji","Segoe UI Emoji";
  font-size: 16px;
  line-height: 1.5;
  word-wrap: break-word;
}

body md,
body figcaption,
body figure {
  display: block;
}

body summary {
  display: list-item;
}

body [hidden] {
  display: none !important;
}

body a {
  background-color: transparent;
  color: var(--accent-fg);
  text-decoration: none;
}

body a:active,
body a:hover {
  outline-width: 0;
}

body abbr[title] {
  border-bottom: none;
  text-decoration: underline dotted;
}

body b,
body strong {
  font-weight: 600;
}

body dfn {
  font-style: italic;
}

body h1 {
  margin: .67em 0;
  font-weight: 600;
  padding-bottom: .3em;
  font-size: 2em;
  border-bottom: 1px solid var(--border-muted);
}

body mark {
  background-color: var(--attention-subtle);
  color: var(--text-primary);
}

body small {
  font-size: 90%;
}

body sub,
body sup {
  font-size: 75%;
  line-height: 0;
  position: relative;
  vertical-align: baseline;
}

body sub {
  bottom: -0.25em;
}

body sup {
  top: -0.5em;
}

body img {
  border-style: none;
  max-width: 100%;
  box-sizing: content-box;
  background-color: var(--canvas-default);
}

body code,
body kbd,
body pre,
body samp {
  font-family: monospace,monospace;
  font-size: 1em;
}

body figure {
  margin: 1em 40px;
}

body hr {
  box-sizing: content-box;
  overflow: hidden;
  background: transparent;
  border-bottom: 1px solid var(--border-muted);
  height: .25em;
  padding: 0;
  margin: 24px 0;
  background-color: var(--border-default);
  border: 0;
}

body input {
  font: inherit;
  margin: 0;
  overflow: visible;
  font-family: inherit;
  font-size: inherit;
  line-height: inherit;
}

body a:hover {
  text-decoration: underline;
}

body hr::before {
  display: table;
  content: "";
}

body hr::after {
  display: table;
  clear: both;
  content: "";
}

body table {
  border-spacing: 0;
  border-collapse: collapse;
  display: block;
  width: max-content;
  max-width: 100%;
  overflow: auto;
}

body td,
body th {
  padding: 0;
}

body h1,
body h2,
body h3,
body h4,
body h5,
body h6 {
  margin-top: 24px;
  margin-bottom: 16px;
  font-weight: 600;
  line-height: 1.25;
}

body h2 {
  font-weight: 600;
  padding-bottom: .3em;
  font-size: 1.5em;
  border-bottom: 1px solid var(--border-muted);
}

body h3 {
  font-weight: 600;
  font-size: 1.25em;
}

body h4 {
  font-weight: 600;
  font-size: 1em;
}

body h5 {
  font-weight: 600;
  font-size: .875em;
}

body h6 {
  font-weight: 600;
  font-size: .85em;
  color: var(--fg-muted);
}

body p {
  margin-top: 0;
  margin-bottom: 10px;
}

body blockquote {
  margin: 0;
  padding: 0 1em;
  color: var(--fg-muted);
  border-left: .25em solid var(--border-default);
}

body ul,
body ol {
  margin-top: 0;
  margin-bottom: 0;
  padding-left: 2em;
}

body ol ol,
body ul ol {
  list-style-type: lower-roman;
}

body ul ul ol,
body ul ol ol,
body ol ul ol,
body ol ol ol {
  list-style-type: lower-alpha;
}

body dd {
  margin-left: 0;
}

body tt,
body code {
  font-family: ui-monospace,SFMono-Regular,SF Mono,Menlo,Consolas,Liberation Mono,monospace;
  font-size: 12px;
}

body pre {
  margin-top: 0;
  margin-bottom: 0;
  font-family: ui-monospace,SFMono-Regular,SF Mono,Menlo,Consolas,Liberation Mono,monospace;
  font-size: 12px;
  word-wrap: normal;
}

body ::placeholder {
  color: var(--fg-subtle);
  opacity: 1;
}

body>*:first-child {
  margin-top: 0 !important;
}

body>*:last-child {
  /*margin-bottom: 0 !important;*/
}

body a:not([href]) {
  color: inherit;
  text-decoration: none;
}

body p,
body blockquote,
body ul,
body ol,
body dl,
body table,
body pre,
body md {
  margin-top: 0;
  margin-bottom: 16px;
}

body blockquote>:first-child {
  margin-top: 0;
}

body blockquote>:last-child {
  margin-bottom: 0;
}

body sup>a::before {
  content: "[";
}

body sup>a::after {
  content: "]";
}

body h1 tt,
body h1 code,
body h2 tt,
body h2 code,
body h3 tt,
body h3 code,
body h4 tt,
body h4 code,
body h5 tt,
body h5 code,
body h6 tt,
body h6 code {
  padding: 0 .2em;
  font-size: inherit;
}

body ol[type="1"] {
  list-style-type: decimal;
}

body ol[type=a] {
  list-style-type: lower-alpha;
}

body ol[type=i] {
  list-style-type: lower-roman;
}

body div>ol:not([type]) {
  list-style-type: decimal;
}

body ul ul,
body ul ol,
body ol ol,
body ol ul {
  margin-top: 0;
  margin-bottom: 0;
}

body li>p {
  margin-top: 16px;
}

body li+li {
  margin-top: .25em;
}

body dl {
  padding: 0;
}

body dl dt {
  padding: 0;
  margin-top: 16px;
  font-size: 1em;
  font-style: italic;
  font-weight: 600;
}

body dl dd {
  padding: 0 16px;
  margin-bottom: 16px;
}

body table th {
  font-weight: 600;
}

body table th,
body table td {
  padding: 6px 13px;
  border: 1px solid var(--border-default);
}

body table tr {
  background-color: var(--canvas-default);
  border-top: 1px solid var(--border-muted);
}

body table tr:nth-child(2n) {
  background-color: var(--canvas-subtle);
}

body table img {
  background-color: transparent;
}

body img[align=right] {
  padding-left: 20px;
}

body img[align=left] {
  padding-right: 20px;
}


body code,
body tt {
  padding: .2em .4em;
  margin: 0;
  font-size: 85%;
  background-color: var(--neutral-muted);
  border-radius: 6px;
}

body code br,
body tt br {
  display: none;
}

body del code {
  text-decoration: inherit;
}

body pre code {
  font-size: 100%;
}

body pre>code {
  padding: 0;
  margin: 0;
  word-break: normal;
  white-space: pre;
  background: transparent;
  border: 0;
}

body pre {
  padding: 16px;
  overflow: auto;
  font-size: 85%;
  line-height: 1.45;
  background-color: var(--canvas-subtle);
  border-radius: 6px;
}

body pre code,
body pre tt {
  display: inline;
  max-width: auto;
  padding: 0;
  margin: 0;
  overflow: visible;
  line-height: inherit;
  word-wrap: normal;
  background-color: transparent;
  border: 0;
}

body ::-webkit-calendar-picker-indicator {
  filter: invert(50%);
}
body {
  width: 40em;
  margin: auto;
  margin-bottom: 24ex;
}

</style>

