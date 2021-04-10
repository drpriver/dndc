from typing import Callable, Optional, Dict, Tuple, List

ErrorReporter = Optional[Callable[[int, str, int, int, str], None]]
SyntaxRegion = Tuple[int, int, int, int]

class FileCache:
    def remove(self, /, path:str) -> None:
        ...
    def clear(self) -> None:
        ...

def htmlgen(
    text:str,
    base_dir:str='.',
    error_reporter:ErrorReporter=None,
    file_cache:Optional[FileCache]=None
    ) -> Tuple[str, List[str]]:
    ...

def reformat(text:str, error_reporter:ErrorReporter=None) -> str:
    ...
def analyze_syntax_for_highlight(text:str) -> Dict[int, List[SyntaxRegion]]:
    ...

__version__: str
DOUBLE_COLON: int
HEADER: int
NODE_TYPE: int
ATTRIBUTE: int
ATTRIBUTE_ARGUMENT: int
CLASS: int
RAW_STRING: int
