from setuptools import setup, find_packages, Extension
import sys
if sys.platform == 'win32':
    # hack to compile with clang
    # pr to add clang cl was never merged to distutils
    # https://github.com/pypa/distutils/pull/7
    import distutils._msvccompiler
    class ClangCl(distutils._msvccompiler.MSVCCompiler):
        def initialize(self):
            super().initialize()
            self.cc = 'clang-cl.exe'
            self.compile_options.append('-mrdseed')
            self.compile_options.append('-Wno-unused-variable')
            self.compile_options.append('-Wno-visibility')
            self.compile_options.append('-D_CRT_SECURE_NO_WARNINGS=1')
    distutils._msvccompiler.MSVCCompiler = ClangCl

extension = Extension(
    'pydndc.pydndc',
    sources = ['Dndc/pydndc.c', 'Vendored/libquickjs.c'],
    include_dirs=['.'],
    define_macros=[('BUILDING_PYTHON_EXTENSION', '1')]
)

setup(
    name='pydndc',
    version='0.26.0',
    license='Proprietary',
    description='dndc, but from python',
    # url='https://github.com/pypa/sampleproject',
    author='David Priver',
    author_email='david@davidpriver.com',
    classifiers=[
        # How mature is this project? Common values are
        #   3 - Alpha
        #   4 - Beta
        #   5 - Production/Stable
        'Development Status :: 4 - Beta',
        # Indicate who your project is intended for
        # 'Intended Audience :: Developers',
        # 'Topic :: Software Development :: Build Tools',
        # Pick your license as you wish
        'License :: Other/Proprietary License',
        # 'License :: OSI Approved :: MIT License',
        # Specify the Python versions you support here. In particular, ensure
        # that you indicate you support Python 3. These classifiers are *not*
        # checked by 'pip install'. See instead 'python_requires' below.
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3 :: Only',
    ],
    # When your source code is in a subdirectory under the project root, e.g.
    # `src/`, it is necessary to specify the `package_dir` argument.
    # package_dir={'': '..'},  # Optional
    # You can just specify package directories manually here if your project is
    # simple. Or you can use find_packages().
    #
    # Alternatively, if you just want to distribute a single Python file, use
    # the `py_modules` argument instead as follows, which will expect a file
    # called `my_module.py` to exist:
    #
    #   py_modules=['my_module'],
    #
    packages=['pydndc'],  # Required
    ext_modules = [extension],
    python_requires='>=3.6, <4',
    package_data={
        'pydndc': ['py.typed', 'pydndc.pyi'],
    },
)
