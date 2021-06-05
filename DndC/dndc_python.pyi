# This file documents the api available within ::python blocks
from typing import List, Tuple, Mapping, Optional, Sequence, NoReturn, Union
class DndcNodeType:
    name: str
    value: int
# Actually just a simple namespace.
class NodeType:
    ROOT         : DndcNodeType
    TEXT         : DndcNodeType
    DIV          : DndcNodeType
    STRING       : DndcNodeType
    PARA         : DndcNodeType
    TITLE        : DndcNodeType
    HEADING      : DndcNodeType
    TABLE        : DndcNodeType
    TABLE_ROW    : DndcNodeType
    STYLESHEETS  : DndcNodeType
    DEPENDENCIES : DndcNodeType
    LINKS        : DndcNodeType
    SCRIPTS      : DndcNodeType
    IMPORT       : DndcNodeType
    IMAGE        : DndcNodeType
    BULLETS      : DndcNodeType
    PYTHON       : DndcNodeType
    RAW          : DndcNodeType
    PRE          : DndcNodeType
    LIST         : DndcNodeType
    LIST_ITEM    : DndcNodeType
    KEYVALUE     : DndcNodeType
    KEYVALUEPAIR : DndcNodeType
    IMGLINKS     : DndcNodeType
    NAV          : DndcNodeType
    DATA         : DndcNodeType
    COMMENT      : DndcNodeType
    MD           : DndcNodeType
    CONTAINER    : DndcNodeType
    QUOTE        : DndcNodeType
    HR           : DndcNodeType
    INVALID      : DndcNodeType

class Node:
    '''
    A node in the document.
    '''
    parent: 'Node'
    type: DndcNodeType
    children: Tuple['Node']
    header: str
    attributes: Mapping[str, Optional[str]]
    classes: Sequence[str]
    id: str
    def parse(self, text:str) -> None:
        '''
        Parses the given string as a dnd document, using this node as the root node (effectively appending the nodes as children of this node).

        Note that embedded python nodes will be queued to be executed.
        '''
        ...
    def detach(self) -> None:
        '''
        Removes this node from its parent, effectively making it an orphan. You should add it as the child of another node at some point.
        '''
        ...
    def add_child(self, child:Union['Node', str]) -> None:
        '''
        Adds an orphaned node as a child of this node.
        '''
        ...
    def err(self, msg:str) -> NoReturn:
        '''
        Issue an error message originating from this node (throws an exception).
        '''
        ...

class DndcContext:
    root: Node
    outfile: str
    outdir: str
    outpath: str
    sourcepath: str
    base: str
    all_nodes: List[Node]
    def set_root(self, new_root:Node) -> None:
        '''
        Replaces the current root node of the document with this node.
        '''
        ...
    def make_string(self, text:str) -> Node:
        '''
        Creates a new STRING node. Will be an orphan node.
        '''
        ...
    def make_node(self, 
        type: DndcNodeType, 
        header:str=None, 
        classes:Sequence[str]=None, 
        attributes:Sequence[str]=None) -> Node:
        '''
        Creates a new node. Will be an orphan node.
        '''
        ...
    def add_dependency(self, text:str) -> None:
        '''
        Adds the given string as a dependency (thus it will be present in for the make-style dependency file).
        '''
        ...
    def kebab(self, text:str) -> str:
        '''
        Converts the given string into a 'kebabed' string, which is the string that is suitable for use as an id in the resulting html document.
        '''
        ...
    def set_data(self, key:str, value:str) -> None:
        '''
        Sets a key, value in the resulting document's data blob.
        '''
        ...
    def select_nodes(self, type:DndcNodeType=None, classes:Sequence[str]=None, attributes:Sequence[str]=None) -> List[Node]:
        '''
        Selects nodes from the document, that are the intersection of the given queries. If no constraints are given, returns all nodes in the document.
        For example, type=NodeType.TABLE and classes=['random'] will select all tables with the .random class.
        '''
        ...
    def read_file(self, path:str) -> str:
        '''
        Reads the given file into a string.
        The path will be added to the dependencies.

        The path will be adjusted by the ctx's base, unless it's an absolute path.
        '''
        ...

ctx: DndcContext
node: Node
