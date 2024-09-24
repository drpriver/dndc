<!-- This md file was generated from a dnd file. -->
<h2>Table of Contents</h2>
<h1>Dndc</h1>

See [the overview](Documentation/OVERVIEW.dnd), written in `.dnd` for a description of the file
format. Or checkout [the reference](Documentation/REFERENCE.dnd).


This is a compiler and associated tooling for the dnd document format. DND
stands for David's New Document.


&lt;table&gt;&lt;tbody&gt;&lt;tr&gt;&lt;td&gt;Wtf&lt;td&gt;Is&lt;td&gt;This&lt;td&gt;bullshit&lt;/table&gt;

<pre>
&lt;table&gt;&lt;tbody&gt;&lt;tr&gt;&lt;td&gt;Wtf&lt;td&gt;Is&lt;td&gt;This&lt;td&gt;bullshit&lt;/table&gt;
::hello

</pre>

<h2>Building</h2>
This project builds with clang (including apple clang) or gcc. It should
compile with MSVC.


Mingw is not supported. Or maybe it is, I haven't tried.

<details><summary>Requirements</summary>

You need a C compiler. It is known to work with gcc and clang.
It should work with MSVC.


You need [make](#make), [meson](#meson) or [cmake](#cmake) to build the code.
Or if you really want you can read the makefiles and type the build command by hand.


The python extension requires python and supports python3.6+.


Qt-based editor requires Qt 5 or 6 and a C++ compiler.

</details>

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
`setup.py` script. Builds with gcc, clang or clang-cl.

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

<dt>[tag_and_syntax.py](Scripts/tag_and_syntax.py)</dt>
<dd>

This script requires libclang and might only work on my machines. If it does
work, it will produce a tags file for vim and also a .vimrc file that will
setup semantic highlighting in vim.


This is invoked by `make tags`.

</dd>

<dt>[cdoc.py](Scripts/cdoc.py)</dt>
<dd>

This script also requires libclang. It converts C headers into a
syntax/semantic highlighted hyperlinked html page.


This and `make_cdoc_index.py` are invoked by `make`
cdocs.

</dd>

<dt>[make_cdoc_index.py](Scripts/make_cdoc_index.py)</dt>
<dd>

This script will add an index.html to all the folders generated by
`cdoc.py`.

</dd>

<dt>[win_utils.py](Scripts/win_utils.py)</dt>
<dd>

On windows the basic cli commands are different and weird. To make up for
this, this script implements some really basic ones. It implements
`rm`, `mkdir`, `mv`, `cp`, and
`install`.

</dd>

<dt>[convert.py](Scripts/convert.py)</dt>
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

<h2>Docs</h2>
Using the makefile, you can generate a dash/zeal docset. Simply do `make`
docset.


Additionally, `make docs` will generate some html files in the
RenderedDocs folder which you can browse.
