from __future__ import annotations
from typing import Callable, NamedTuple, Any, Sequence, Iterable, Iterator
from enum import IntEnum, IntFlag

class FileCache:
    def remove(self, /, path:str) -> None:
        ...
    def clear(self) -> None:
        ...
    def paths(self) -> list[str]:
        ...
    def store(self, path:str, data:str, overwrite:bool=True) -> bool:
        ...

class MsgType(IntEnum):
    ERROR: int
    WARNING: int
    NODELESS: int
    STATISTIC: int
    DEBUG: int
# Called with (type, filename, line, col, messsage)
Logger = Callable[[MsgType, str, int, int, str], None]
stderr_logger: Logger


class SynType(IntEnum):
    DOUBLE_COLON: int
    HEADER: int
    NODE_TYPE: int
    ATTRIBUTE: int
    DIRECTIVE: int
    ATTRIBUTE_ARGUMENT: int
    CLASS: int
    RAW_STRING: int
    JS_COMMENT: int
    JS_STRING: int
    JS_REGEX: int
    JS_NUMBER: int
    JS_KEYWORD:int
    JS_KEYWORD_VALUE: int
    JS_VAR: int
    JS_IDENTIFIER: int
    JS_BUILTIN: int
    JS_NODETYPE: int
    JS_BRACE: int

class Flags(IntFlag):
    NONE: int
    INPUT_IS_UNTRUSTED: int
    FRAGMENT_ONLY: int
    DONT_INLINE_IMAGES: int
    NO_THREADS: int
    USE_DND_URL_SCHEME: int
    STRIP_WHITESPACE: int
    DONT_READ: int
    DONT_IMPORT: int
    NO_COMPILETIME_JS: int
    SUPPRESS_WARNINGS: int
    PRINT_STATS: int
    DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP: int
    NO_CSS: int
    ENABLE_JS_WRITE: int

class SyntaxRegion(NamedTuple):
    type: SynType
    column: int
    offset: int
    length: int

def htmlgen(
    text:str,
    base_dir:str='.',
    filename:str='',
    logger:Logger | None=None,
    file_cache:FileCache | None=None,
    flags:Flags=Flags.NONE,
    jsargs:dict| list| str | None = None,
    deps:set[str] | None = None,
) -> str:
    ...

def expand(
    text:str,
    base_dir:str='.',
    filename:str='',
    logger:Logger | None=None,
    file_cache:FileCache | None=None,
    flags:Flags=Flags.NONE,
    jsargs:dict | list| str | None = None
) -> str:
    ...

def to_markdown(
    text:str,
    base_dir:str='.',
    filename:str='',
    logger:Logger | None=None,
    file_cache:FileCache | None=None,
    flags:Flags=Flags.NONE,
    jsargs:dict | list | str | None=None,
) -> str:
    ...

def reformat(text:str,
    filename:str='',
    logger:Logger | None=None,
) -> str:
    ...

# result is {line: [SyntaxRegion]}
def analyze_syntax_for_highlight(text:str) -> dict[int, list[SyntaxRegion]]:
    ...

class NodeType(IntEnum):
    MD           =  0
    DIV          =  1
    STRING       =  2
    PARA         =  3
    TITLE        =  4
    HEADING      =  5
    TABLE        =  6
    TABLE_ROW    =  7
    STYLESHEETS  =  8
    LINKS        =  9
    SCRIPTS      = 10
    IMPORT       = 11
    IMAGE        = 12
    BULLETS      = 13
    RAW          = 14
    PRE          = 15
    LIST         = 16
    LIST_ITEM    = 17
    KEYVALUE     = 18
    KEYVALUEPAIR = 19
    IMGLINKS     = 20
    TOC          = 21
    COMMENT      = 22
    CONTAINER    = 23
    QUOTE        = 24
    JS           = 25
    DETAILS      = 26
    META         = 27
    DEFLIST      = 28
    DEF          = 29
    INVALID      = 30

class Context:
    def __new__(cls, flags:Flags=Flags.NONE, filename:str|None=None, filecache:FileCache | None=None) -> Context:
        ...
    # This is wrong, it defines __new__ instead, but whatever
    # Autocomplete doesn't work without this.
    def __init__(cls, flags:Flags=Flags.NONE, filename:str|None=None, filecache:FileCache | None=None) -> None:
        ...
    errors: list[str]
    filename: str | None
    root: Node
    base_dir: str
    logger: Logger | None
    dependencies: set[str]
    flags: Flags

    def node_from_int(self, handle:int) -> Node:
        ...
    def node_by_id(self, id:str) -> Node | None:
        ...
    def node_by_approximate_location(self, filename:str, row:int, column:int=0) -> Node | None:
        ...
    def format_tree(self) -> str:
        ...
    def expand(self) -> str:
        ...
    def to_md(self) -> str:
        ...
    def render(self) -> str:
        ...
    def make_node(self, type:NodeType, header:str | None=None) -> Node:
        ...
    def resolve_imports(self) -> None:
        ...
    def execute_js(self, jsargs:Any=None) -> None:
        ...
    def resolve_links(self) -> None:
        ...
    def build_toc(self) -> None:
        ...
    def resolve_data_blocks(self) -> None:
        ...
    def select_nodes(self,
        type:NodeType|None=None,
        attributes:Iterable[str]|None=None,
        classes:Iterable[str]|None=None,
    ) -> list[Node] | None:
        ...
    def clone(self) -> Context:
        ...
    def pseudo_clone(self) -> Context:
        ...
    def add_link(self, key:str, value:str) -> None:
        ...
    def _to_json(self) -> str:
        ...

class Location(NamedTuple):
    filename: str
    row: int
    column: int

# has dict-like semantics
class Attributes:
    def __getitem__(self, key:str) -> str:
        ...
    def __setitem__(self, key:str, value:str) -> None:
        ...
    def __delitem__(self, key:str) -> None:
        ...
    def __iter__(self) -> Iterator[tuple[str, str]]:
        ...
    def __len__(self) -> int:
        ...
    def __contains__(self, key:str) -> bool:
        ...
    ctx: Context
    node: Node

# has set-like semantics
class Classes:
    def __contains__(self, cls:str) -> bool:
        ...
    def __len__(self) -> int:
        ...
    def __iter__(self) -> Iterator[str]:
        ...
    def add(self, cls:str) -> None:
        ...
    def discard(self, cls:str) -> None:
        ...
    ctx: Context
    node: Node

class Node:
    header: str
    type: NodeType
    id: str
    parent: Node
    children: Sequence[Node]
    location: Location
    classes: Classes
    attributes: Attributes
    import_: bool
    noid: bool
    hide: bool
    noinline: bool
    ctx: Context
    handle: int
    def execute_js(self, script:str) -> None:
        ...
    def parse(self, text:str, filename:str | None=None) -> None:
        ...
    def parse_file(self, path:str) -> None:
        ...
    def format(self, indent:int) -> str:
        ...
    def render(self) -> str:
        ...
    def append_child(self, child:Node|str) -> None:
        ...
    def insert_child(self, idx:int, child:Node|str) -> None:
        ...
    def detach(self) -> None:
        ...
    def make_child(self, type:NodeType, header:str | None=None) -> Node:
        ...
    def tree_repr(self) -> str:
        ...
    def _to_json(self) -> str:
        ...
    def __contains__(self, o:int|Node) -> bool:
        ...
    def __getitem__(self, idx:int) -> Node:
        ...
    def __len__(self) -> int:
        ...
    ...

def kebab(txt:str, /) -> str:
    ...

__version__: str
version: tuple[int, int, int]
INT_VERSION: int
