project('DndC', 'c')

pymod = import('python')
pyinstall = pymod.find_installation()
pydep = pyinstall.dependency(embed: true)
includes = include_directories('Allocators', 'Utils', 'PythonEmbed', 'DndC')

executable(
  'dndc', 
  'DndC/dndc.c', 'PythonEmbed/frozenstdlib.c', 
  include_directories: includes, 
  dependencies:pydep, 
  c_args: ['-DDNDCMAIN']
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
    dependencies:[pydep, dependency('appleframeworks', modules: ['Cocoa', 'WebKit'])],
    include_directories: includes,
    objc_args: ['-fobjc-arc'],
    link_args: '-Wl,-sectcreate,__TEXT,__info_plist,'+meson.source_root() + '/Platform/MacOS/Info.plist'
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
t = executable('TestDndC', 'DndC/TestDndC.c', 'PythonEmbed/frozenstdlib.c', include_directories:includes, dependencies:pydep)
test('dndc', t, args:['-C', meson.source_root()])

