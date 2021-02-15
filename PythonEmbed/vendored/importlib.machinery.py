"""The machinery of importlib: finders, loaders, hooks, etc."""

import _imp

# FROZENHACKS: why not use the frozen importlib version?
from _frozen_importlib import ModuleSpec
from _frozen_importlib import BuiltinImporter
from _frozen_importlib import FrozenImporter
from _frozen_importlib_external import (SOURCE_SUFFIXES, DEBUG_BYTECODE_SUFFIXES,
                     OPTIMIZED_BYTECODE_SUFFIXES, BYTECODE_SUFFIXES,
                     EXTENSION_SUFFIXES)
from _frozen_importlib_external import WindowsRegistryFinder
from _frozen_importlib_external import PathFinder
from _frozen_importlib_external import FileFinder
from _frozen_importlib_external import SourceFileLoader
from _frozen_importlib_external import SourcelessFileLoader
from _frozen_importlib_external import ExtensionFileLoader


def all_suffixes():
    """Returns a list of all recognized module suffixes for this process"""
    return SOURCE_SUFFIXES + BYTECODE_SUFFIXES + EXTENSION_SUFFIXES
