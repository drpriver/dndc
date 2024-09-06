
project('Dndc', 'c', 'cpp', version: '1.3.2', default_options:['c_std=gnu17,c17', 'cpp_std=gnu++17,c++17'])
COMPAT_VERSION = '1.0.0'
thread_dep = dependency('threads')
pymod = import('python')
pyinstall = pymod.find_installation()
if host_machine.system() == 'windows'
  ignore_bogus_deprecations = ['-D_CRT_NONSTDC_NO_WARNINGS', '-D_CRT_SECURE_NO_WARNINGS']
else
  ignore_bogus_deprecations = []
endif
if host_machine.system() == 'darwin'
  arches = ['-arch', 'x86_64', '-arch', 'arm64']
else
  arches = []
endif

includes = include_directories('.')
install_headers('Dndc/dndc.h', 'Dndc/dndc_ast.h', subdir:'Dndc')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)
if host_machine.system() == 'windows'
  # python header on windows has a bad timeval decl
  if cc.get_id() == 'clang'
    py_extra_args = ['-Wno-visibility']
  else
    py_extra_args = []
  endif
  qjs_extra_args = ['-mrdseed']
else
  py_extra_args = []
  qjs_extra_args = []
endif

quickjs_static = static_library(
  'quickjs',
  'Vendored/libquickjs.c',
  include_directories: 'Vendored/quickjs',
  c_args: qjs_extra_args+ignore_bogus_deprecations+arches,
  install: true,
  dependencies:[m_dep],
)
quickjs_dylib = shared_library(
  'quickjs',
  'Vendored/libquickjs.c',
  include_directories: 'Vendored/quickjs',
  c_args: qjs_extra_args+['-DBUILDING_SHARED_OBJECT']+ignore_bogus_deprecations+arches,
  install: true,
  version: meson.project_version(),
  soversion: meson.project_version(),
  darwin_versions: [COMPAT_VERSION, meson.project_version()],
  dependencies:[m_dep],
  link_args:arches,
)

executable(
  'dndc',
  'Dndc/dndc_cli.c',
  include_directories: includes,
  dependencies:[thread_dep],
  install: true,
  link_with: quickjs_static,
  c_args:ignore_bogus_deprecations+arches,
  link_args:arches,
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
  c_args:ignore_bogus_deprecations+arches,
  link_args:arches,
)
if host_machine.system() == 'darwin'
  browsedep = [dependency('appleframeworks', modules:['Cocoa'])]
else
  browsedep = []
endif
executable(
  'dndc-browse',
  'Dndc/dndc_browse.c',
  include_directories: includes,
  install: true,
  link_with: dndc_lib,
  dependencies:[thread_dep] + browsedep,
  c_args:ignore_bogus_deprecations+arches,
  link_args:arches,
)
executable(
  'dndc-tag',
  'Dndc/dndc_tag.c',
  include_directories: includes,
  install: true,
  link_with: dndc_lib,
  dependencies:[thread_dep],
  c_args:ignore_bogus_deprecations+arches,
  link_args:arches,
)


pydndc = pyinstall.extension_module(
  'pydndc',
  'Dndc/pydndc.c',
  include_directories: includes,
  link_with: quickjs_static,
  dependencies:pyinstall.dependency(),
  c_args:ignore_bogus_deprecations + py_extra_args+arches,
  link_args:arches,
)

if host_machine.system() == 'darwin'
  add_languages('objc')
  executable(
    'DndEdit',
    'Platform/MacOS/gdndc.m',
    dependencies:[dependency('appleframeworks', modules: ['Cocoa', 'WebKit'])],
    include_directories: includes,
    objc_args: ['-fobjc-arc', '-DAPP_ICON_PATH="'+meson.source_root()+'/Platform/MacOS/app_icon.png'+'"']+arches,
    link_args: ['-Wl,-sectcreate,__TEXT,__info_plist,'+meson.source_root() + '/Platform/MacOS/Info.plist']+arches,
    link_with: quickjs_static,
    install: true,
  )
  executable(
    'dndbr',
    'Platform/MacOS/dndbr.m',
    'Dndc/dndc_local_server.c',
    dependencies:[dependency('appleframeworks', modules:'Cocoa')],
    include_directories: includes,
    objc_args: ['-fobjc-arc', '-DAPP_ICON_PATH="'+meson.source_root()+'/Platform/MacOS/dndbr_app_icon.png'+'"']+arches,
    c_args:arches,
    link_args: ['-Wl,-sectcreate,__TEXT,__info_plist,'+meson.source_root() + '/Platform/MacOS/DndBrInfo.plist']+arches,
    link_with: dndc_lib,
    install: true,
  )

endif

# tests
testkw = {'include_directories': includes}
test('argumentparsing', executable('TestArgumentParsing', 'Utils/TestArgumentParsing.c', kwargs:testkw, c_args:ignore_bogus_deprecations, dependencies:[thread_dep]))
test('base64', executable('TestBase64', 'Utils/TestBase64.c', kwargs:testkw, c_args:ignore_bogus_deprecations, dependencies:[thread_dep]))
test('dsort', executable('TestDsort', 'Utils/TestDsort.c', kwargs:testkw, c_args:ignore_bogus_deprecations, dependencies:[thread_dep]))
test('mstringbuilder', executable('TestMStringBuilder', 'Utils/TestMStringBuilder.c', kwargs:testkw, c_args:ignore_bogus_deprecations, dependencies:[thread_dep]))
test('pathutil', executable('TestPathUtil', 'Utils/TestPathUtil.c', kwargs:testkw, c_args:ignore_bogus_deprecations, dependencies:[thread_dep]))
test('rarray', executable('TestRarray', 'Utils/TestRarray.c', kwargs:testkw, c_args:ignore_bogus_deprecations, dependencies:[thread_dep]))
test('strutil', executable('TestStrUtils', 'Utils/TestStrUtils.c', kwargs:testkw, c_args:ignore_bogus_deprecations, dependencies:[thread_dep]))
test('msbext', executable('TestMsbExtension', 'Utils/TestMsbExtension.c', kwargs:testkw, c_args:ignore_bogus_deprecations, dependencies:[thread_dep]))
test('attr-table', executable('TestAttrTable', 'Dndc/TestAttrTable.c', kwargs:testkw, c_args:ignore_bogus_deprecations, dependencies:[thread_dep]))
test('dndc',
  executable('TestDndc',
    'Dndc/TestDndc.c',
    include_directories:includes,
    dependencies:[thread_dep],
    c_args: ['-DQJS_SHARED_LIBRARY']+ignore_bogus_deprecations,
    link_with: quickjs_dylib,
  ),
  args:['-C', meson.source_root()]
)
test('dndc-ast',
  executable('TestDndcAst',
    'Dndc/TestDndcAst.c',
    include_directories:includes,
    dependencies:[thread_dep],
    c_args: ['-DQJS_SHARED_LIBRARY']+ignore_bogus_deprecations,
    link_with: quickjs_dylib,
  ),
  args:['-C', meson.source_root()]
)
dndc_alloc = executable('TestDndcAlloc',
  'Dndc/TestDndcAlloc.c',
  include_directories:includes,
  dependencies:[thread_dep],
  c_args: ['-DQJS_SHARED_LIBRARY']+ignore_bogus_deprecations,
  link_with: quickjs_dylib,
)

# Run the same test with different args for parallelism
test('dndc-alloc-html', dndc_alloc,
  args:['-C', meson.source_root(), '--html']
)
test('dndc-alloc-md', dndc_alloc,
  args:['-C', meson.source_root(), '--md']
)
test('dndc-alloc-reformat', dndc_alloc,
  args:['-C', meson.source_root(), '--reformat']
)
test('dndc-alloc-expand', dndc_alloc,
  args:['-C', meson.source_root(), '--expand']
)

test('test-qjs',
  executable(
    'TestQJS',
    'Dndc/TestQJS.c',
    include_directories: includes,
    dependencies:m_dep,
    c_args:ignore_bogus_deprecations+qjs_extra_args),
  args:['-C', meson.source_root()]
)

test('pydndc', pyinstall, args:['Dndc/testpydndc.py', '--extension-directory', meson.build_root()], workdir:meson.source_root(), depends:pydndc)

qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Gui', 'Widgets', 'WebEngineWidgets'], required:false)
if qt6_dep.found()
  moc_files = qt6.compile_moc(headers: 'QtDndcEdit/DndcEdit.h', dependencies: qt6_dep)
  executable('DndcEdit', 'QtDndcEdit/DndcEdit.cpp', moc_files, dependencies: qt6_dep, link_with:dndc_lib)
else
  qt5 = import('qt5')
  qt5_dep = dependency('qt5', modules: ['Core', 'Gui', 'Widgets', 'WebEngineWidgets'], required:false)
  if qt5_dep.found()
    moc_files = qt5.compile_moc(headers: 'QtDndcEdit/DndcEdit.h', dependencies: qt5_dep)
    executable('DndcEdit', 'QtDndcEdit/DndcEdit.cpp', moc_files, dependencies: qt5_dep, link_with:dndc_lib)
  endif
endif
