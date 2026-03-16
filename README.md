<!-- This md file was generated from a dnd file. -->
## Table of Contents
* [Building](#building)
  * [Requirements](#requirements)
  * [Make](#make)
  * [Meson](#meson)
  * [Cmake](#cmake)
  * [setuptools](#setuptools)
* [Vendored Libraries](#vendored-libraries)
  * [quickjs](#quickjs)
* [Build Artifacts](#build-artifacts)
  * [dndc](#dndc)
  * [libdndc](#libdndc)
  * [dndc.wasm](#dndcwasm)
  * [pydndc](#pydndc)
  * [DndEdit](#dndedit)
  * [DndcEdit](#dndcedit)
  * [PyDndEdit](#pydndedit)
  * [dndc-tag](#dndc-tag)
  * [dndc-browse](#dndc-browse)
  * [dndbr](#dndbr)
* [Testing](#testing)
* [Type Checking](#type-checking)
* [Scripts](#scripts)
  * [tag_and_syntax.py](#tagandsyntaxpy)
  * [cdoc.py](#cdocpy)
  * [make_cdoc_index.py](#makecdocindexpy)
  * [win_utils.py](#winutilspy)
  * [convert.py](#convertpy)
* [Docs](#docs)


# Dndc


See [the overview](Documentation/OVERVIEW.dnd), written in <code>.dnd</code> for a description of the file
format. Or checkout [the reference](Documentation/REFERENCE.dnd).


This is a compiler and associated tooling for the dnd document format. DND
stands for David's New Document.

## Building

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

Building with the <code>Makefile</code> requires gnu make. The first time
you invoke <code>make</code>, it will create a makefile called
<code>opt.mak</code>. <code>opt.mak</code> allows you to configure things
like debug vs release build, the right compile flags for the python
extension if on windows and whether to build the qt-based gui editor.


You can invoke <code>make opt.mak</code> yourself to not use the default
options before the first build.


To compile everything + tests, do <code>make all</code>.


To compile and run the tests, do <code>make tests</code>.


To compile the documentation, do <code>make docs</code> and for C docs do
<code>make cdocs</code>.


You can list the make targets with <code>make list</code>.

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
<code>setup.py</code> script. Builds with gcc, clang or clang-cl.

</details>


## Vendored Libraries
#### quickjs

The dnd document format allows for in-document scripting to manipulate the
document's ast using javascript. To support this, we include a vendored copy
of [quickjs](https://bellard.org/quickjs/), with some small modifications.


It is a future project to see if we can use system javascript engines
instead to reduce the filesize footprint of dndc. For example, javascript
core on macos.



## Build Artifacts
#### dndc

The command-line, batch program that can compile dnd files into html, md or
to dnd. It can also be used to format the dnd file to a standard width.


#### libdndc

Both static and dynamic versions are supported. libquickjs is also built and
is required for linking. You can either transforms strings in a batch mode,
similar to the interface of [dndc](#dndc), or you can use the slightly more
low-level ast api to manipulate a dnd document.


#### dndc.wasm

A stripped down version of [libdndc](#libdndc) is available as a wasm target. Some of
the output formats are not available and in-document scripting with quickjs
is not enabled.


Compiling to wasm requires compiler support. Apple clang does not support
this, but other versions of clang do.


#### pydndc

[libdndc](#libdndc), but as a python extension. Exposes a pythonic api to libdndc.
This will embed quickjs in the python extension.


#### DndEdit

Mac only. A rich gui editor for dnd files, showing the resulting document in
a webview. Built with appkit.


This does not produce an app bundle. It creates a self-contained executable
with an embedded Info.plist and app icon. Patches to create an actual app
bundle instead are welcome.


#### DndcEdit

Qt 5 or 6 required. Also a rich gui editor for dnd files, like DndEdit. This
is built with Qt instead.


#### PyDndEdit

A python version of DndcEdit, using pydndc and PySide6's bindings to Qt.


#### dndc-tag

A command-line utility that will create tags from a list of files or the
current directory. These tags are formatted so vim and similar editors can
use them to jump around.


#### dndc-browse

A command-line utility that will start a local webserver that will serve dnd
files in the current directory as html. All documents are compiled upon
request, so you can edit documents and then immediately see the new document
in your browser.


#### dndbr

Mac Only. This is similar to dndc-browse, but is a gui app instead of a
command-line utility.


Similar to DndEdit, this does not produce an app bundle.



## Testing

Tests are C programs that can be compiled and run individually. If you are
building with make, <code>make tests</code> will build and run all the tests.
If using meson, you can do <code>meson test -C <builddir></code> or do
<code>ninja test</code> from the build directory. With Cmake, be aware that
<code>ninja test</code> does not build the tests, it only runs them. Cmake
just being Cmake I guess.


Once the tests are compiled, you can run them individually if you want. Run
the binary with <code>-h</code> to see a list of options. You can even run
only a subset of tests within each binary.


Some tests need their working directory to be the project root as they read
the examples or the testcases, etc. Each test has a <code>-C</code> argument
to set the current directory.


There is also a python test for the python extension, at
[Dndc/testpydndc.py](Dndc/testpydndc.py). That one needs to know where the compiled
python extension is and has an argument for that.


## Type Checking

The python code should all type check with mypy. If it doesn't then that is a
bug.


## Scripts

There are some useful scripts in the <code>Scripts</code> folder for
development.

#### [tag_and_syntax.py](Scripts/tag_and_syntax.py)

This script requires libclang and might only work on my machines. If it does
work, it will produce a tags file for vim and also a .vimrc file that will
setup semantic highlighting in vim.


This is invoked by <code>make tags</code>.


#### [cdoc.py](Scripts/cdoc.py)

This script also requires libclang. It converts C headers into a
syntax/semantic highlighted hyperlinked html page.


This and <code>make_cdoc_index.py</code> are invoked by <code>make`
cdocs.


#### [make_cdoc_index.py](Scripts/make_cdoc_index.py)

This script will add an index.html to all the folders generated by
<code>cdoc.py</code>.


#### [win_utils.py](Scripts/win_utils.py)

On windows the basic cli commands are different and weird. To make up for
this, this script implements some really basic ones. It implements
<code>rm</code>, <code>mkdir</code>, <code>mv</code>, <code>cp</code>, and
<code>install</code>.


#### [convert.py](Scripts/convert.py)

This script can be run in two modes. In either mode, it will recursively
find all interesting files if no files are given as an argument. If
<code>--strip_only</code> is passed, then it will strip trailing whitespace
from all of the files. If not, then it will do some regex replacements in
all of the files. This should probably take those as an argument, but it
doesn't, you have to edit the script.


This can be invoked by <code>make strip</code> or <code>make convert</code>.



## Docs

Using the makefile, you can generate a dash/zeal docset. Simply do <code>make`
docset.


Additionally, <code>make docs</code> will generate some html files in the
RenderedDocs folder which you can browse.
