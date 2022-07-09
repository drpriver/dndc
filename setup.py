# Always prefer setuptools over distutils
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

# Arguments marked as 'Required' below must be included for upload to PyPI.
# Fields marked as 'Optional' may be commented out.
setup(
    name='pydndc',  # Required
    version='0.23.0',  # Required
    license='Proprietary',
    description='dndc, but from python',  # Optional
    # url='https://github.com/pypa/sampleproject',  # Optional
    author='David Priver',  # Optional
    # author_email='author@example.com',  # Optional
    # Classifiers help users find your project by categorizing it.
    #
    # For a list of valid classifiers, see https://pypi.org/classifiers/
    classifiers=[  # Optional
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
    # Specify which Python versions you support. In contrast to the
    # 'Programming Language' classifiers above, 'pip install' will check this
    # and refuse to install the project if the version does not match. See
    # https://packaging.python.org/guides/distributing-packages-using-setuptools/#python-requires
    python_requires='>=3.8, <4',
    package_data={
        'pydndc': ['py.typed', 'pydndc.pyi'],
    },
)
