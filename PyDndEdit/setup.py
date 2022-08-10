from setuptools import setup

setup(
    name='PyDndEdit',
    version='0.27.0',
    license='Proprietary',
    description='Qt based editor for the dnd file format',
    author = 'David Priver',
    author_email='david@davidpriver.com',
    classifiers = [
        'Development Status :: 4 - Beta',
        'License :: Other/Proprietary License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3 :: Only',
    ],
    packages=['PyDndEdit'],  # Required
    python_requires='>=3.8, <4',
    package_data={
        'PyDndEdit':[
            'Manual.dnd',
            'changelog.dnd',
            # 'OVERVIEW.dnd',
            'REFERENCE.dnd',
            'jsdoc.dnd',
            'dndc_js_api.d.ts',
        ],
    },
    install_requires=[
        'PySide6',
        'pydndc',
    ],
)
