project('Dndc', 'c', 'cpp', version: '0.8.0')
# FIXME: meson is producing x64 binaries on my m1 mac. Idk why.
COMPAT_VERSION = '0.8.0'
thread_dep = dependency('threads')
pymod = import('python')
pyinstall = pymod.find_installation()
if host_machine.system() == 'windows'
  ignore_bogus_deprecations = ['-D_CRT_NONSTDC_NO_WARNINGS', '-D_CRT_SECURE_NO_WARNINGS']
else
  ignore_bogus_deprecations = []
endif

if host_machine.system() == 'windows'
includes = include_directories('Allocators', 'Utils', 'Dndc', 'Platform/Windows', 'Vendored/quickjs')
else
includes = include_directories('Allocators', 'Utils', 'Dndc', 'Vendored/quickjs')
endif

install_headers('Dndc/dndc.h', subdir:'Dndc')

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
  include_directories: includes,
  dependencies:[thread_dep],
  install: true,
  link_with: quickjs_static,
  )
dndc_lib = both_libraries(
  'dndc',
  'Dndc/dndc.c',
  include_directories: includes,
  dependencies:[thread_dep],
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
    dependencies:[dependency('appleframeworks', modules: ['Cocoa', 'WebKit'])],
    include_directories: includes,
    objc_args: ['-fobjc-arc'],
    link_args: '-Wl,-sectcreate,__TEXT,__info_plist,'+meson.source_root() + '/Platform/MacOS/Info.plist',
    link_with: quickjs_static,
    install: true,
    )
endif

# tests
testkw = {'include_directories': includes}
test('argumentparsing', executable('TestArgumentParsing', 'Utils/TestArgumentParsing.c', kwargs:testkw, c_args:ignore_bogus_deprecations))
test('base64', executable('TestBase64', 'Utils/TestBase64.c', kwargs:testkw, c_args:ignore_bogus_deprecations))
test('bytebuilder', executable('TestByteBuilder', 'Utils/TestByteBuilder.c', kwargs:testkw, c_args:ignore_bogus_deprecations))
test('mstringbuilder', executable('TestMStringBuilder', 'Utils/TestMStringBuilder.c', kwargs:testkw, c_args:ignore_bogus_deprecations))
test('pathutil', executable('TestPathUtil', 'Utils/TestPathUtil.c', kwargs:testkw, c_args:ignore_bogus_deprecations))
test('strutil', executable('TestStrUtils', 'Utils/TestStrUtils.c', kwargs:testkw, c_args:ignore_bogus_deprecations))
test('rarray', executable('TestRarray', 'Utils/TestRarray.c', kwargs:testkw, c_args:ignore_bogus_deprecations))
test('dndc',
  executable('TestDndc',
  'Dndc/TestDndc.c',
  include_directories:includes,
  dependencies:[thread_dep],
  c_args: ['-DQJS_SHARED_LIBRARY']+ignore_bogus_deprecations,
  link_with: quickjs_dylib,
  ),
  args:['-C', meson.source_root()])

qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: ['Core', 'Gui', 'Widgets', 'WebEngineWidgets'], required:false)
if qt5_dep.found()
  moc_files = qt5.compile_moc(headers: 'QtDndcEdit/DndcEdit.h', dependencies: qt5_dep)
  executable('DndcEdit', 'QtDndcEdit/DndcEdit.cpp', moc_files, dependencies: qt5_dep, link_with:dndc_lib)
endif
