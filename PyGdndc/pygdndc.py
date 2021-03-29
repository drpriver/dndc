import os
os.environ["QT_AUTO_SCREEN_SCALE_FACTOR"] = "1"
import sys
from PySide2.QtWidgets import QApplication, QLabel, QMainWindow, QHBoxLayout, QPlainTextEdit, QWidget, QSplitter, QTabWidget, QAction, QFileDialog, QTextEdit
from PySide2.QtWebEngineWidgets import QWebEngineView, QWebEnginePage
from PySide2.QtGui import QFont, QKeySequence, QFontMetrics, QPainter, QColor, QTextFormat, QKeyEvent
from PySide2.QtCore import Slot, Signal, QRect, QSize, Qt, QUrl
import pydndc
from typing import Optional, List, Dict, Optional
import time
import re
whitespace_re = re.compile(r'^\s+')

app = QApplication(sys.argv)
APPNAME = 'PyGdndc'
app.setApplicationName(APPNAME)
app.setApplicationDisplayName(APPNAME)
all_windows: Dict[str, 'Page'] = {}

FONT = QFont()
FONT.setPointSize(11)
FONT.setFixedPitch(True)
FONT.setFamilies(['Menlo','Cascadia Mono', 'Consolas',])
fontmetrics = QFontMetrics(FONT)
EIGHTYCHARS = fontmetrics.horizontalAdvance('M')*80

window = QMainWindow()
tabwidget = QTabWidget()
window.setCentralWidget(tabwidget)

# https://tools.ietf.org/html/rfc6761 says we can use invalid. as a specially
# recognized app domain.
APPHOST = 'invalid.'


class LineNumberArea(QWidget):
    def __init__(self, editor) -> None:
        super().__init__(editor)
        self.codeEditor = editor

    def sizeHint(self) -> QSize:
        return QSize(self.editor.lineNumberAreaWidth(), 0)

    def paintEvent(self, event) -> None:
        self.codeEditor.lineNumberAreaPaintEvent(event)

class DndEditor(QPlainTextEdit):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.lineNumberArea = LineNumberArea(self)
        self.blockCountChanged.connect(self.updateLineNumberAreaWidth)
        self.updateRequest.connect(self.updateLineNumberArea)
        self.cursorPositionChanged.connect(self.highlightCurrentLine)
        self.updateLineNumberAreaWidth(0)
        self.error_line = None

    def lineNumberAreaWidth(self) -> int:
        digits = 1
        max_value = max(1, self.blockCount())
        space = 3 + self.fontMetrics().horizontalAdvance(str(max_value))
        return space

    def updateLineNumberAreaWidth(self, _) -> None:
        self.setViewportMargins(self.lineNumberAreaWidth(), 0, 0, 0)

    def updateLineNumberArea(self, rect, dy) -> None:
        if dy:
            self.lineNumberArea.scroll(0, dy)
        else:
            self.lineNumberArea.update(0, rect.y(), self.lineNumberArea.width(), rect.height())
        if rect.contains(self.viewport().rect()):
            self.updateLineNumberAreaWidth(0)

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        cr = self.contentsRect()
        self.lineNumberArea.setGeometry(QRect(cr.left(), cr.top(), self.lineNumberAreaWidth(), cr.height()))

    def keyPressEvent(self, event:QKeyEvent) -> None:
        if event.key() == Qt.Key.Key_Tab:
            self.insertPlainText('  ')
            return
        if event.key() == Qt.Key.Key_Return or event.key() == Qt.Key.Key_Enter:
            block = self.textCursor().block()
            text = block.text()
            leading_space = re.match(whitespace_re, text)
            if leading_space:
                self.insertPlainText('\n' + leading_space[0])
            else:
                self.insertPlainText('\n')
            return
        if event.key() == Qt.Key.Key_Backspace:
            cursor = self.textCursor()
            block = cursor.block()
            text = block.text()
            position = cursor.positionInBlock()
            if (position & 1 ) != 1 and position >= 2:
                if text[position-1] == ' ' and text[position-2] == ' ':
                    cursor.deletePreviousChar()
                    cursor.deletePreviousChar()
                    return
        super().keyPressEvent(event)

    def highlightCurrentLine(self) -> None:
        return
        extraSelections = []
        if not self.isReadOnly():
            selection = QTextEdit.ExtraSelection()
            lineColor = QColor(Qt.yellow).lighter(160)
            selection.format.setBackground(lineColor)
            selection.format.setProperty(QTextFormat.FullWidthSelection, True)
            selection.cursor = self.textCursor()
            selection.cursor.clearSelection()
            extraSelections.append(selection)
        self.setExtraSelections(extraSelections)

    def lineNumberAreaPaintEvent(self, event) -> None:
        painter = QPainter(self.lineNumberArea)

        painter.fillRect(event.rect(), Qt.lightGray)

        block = self.firstVisibleBlock()
        blockNumber = block.blockNumber()
        cursor_number = self.textCursor().blockNumber()
        top = self.blockBoundingGeometry(block).translated(self.contentOffset()).top()
        bottom = top + self.blockBoundingRect(block).height()

        # Just to make sure I use the right font
        height = self.fontMetrics().height()
        while block.isValid() and (top <= event.rect().bottom()):
            if block.isVisible() and (bottom >= event.rect().top()):
                if blockNumber == cursor_number:
                    painter.fillRect(QRect(0, top, self.lineNumberArea.width(), height), Qt.yellow)  # type: ignore
                if blockNumber == self.error_line:
                    painter.fillRect(QRect(0, top, self.lineNumberArea.width(), height), Qt.red)  # type: ignore
                
                number = str(blockNumber + 1)
                painter.setPen(Qt.black)
                painter.drawText(0, top, self.lineNumberArea.width(), height, Qt.AlignRight, number)  # type: ignore

            block = block.next()
            top = bottom
            bottom = top + self.blockBoundingRect(block).height()
            blockNumber += 1

class DndWebPage(QWebEnginePage):
    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)
        self.basedir = ''

    def acceptNavigationRequest(self, url:QUrl, navtype:QWebEnginePage.NavigationType, isMainFrame:bool) -> bool:
        if url.scheme() == 'data':
            return True
        if navtype == QWebEnginePage.NavigationType.NavigationTypeLinkClicked:
            path = url.path()
            host = url.host()
            if host != APPHOST:
                return False
            if path.endswith('.html'):
                path = path.lstrip('/').replace('/', os.path.sep)
                filepath = os.path.join(self.basedir, path[:-len('.html')]+'.dnd')
                if os.path.isfile(filepath):
                    add_tab(filepath)
                return False
            return False
        return False
        result = super().acceptNavigationRequest(url, navtype, isMainFrame)
        return result

class Page(QSplitter):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.webpage = DndWebPage()
        self.web = QWebEngineView()
        self.web.setPage(self.webpage)
        self.webpage.setHtml(' ', baseUrl=QUrl(f'https://{APPHOST}/this.html'))
        # no idea why this is needed
        self.web.resize(400, 400)
        self.textedit = DndEditor('')
        self.textedit.setFont(FONT)
        self.textedit.setMinimumSize(EIGHTYCHARS*1.05, 200)  # type: ignore
        self.dirname = '.'
        self.textedit.textChanged.connect(self.update_html)
        self.error_display = QPlainTextEdit()
        self.error_display.setFont(FONT)
        self.editor_holder = QSplitter()
        self.editor_holder.setOrientation(Qt.Orientation().Vertical)
        self.editor_holder.addWidget(self.textedit)
        self.editor_holder.addWidget(self.error_display)
        self.editor_holder.setStretchFactor(0, 8)
        self.editor_holder.setStretchFactor(1, 1)
        self.filename = ''
        left = True
        show_errors = True
        self.show_errors = True
        self.editor_is_on_left = True
        if all_windows:
            first_window = next(iter(all_windows.values()))
            left = first_window.editor_is_on_left
            show_errors = first_window.show_errors
        if left:
            self.put_editor_left()
        else:
            self.put_editor_right()
        if show_errors:
            self.show_error()
        else:
            self.hide_error()

    def clear_errors(self):
        self.error_display.setPlainText('')
        self.textedit.error_line = None
    def display_dndc_error(self, error_type:int, filename:str, row:int, col:int, message:str) -> None:
        error_types = (
            'Error',
            'Warning',
            'System Error',
            'Info',
            )
        if error_type < 0 or error_type >= len(error_types):
            #TODO: logging
            return
        if error_type == 0:
            self.textedit.error_line = row
        et = error_types[error_type]
        self.error_display.appendPlainText(f'{et}:{row+1}:{col+1}: {message}')

    def update_html(self) -> None:
        position = self.textedit.textCursor().position()
        self.clear_errors()
        try:
            html = pydndc.htmlgen(self.textedit.toPlainText(), base_dir=self.dirname, error_reporter=self.display_dndc_error)
        except ValueError:
            return
        self.webpage.setHtml(html, baseUrl=QUrl(f'https://{APPHOST}/this.html'))

    def format(self) -> None:
        try:
            text = pydndc.reformat(self.textedit.toPlainText(), error_reporter=self.display_dndc_error)
        except ValueError:
            return
        self.textedit.setPlainText(text)
    def hide_editor(self) -> None:
        self.editor_holder.hide()
        
    def show_editor(self) -> None:
        self.editor_holder.show()
    def show_error(self) -> None:
        self.error_display.show()
        self.show_errors = True
    def hide_error(self) -> None:
        self.clear_errors()
        self.error_display.hide()
        self.show_errors = False
    def put_editor_right(self) -> None:
        self.editor_holder.setParent(None)  # type: ignore
        self.web.setParent(None)  # type: ignore
        self.addWidget(self.web)
        self.addWidget(self.editor_holder)
        self.editor_is_on_left = False
    def put_editor_left(self) -> None:
        self.editor_holder.setParent(None)  # type: ignore
        self.web.setParent(None)  # type: ignore
        self.addWidget(self.editor_holder)
        self.addWidget(self.web)
        self.editor_is_on_left = True
    
            
def make_page_widget(filename:str) -> Optional[QWidget]:
    if filename in all_windows:
        return None
    result = Page()
    try:
        text = open(filename).read()
    except:
        text = ''
    result.textedit.setPlainText(text)
    dirname = os.path.dirname(filename)
    result.dirname = dirname
    result.filename = filename
    result.webpage.basedir = dirname
    result.update_html()
    all_windows[filename]= result
    return result

def format_dnd(*args) -> None:
    current = tabwidget.currentWidget()
    current.format()

def condense(filename:str, IS_WINDOWS=sys.platform=='windows') -> str:
    BUDGET = 32
    sep = '\\' if IS_WINDOWS else '/'
    user = os.path.expanduser('~')
    if filename.startswith(user):
        filename = sep.join(['~', filename[len(user)+1:]])
    elif IS_WINDOWS:
        drive = filename[0]
    if len(filename) < BUDGET:
        return filename
    components = filename.split(sep)
    if IS_WINDOWS and filename[0] != '~':
        components = components[1:]
    parts = []
    budget = BUDGET
    comps = iter(reversed(components))
    first = next(comps)
    budget = BUDGET - len(first)
    parts.append(first)
    while budget > 0:
        try:
            p = next(comps)
        except StopIteration:
            break
        budget -= len(p)
        budget -= 1
        if budget > 0:
            parts.append(p)
        else:
            parts.append(p[0])
    while True:
        try:
            p = next(comps)
        except StopIteration:
            break
        parts.append(p[0])
    name = sep.join(reversed(parts))
    if IS_WINDOWS and name[0] != '~':
        name = drive + ':\\' + name
    return name

if 0:
    def print_condense(s, is_windows):
        print(f'          {s=}')
        print(f'{is_windows=}')
        print(f'{condense(s, is_windows)=}')
        print('-------------')

    for path in [
            '/Users/drpriver/Advanced_David_Dungeon_2nd_Edition/adventurerclass.md',
            r'C:\Users\David\Documents\Hello\World\This\Is\Long\Path\But this is a document.dnd',
            ]:
        print_condense(path, True)
        print_condense(path, False)
    exit(0)

def add_tab(filename:str, focus=True) -> None:
    if sys.platform == 'windows':
        filename = filename.replace('/', '\\')
    if filename in all_windows:
        if focus:
            tabwidget.setCurrentWidget(all_windows[filename])
        return
    page = make_page_widget(filename)
    if page is None:
        return
    tabwidget.addTab(page, condense(filename))
    if focus:
        tabwidget.setCurrentWidget(page)

def open_file(*args) -> None:
    fname, _ = QFileDialog.getOpenFileName(None, 'Choose a dnd file', '', 'Dnd Files (*.dnd)')
    if not fname:
        return
    add_tab(fname)

def new_file(*args) -> None:
    options = QFileDialog.Options()
    options |= QFileDialog.DontConfirmOverwrite
    options |= QFileDialog.DontUseNativeDialog
    fname, _ = QFileDialog.getSaveFileName(None, 'Choose or Create a dnd file', '', 'Dnd Files (*.dnd)', initialFilter="*.dnd", options=options)  # type: ignore
    if not fname:
        return
    add_tab(fname)

def toggle_editors(*args) -> None:
    if not all_windows:
        return
    if next(iter(all_windows.values())).editor_holder.isHidden():
        for w in all_windows.values():
            w.show_editor()
    else:
        for w in all_windows.values():
            w.hide_editor()

def toggle_errors(*args) -> None:
    if not all_windows:
        return
    if next(iter(all_windows.values())).show_errors:
        for w in all_windows.values():
            w.hide_error()
    else:
        for w in all_windows.values():
            w.show_error()

def flop_editors(*args) -> None:
    if not all_windows:
        return
    if next(iter(all_windows.values())).editor_is_on_left:
        for w in all_windows.values():
            w.put_editor_right()
    else:
        for w in all_windows.values():
            w.put_editor_left()

def add_menus() -> None:
    menubar = window.menuBar()

    filemenu = menubar.addMenu('File')

    action = QAction('&Open', window)
    action.triggered.connect(open_file)
    action.setShortcut(QKeySequence('Ctrl+o'))
    filemenu.addAction(action)

    action = QAction('&New', window)
    action.triggered.connect(new_file)
    action.setShortcut(QKeySequence('Ctrl+n'))
    filemenu.addAction(action)

    editmenu = menubar.addMenu('Edit')

    action = QAction('&Format', window)
    action.triggered.connect(format_dnd)
    editmenu.addAction(action)

    viewmenu = menubar.addMenu('View')

    action = QAction('&Toggle Editors', window)
    action.triggered.connect(toggle_editors)
    viewmenu.addAction(action)

    action = QAction('Toggle &Error', window)
    action.triggered.connect(toggle_errors)
    viewmenu.addAction(action)

    action = QAction('&Flop Editors', window)
    action.triggered.connect(flop_editors)
    viewmenu.addAction(action)

add_menus()
# add_tab('/Users/drpriver/Documents/Dungeons/BarrowMaze/index.dnd')
open_file()
if not tabwidget.currentWidget():
    sys.exit(0)
window.showMaximized()
app.exec_()
