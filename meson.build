project('Dndc', 'c', 'cpp', version: '0.7.2')
# FIXME: meson is producing x64 binaries on my m1 mac. Idk why.
COMPAT_VERSION = '0.7.1'
thread_dep = dependency('threads')
pymod = import('python')
pyinstall = pymod.find_installation()
pydep = pyinstall.dependency(embed: true)
if host_machine.system() == 'windows'
  ignore_bogus_deprecations = ['-D_CRT_NONSTDC_NO_WARNINGS', '-D_CRT_SECURE_NO_WARNINGS']
else
  ignore_bogus_deprecations = []
endif

if host_machine.system() == 'windows'
includes = include_directories('Allocators', 'Utils', 'PythonEmbed', 'Dndc', 'Platform/Windows', 'Vendored/quickjs')
else
includes = include_directories('Allocators', 'Utils', 'PythonEmbed', 'Dndc', 'Vendored/quickjs')
endif

install_headers('Dndc/dndc.h', subdir:'Dndc')

freeze_exec = executable(
  'freeze_py_module',
  'PythonEmbed/freeze_py_module.c',
  include_directories: includes,
  dependencies:pydep,
  c_args: ignore_bogus_deprecations,
  )
freezer = generator(
  freeze_exec,
  output: '@BASENAME@.py.h',
  arguments: ['@BASENAME@', '@INPUT@', '@OUTPUT@']
  )

frozen_modules = freezer.process(
  'PythonEmbed/vendored/abc.py',
  'PythonEmbed/vendored/importlib_bootstrap.py',
  'PythonEmbed/vendored/importlib_bootstrap_external.py',
  'PythonEmbed/vendored/zipimport.py',
  'PythonEmbed/vendored/io.py',
  'PythonEmbed/vendored/codecs.py',
  'PythonEmbed/vendored/encodings.py',
  'PythonEmbed/vendored/encodings.latin_1.py',
  'PythonEmbed/vendored/encodings.utf_8.py',
  'PythonEmbed/vendored/stat.py',
  'PythonEmbed/vendored/_collections_abc.py',
  'PythonEmbed/vendored/genericpath.py',
  'PythonEmbed/vendored/posixpath.py',
  'PythonEmbed/vendored/ntpath.py',
  'PythonEmbed/vendored/os.py',
  'PythonEmbed/vendored/operator.py',
  'PythonEmbed/vendored/keyword.py',
  'PythonEmbed/vendored/heapq.py',
  'PythonEmbed/vendored/reprlib.py',
  'PythonEmbed/vendored/collections.py',
  'PythonEmbed/vendored/collections.abc.py',
  'PythonEmbed/vendored/types.py',
  'PythonEmbed/vendored/functools.py',
  'PythonEmbed/vendored/weakref.py',
  'PythonEmbed/vendored/copy.py',
  'PythonEmbed/vendored/copyreg.py',
  'PythonEmbed/vendored/typing.py',
  'PythonEmbed/vendored/contextlib.py',
  'PythonEmbed/vendored/enum.py',
  'PythonEmbed/vendored/sre_constants.py',
  'PythonEmbed/vendored/sre_compile.py',
  'PythonEmbed/vendored/sre_parse.py',
  'PythonEmbed/vendored/re.py',
  'PythonEmbed/vendored/string.py',
  'PythonEmbed/vendored/cmd.py',
  'PythonEmbed/vendored/fnmatch.py',
  'PythonEmbed/vendored/token.py',
  'PythonEmbed/vendored/tokenize.py',
  'PythonEmbed/vendored/linecache.py',
  'PythonEmbed/vendored/dis.py',
  'PythonEmbed/vendored/opcode.py',
  'PythonEmbed/vendored/importlib.machinery.py',
  'PythonEmbed/vendored/inspect.py',
  'PythonEmbed/vendored/bdb.py',
  'PythonEmbed/vendored/code.py',
  'PythonEmbed/vendored/codeop.py',
  'PythonEmbed/vendored/traceback.py',
  'PythonEmbed/vendored/__future__.py',
  'PythonEmbed/vendored/glob.py',
  'PythonEmbed/vendored/pprint.py',
  'PythonEmbed/vendored/signal.py',
  'PythonEmbed/vendored/shlex.py',
  'PythonEmbed/vendored/pdb.py',
)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)
if host_machine.system() == 'windows'
  qjs_extra_args = ['-mrdseed']
else
  qjs_extra_args = []
endif

quickjs_static = static_library(
  'quickjs',
  'Vendored/libquickjs.c',
  include_directories: 'Vendored/quickjs',
  c_args: qjs_extra_args+ignore_bogus_deprecations,
  install: true,
  dependencies:[m_dep],
  )
quickjs_dylib = shared_library(
  'quickjs',
  'Vendored/libquickjs.c',
  include_directories: 'Vendored/quickjs',
  c_args: qjs_extra_args+['-DBUILDING_SHARED_OBJECT']+ignore_bogus_deprecations,
  install: true,
  version: meson.project_version(),
  soversion: meson.project_version(),
  darwin_versions: [COMPAT_VERSION, meson.project_version()],
  dependencies:[m_dep],
  )

executable(
  'dndc',
  'Dndc/dndc_cli.c',
  'PythonEmbed/frozenstdlib.c',
  frozen_modules,
  include_directories: includes,
  dependencies:[pydep, thread_dep],
  install: true,
  link_with: quickjs_static,
  )
dndc_lib = both_libraries(
  'dndc',
  'Dndc/dndc.c',
  'PythonEmbed/frozenstdlib.c',
  frozen_modules,
  include_directories: includes,
  dependencies:[pydep, thread_dep],
  install: true,
  version: meson.project_version(),
  soversion: meson.project_version(),
  darwin_versions: [COMPAT_VERSION, meson.project_version()],
  link_with: quickjs_static,
  )


pyinstall.extension_module(
  'pydndc',
  'Dndc/pydndc.c',
  include_directories: includes,
  link_with: quickjs_static,
  dependencies:pyinstall.dependency(),
  )

if host_machine.system() == 'darwin'
  add_languages('objc')
  executable(
    'gdndc',
    'Platform/MacOS/gdndc.m',
    'PythonEmbed/frozenstdlib.c',
    frozen_modules,
    dependencies:[pydep, dependency('appleframeworks', modules: ['Cocoa', 'WebKit'])],
    include_directories: includes,
    objc_args: ['-fobjc-arc'],
    link_args: '-Wl,-sectcreate,__TEXT,__info_plist,'+meson.source_root() + '/Platform/MacOS/Info.plist',
    link_with: quickjs_static,
    install: true,
    )
endif

# tests
testkw = {'include_directories': includes}
t = executable('TestArgumentParsing', 'Utils/TestArgumentParsing.c', kwargs:testkw, c_args:ignore_bogus_deprecations)
test('argumentparsing', t)
t = executable('TestBase64', 'Utils/TestBase64.c', kwargs:testkw, c_args:ignore_bogus_deprecations)
test('base64', t)
t = executable('TestByteBuilder', 'Utils/TestByteBuilder.c', kwargs:testkw, c_args:ignore_bogus_deprecations)
test('bytebuilder', t)
t = executable('TestMStringBuilder', 'Utils/TestMStringBuilder.c', kwargs:testkw, c_args:ignore_bogus_deprecations)
test('mstringbuilder', t)
t = executable('TestPathUtil', 'Utils/TestPathUtil.c', kwargs:testkw, c_args:ignore_bogus_deprecations)
test('pathutil', t)
t = executable('TestStrUtils', 'Utils/TestStrUtils.c', kwargs:testkw, c_args:ignore_bogus_deprecations)
test('strutil', t)
t = executable('TestRarray', 'Utils/TestRarray.c', kwargs:testkw, c_args:ignore_bogus_deprecations)
test('rarray', t)
t = executable('TestDndc',
  'Dndc/TestDndc.c',
  'PythonEmbed/frozenstdlib.c',
  frozen_modules,
  include_directories:includes,
  dependencies:[pydep, thread_dep],
  c_args: ['-DQJS_SHARED_LIBRARY']+ignore_bogus_deprecations,
  link_with: quickjs_dylib,
  )
test('dndc', t, args:['-C', meson.source_root()])

qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: ['Core', 'Gui', 'Widgets', 'WebEngineWidgets'], required:false)
if qt5_dep.found()
  moc_files = qt5.compile_moc(headers: 'QtDndcEdit/DndcEdit.h', dependencies: qt5_dep)
  executable('DndcEdit', 'QtDndcEdit/DndcEdit.cpp', moc_files, dependencies: qt5_dep, link_with:dndc_lib)
endif
