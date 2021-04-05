from typing import Callable, Optional, Dict, Tuple, List
ErrorReporter = Optional[Callable[[int, str, int, int, str], None]]
def htmlgen(text:str, base_dir:str='.', error_reporter:ErrorReporter=None) -> str: ...
def reformat(text:str, error_reporter:ErrorReporter=None) -> str: ...
def analyze_syntax_for_highlight(text:str) -> Dict[int, List[Tuple[int, int, int, int]]]: ...
__version__: str
DOUBLE_COLON: int
HEADER: int
NODE_TYPE: int
ATTRIBUTE: int
ATTRIBUTE_ARGUMENT: int
CLASS: int
RAW_STRING: int
