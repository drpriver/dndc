# python package structure is fucking garbage, so we have this stupid dummy
# file that just re-exports the symbols from the c extensions.
from .pydndc import *
from .pydndc import __doc__, __version__
