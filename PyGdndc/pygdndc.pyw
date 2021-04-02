#!/usr/bin/env python3
import os
os.environ["QT_AUTO_SCREEN_SCALE_FACTOR"] = "1"
import install_deps
have_deps = install_deps.ensure_deps(False)
import sys
if not have_deps:
    sys.exit(0)
from PySide2.QtWidgets import QApplication, QLabel, QMainWindow, QHBoxLayout, QPlainTextEdit, QWidget, QSplitter, QTabWidget, QAction, QFileDialog, QTextEdit, QFontDialog, QMessageBox
from PySide2.QtWebEngineWidgets import QWebEngineView, QWebEnginePage
from PySide2.QtGui import QFont, QKeySequence, QFontMetrics, QPainter, QColor, QTextFormat, QKeyEvent, QSyntaxHighlighter, QTextCharFormat, QImage
from PySide2.QtCore import Slot, Signal, QRect, QSize, Qt, QUrl, QStandardPaths, QSaveFile, QSettings
import pydndc
from typing import Optional, List, Dict, Optional
import time
import re
import textwrap
whitespace_re = re.compile(r'^\s+')

# https://tools.ietf.org/html/rfc6761 says we can use invalid. as a specially
# recognized app domain.
APPHOST = 'invalid.'

app = QApplication(sys.argv)
APPNAME = 'PyGdndc'
app.setApplicationName(APPNAME)
app.setApplicationDisplayName(APPNAME)
all_windows: Dict[str, 'Page'] = {}

FONT = QFont()
if sys.platform == 'win32':
    # Windows use 96 "ppi" whereas MacOS uses 72.
    # Use a smaller point size on windows or it looks way too big.
    pointsize = int(11*72/96)
else:
    pointsize = 11
FONT.setPointSize(pointsize)
FONT.setFixedPitch(True)
FONT.setFamilies(['Menlo','Cascadia Mono', 'Consolas','Ubuntu Mono', 'Mono'])
fontmetrics = QFontMetrics(FONT)
EIGHTYCHARS = fontmetrics.horizontalAdvance('M')*80
EDITOR_ON_LEFT = True

class DndMainWindow(QMainWindow):
    def __init__(self)->None:
        super().__init__()
        self.settings = QSettings('DavidTechnology', APPNAME)

    def restore_everything(self)->None:
        global EDITOR_ON_LEFT
        geometry = self.settings.value('window_geometry')
        if geometry is not None:
            self.restoreGeometry(geometry)
        else:
            self.showMaximized()
        on_left = self.settings.value('editor_on_left')
        if on_left is not None:
            EDITOR_ON_LEFT = on_left
        filenames = self.settings.value('filenames')
        if filenames:
            if isinstance(filenames, str):
                if os.path.isfile(filenames):
                    add_tab(filenames)
            else:
                for filename in filenames:
                    if not os.path.isfile(filename):
                        continue
                    add_tab(filename)

    def closeEvent(self, e) -> None:
        filenames = list(all_windows.keys())
        self.settings.setValue('filenames', filenames)
        self.settings.setValue('editor_on_left', EDITOR_ON_LEFT)
        self.settings.setValue('window_geometry', self.saveGeometry())
        for page in all_windows.values():
            page.save()
        e.accept()

window = DndMainWindow()
tabwidget = QTabWidget()
tabwidget.setTabsClosable(True)
def close_tab(index:int):
    page = tabwidget.widget(index)
    page.close()
    tabwidget.removeTab(index)

tabwidget.tabCloseRequested.connect(close_tab)
window.setCentralWidget(tabwidget)


class DndSyntaxHighlighter(QSyntaxHighlighter):
    def highlightBlock(self, text:str) -> None:
        doublecolon = text.find('::')
        if doublecolon == -1:
            return
        fmt = QTextCharFormat()
        color = QColor()
        color.setNamedColor('blue')
        fmt.setForeground(color)
        self.setFormat(0, doublecolon, fmt)
        color.setNamedColor('gray')
        fmt.setForeground(color)
        self.setFormat(doublecolon, 2, fmt)
        color.setNamedColor('darkgray')
        fmt.setForeground(color)
        self.setFormat(doublecolon+2, len(text)-doublecolon+1, fmt)

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
        self.highlight = DndSyntaxHighlighter(self.document())

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
    def insert_dnd_block(self, dndtext:str) -> None:
        block = self.textCursor().block()
        text = block.text()
        leading_space = re.match(whitespace_re, text)
        if leading_space:
            lead = leading_space[0]
            dndtext = textwrap.indent(dndtext, lead)
            if len(lead) == len(text):
                dndtext = dndtext.lstrip()
            else:
                dndtext = '\n' + dndtext
        elif text:
            dndtext = '\n' + dndtext
        self.insertPlainText(dndtext)

    def insert_image(self, fname:str) -> None:
        self.insert_dnd_block('::img\n  ' + fname + '\n')
    def insert_dnd(self, fname:str) -> None:
        self.insert_dnd_block('::import\n  ' + fname + '\n')
    def insert_css(self, fname:str) -> None:
        self.insert_dnd_block('::css\n  ' + fname + '\n')
    def insert_js(self, fname:str) -> None:
        self.insert_dnd_block('::js\n  ' + fname + '\n')
    def insert_image_links(self, fullname:str, fname:str) -> None:
        img = QImage()
        img.load(fullname)
        size = img.size()
        w = size.width()
        h = size.height()
        scale = 800/w if w > h else 800/h
        self.insert_dnd_block('\n'.join((
            '::imglinks',
            f'  {fname}',
            f'  width = {int(w*scale)}',
            f'  height = {int(h*scale)}',
            f'  viewBox = 0 0 {w} {h}',
            r"  ::python",
            r"    # this is an example of how to script the imglinks",
            r"    imglinks = node.parent",
            r"    coord_nodes = ctx.select_nodes(attributes=['coord'])",
            r"    for c in coord_nodes:",
            r"      lead = c.header  # change this probably",
            r"      position = c.attributes['coord']",
            r"      imglinks.add_child(f'{lead} = {ctx.outfile}#{c.id} @{position}')",
            )))


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
        left = EDITOR_ON_LEFT
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
    def save(self) -> None:
        if not self.filename:
            return
        savefile = QSaveFile(self)
        savefile.setFileName(self.filename)
        savefile.open(savefile.WriteOnly)
        text = self.textedit.toPlainText().encode('utf-8')
        if not text.endswith(b'\n'):
            text += b'\n'
        savefile.write(text)  # type: ignore
        savefile.commit()
    def insert_image(self) -> None:
        fname, _ = QFileDialog.getOpenFileName(None, 'Choose an image file', '', 'PNG images (*.png)')
        if not fname:
            return
        if self.dirname:
            try:
                relative = os.path.relpath(fname, self.dirname)
            except: # this can throw on Windows
                pass
            else:
                if '..' not in relative:
                    fname = relative
        self.textedit.insert_image(fname)
    def insert_image_links(self)-> None:
        fullname, _ = QFileDialog.getOpenFileName(None, 'Choose an image file', '', 'PNG images (*.png)')
        if not fullname:
            return
        if self.dirname:
            try:
                relative = os.path.relpath(fullname, self.dirname)
            except: # this can throw on Windows
                fname = fullname
            else:
                if '..' not in relative:
                    fname = relative
                else:
                    fname = fullname
        else:
            fname = fullname
        self.textedit.insert_image_links(fullname, fname)
    def insert_dnd(self) -> None:
        fname, _ = QFileDialog.getOpenFileName(None, 'Choose a dnd file', '', 'Dnd files (*.dnd)')
        if not fname:
            return
        if self.dirname:
            try:
                relative = os.path.relpath(fname, self.dirname)
            except: # this can throw on Windows
                pass
            else:
                if '..' not in relative:
                    fname = relative
        self.textedit.insert_dnd(fname)
    def insert_css(self) -> None:
        fname, _ = QFileDialog.getOpenFileName(None, 'Choose a css file', '', 'CSS files (*.css)')
        if not fname:
            return
        if self.dirname:
            try:
                relative = os.path.relpath(fname, self.dirname)
            except: # this can throw on Windows
                pass
            else:
                if '..' not in relative:
                    fname = relative
        self.textedit.insert_css(fname)
    def insert_js(self) -> None:
        fname, _ = QFileDialog.getOpenFileName(None, 'Choose a JavaScript file', '', 'JS files (*.js)')
        if not fname:
            return
        if self.dirname:
            try:
                relative = os.path.relpath(fname, self.dirname)
            except: # this can throw on Windows
                pass
            else:
                if '..' not in relative:
                    fname = relative
        self.textedit.insert_js(fname)
    def export_as_html(self):
        try:
            html = pydndc.htmlgen(self.textedit.toPlainText(), base_dir=self.dirname)
        except ValueError:
            mbox = QMessageBox()
            mbox.critical(None, 'Unable to convert current document', 'Unable to convert current document to html.\n\nSyntax Error in document (see error output).')
            return
        options = QFileDialog.Options()
        options |= QFileDialog.DontConfirmOverwrite
        if sys.platform == 'darwin':
            options |= QFileDialog.DontUseNativeDialog
        fname, _ = QFileDialog.getSaveFileName(None, 'Choose where to save html', '', 'HTML files (*.html)', initialFilter="*.html", options=options)  # type: ignore
        if not fname:
            return
        if not fname.endswith('.html'):
            fname += '.html'
        savefile = QSaveFile(self)
        savefile.setFileName(fname)
        savefile.open(savefile.WriteOnly)
        text = html.encode('utf-8')
        if not text.endswith(b'\n'):
            text += b'\n'
        savefile.write(text)  # type: ignore
        savefile.commit()


def make_page_widget(filename:str) -> Optional[QWidget]:
    if filename in all_windows:
        return None
    result = Page()
    try:
        text = open(filename).read()
    except:
        text = ''
    # Qt uses newlines as separators, not terminators.
    # We'll add a newline back when we save.
    if text.endswith('\n'):
        text = text[:-1]
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

def condense(filename:str, IS_WINDOWS=sys.platform=='win32') -> str:
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
    if sys.platform == 'win32':
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
    if sys.platform == 'darwin':
        options |= QFileDialog.DontUseNativeDialog
    fname, _ = QFileDialog.getSaveFileName(None, 'Choose or Create a dnd file', '', 'Dnd Files (*.dnd)', initialFilter="*.dnd", options=options)  # type: ignore
    if not fname:
        return
    add_tab(fname)

def save_file(*args) -> None:
    page = tabwidget.currentWidget()
    if not page:
        return
    page.save()

def export_file(*args) -> None:
    page: Optional[Page] = tabwidget.currentWidget()
    if not page:
        return
    page.export_as_html()


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
    global EDITOR_ON_LEFT
    if not all_windows:
        return
    if next(iter(all_windows.values())).editor_is_on_left:
        EDITOR_ON_LEFT = False
        for w in all_windows.values():
            w.put_editor_right()
    else:
        EDITOR_ON_LEFT = True
        for w in all_windows.values():
            w.put_editor_left()

def close_current_tab(*args) -> None:
    current_tab: Optional[Page] = tabwidget.currentWidget()
    if not current_tab:
        window.close()
        return
    current_tab.save()
    del all_windows[current_tab.filename]
    current_tab.setParent(None)  # type: ignore

def pickfont(*args) -> None:
    global FONT
    ok, font = QFontDialog.getFont(FONT)
    if ok:
        FONT = font
        for page in all_windows.values():
            page.textedit.setFont(FONT)

def insert_func(method):
    def insert_foo(*args) -> None:
        current_tab: Optional[Page] = tabwidget.currentWidget()
        if not current_tab:
            return
        method(current_tab)
    return insert_foo

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

    action = QAction('&Save', window)
    action.triggered.connect(save_file)
    action.setShortcut(QKeySequence('Ctrl+s'))
    filemenu.addAction(action)

    action = QAction('&Export As HTML', window)
    action.triggered.connect(export_file)
    action.setShortcut(QKeySequence('Ctrl+e'))
    filemenu.addAction(action)

    action = QAction('&Close', window)
    action.triggered.connect(close_current_tab)
    action.setShortcut(QKeySequence('Ctrl+w'))
    filemenu.addAction(action)

    if sys.platform != 'darwin':
        action = QAction('&Exit', window)
        action.triggered.connect(window.close)
        filemenu.addAction(action)

    editmenu = menubar.addMenu('Edit')

    action = QAction('&Format', window)
    action.triggered.connect(format_dnd)
    editmenu.addAction(action)

    action = QAction('F&ont', window)
    action.triggered.connect(pickfont)
    editmenu.addAction(action)

    insert = menubar.addMenu('Insert')

    action = QAction('&Image', window)
    action.triggered.connect(insert_func(Page.insert_image))
    insert.addAction(action)

    action = QAction('Image &Links', window)
    action.triggered.connect(insert_func(Page.insert_image_links))
    insert.addAction(action)

    action = QAction('&Dnd Import', window)
    action.triggered.connect(insert_func(Page.insert_dnd))
    insert.addAction(action)

    action = QAction('&JS', window)
    action.triggered.connect(insert_func(Page.insert_js))
    insert.addAction(action)

    action = QAction('&CSS', window)
    action.triggered.connect(insert_func(Page.insert_css))
    insert.addAction(action)

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
window.restore_everything()
if not tabwidget.currentWidget():
    open_file()
if not tabwidget.currentWidget():
    sys.exit(0)
window.show()
app.exec_()
