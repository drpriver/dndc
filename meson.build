project('DndC', 'c', version: '0.3.13')

pymod = import('python')
pyinstall = pymod.find_installation()
pydep = pyinstall.dependency(embed: true)
if host_machine.system() == 'windows'
includes = include_directories('Allocators', 'Utils', 'PythonEmbed', 'DndC', 'Platform/Windows')
else
includes = include_directories('Allocators', 'Utils', 'PythonEmbed', 'DndC')
endif

freeze_exec = executable(
  'freeze_py_module',
  'PythonEmbed/freeze_py_module.c',
  include_directories: includes,
  dependencies:pydep,
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

executable(
  'dndc',
  'DndC/dndc.c',
  'PythonEmbed/frozenstdlib.c',
  frozen_modules,
  include_directories: includes,
  dependencies:[pydep],
  c_args: ['-DDNDCMAIN'],
  install: true,
  )

pyinstall.extension_module(
  'pydndc',
  'DndC/pydndc.c',
  include_directories: includes,
  dependencies:pyinstall.dependency(),
  )

if host_machine.system() == 'darwin'
  add_languages('objc')
  executable(
    'gdndc',
    'Platform/MacOS/gdndc.m',
    'DndC/dndc.c',
    'PythonEmbed/frozenstdlib.c',
    frozen_modules,
    dependencies:[pydep, dependency('appleframeworks', modules: ['Cocoa', 'WebKit'])],
    include_directories: includes,
    objc_args: ['-fobjc-arc'],
    link_args: '-Wl,-sectcreate,__TEXT,__info_plist,'+meson.source_root() + '/Platform/MacOS/Info.plist',
    install: true,
    )
endif

# tests
testkw = {'include_directories': includes}
t = executable('TestArgumentParsing', 'Utils/TestArgumentParsing.c', kwargs:testkw)
test('argumentparsing', t)
t = executable('TestBase64', 'Utils/TestBase64.c', kwargs:testkw)
test('base64', t)
t = executable('TestByteBuilder', 'Utils/TestByteBuilder.c', kwargs:testkw)
test('bytebuilder', t)
t = executable('TestMStringBuilder', 'Utils/TestMStringBuilder.c', kwargs:testkw)
test('mstringbuilder', t)
t = executable('TestPathUtil', 'Utils/TestPathUtil.c', kwargs:testkw)
test('pathutil', t)
t = executable('TestStrUtils', 'Utils/TestStrUtils.c', kwargs:testkw)
test('strutil', t)
t = executable('TestDndC',
  'DndC/TestDndC.c',
  'PythonEmbed/frozenstdlib.c',
  frozen_modules,
  include_directories:includes,
  dependencies:pydep,
  )
test('dndc', t, args:['-C', meson.source_root()])
