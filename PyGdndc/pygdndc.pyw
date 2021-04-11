#!/usr/bin/env python3
import os
os.environ["QT_AUTO_SCREEN_SCALE_FACTOR"] = "1"
import install_deps
have_deps = install_deps.ensure_deps(False)
import sys
if not have_deps:
    sys.exit(0)
from PySide2.QtWidgets import QApplication, QLabel, QMainWindow, QHBoxLayout, QPlainTextEdit, QWidget, QSplitter, QTabWidget, QAction, QFileDialog, QTextEdit, QFontDialog, QMessageBox, QSplitterHandle, QCheckBox
from PySide2.QtWebEngineWidgets import QWebEngineView, QWebEnginePage, QWebEngineProfile
from PySide2.QtWebEngineCore import QWebEngineUrlScheme, QWebEngineUrlSchemeHandler, QWebEngineUrlRequestJob
from PySide2.QtGui import QFont, QKeySequence, QFontMetrics, QPainter, QColor, QTextFormat, QKeyEvent, QSyntaxHighlighter, QTextCharFormat, QImage, QDesktopServices, QContextMenuEvent, QDesktopServices
from PySide2.QtCore import Slot, Signal, QRect, QSize, Qt, QUrl, QStandardPaths, QSaveFile, QSettings, QObject, QEvent, QFileSystemWatcher, QFile
import pydndc
from typing import Optional, List, Dict, Optional, Callable, Tuple, Set
import time
import re
import textwrap
import sys
import logging
import datetime
import zipfile
import io
IS_WINDOWS = sys.platform == 'win32'
APPLOCAL = QStandardPaths.writableLocation(QStandardPaths.StandardLocation.AppLocalDataLocation)
if IS_WINDOWS:
    APPLOCAL = APPLOCAL.replace('/', '\\')
APPNAME = 'PyGdndc'
APPFOLDER = os.path.join(APPLOCAL, APPNAME)
LOGS_FOLDER = os.path.join(APPFOLDER, 'Logs')
os.makedirs(LOGS_FOLDER, exist_ok=True)
LOGFILE_LOCATION = os.path.join(LOGS_FOLDER, datetime.datetime.now().strftime('%Y-%m-%d.txt'))
PYGDNDC_VERSION = '0.4.1'
SCHEME = QWebEngineUrlScheme(b'dnd')  # type: ignore
SCHEME.setFlags(
        QWebEngineUrlScheme.Flag.SecureScheme
        | QWebEngineUrlScheme.Flag.LocalAccessAllowed # type: ignore
      )
SCHEME.setSyntax(QWebEngineUrlScheme.Syntax.Path)
QWebEngineUrlScheme.registerScheme(SCHEME)
class SCHEME_Handler(QWebEngineUrlSchemeHandler):
    def requestStarted(self, request:QWebEngineUrlRequestJob) -> None:
        if request.requestMethod() != b'GET':
            logger.debug(f'Not GET: {request.requestMethod()=}')
            request.fail(QWebEngineUrlRequestJob.Error.RequestDenied)
            return
        url = request.requestUrl()
        imgpath = url.path()
        if not os.path.isfile(imgpath):
            request.fail(QWebEngineUrlRequestJob.Error.UrlNotFound)
            return
        parts = imgpath.split('.')
        if parts:
            imgtype = parts[-1]
            types = {
                    'png' : b'image/png',
                    'jpg' : b'image/jpeg',
                    'jpeg': b'image/jpeg',
                    'gif' : b'image/gif',
                    }
            if imgtype in types:
                file = QFile(imgpath, request)
                request.reply(types[imgtype], file)  # type: ignore
                return
        request.fail(QWebEngineUrlRequestJob.Error().RequestDenied)


class Logs:
    def __init__(self) -> None:
        self.old_hook: Optional[Callable] = None
        try:
            self.stream = open(LOGFILE_LOCATION, 'a', encoding='utf-8')
        except:
            self.stream = sys.stderr
        self.logger = logging.getLogger('pygdndc')
        self.logger.setLevel(logging.DEBUG)
        handler = logging.StreamHandler(stream=self.stream)
        handler.setFormatter(logging.Formatter(
            fmt='[%(levelname)s] %(asctime)s L%(lineno)d: %(message)s',
            datefmt='%H:%M:%S',
            ))
        self.logger.addHandler(handler)
        self.error = self.logger.error
        self.info = self.logger.info
        self.warn = self.logger.warn
        self.debug = self.logger.debug
        self.exception = self.logger.exception
        self.info('New Session')
        self.info('pydndc: version is %s', pydndc.__version__)
        self.info('pygdndc: version is %s', PYGDNDC_VERSION)
    def hook(self, exctype, value, traceback) -> None:
        self.error('Uncaught exception', exc_info=(exctype, value, traceback))
        # self.old_hook(exctype, value, traceback)
    def install(self) -> None:
        self.old_hook = sys.excepthook
        sys.excepthook = self.hook
    def uninstall(self) -> None:
        if self.old_hook is not None:
            sys.excepthook = self.old_hook
    def close(self) -> None:
        self.stream.flush()
        self.stream.close()

logger = Logs()
logger.install()

whitespace_re = re.compile(r'^\s+')

# https://tools.ietf.org/html/rfc6761 says we can use invalid. as a specially
# recognized app domain.
APPHOST = 'invalid.'

app = QApplication(sys.argv)
app.setApplicationName(APPNAME)
app.setApplicationDisplayName(APPNAME)
handler = SCHEME_Handler()
QWebEngineProfile.defaultProfile().installUrlSchemeHandler(b'dnd', handler)  # type: ignore
all_windows: Dict[str, 'Page'] = {}

FONT = QFont()
if IS_WINDOWS:
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
PRINT_STATS = False
FILE_CACHE = pydndc.FileCache()

class DndMainWindow(QMainWindow):
    def __init__(self)->None:
        super().__init__()
        self.settings = QSettings('DavidTechnology', APPNAME)
        self.watcher = QFileSystemWatcher(self)
        self.watcher.fileChanged.connect(self.file_changed)

    def file_changed(self, path:str) -> None:
        if path.endswith('png'):
            QWebEngineProfile.defaultProfile().clearHttpCache()
        FILE_CACHE.remove(path)
        for page in all_windows.values():
            page.file_changed(path)

    def restore_everything(self)->None:
        global EDITOR_ON_LEFT
        geometry = self.settings.value('window_geometry')
        if geometry is not None:
            self.restoreGeometry(geometry) # type: ignore
        else:
            self.showMaximized()
        on_left = self.settings.value('editor_on_left')
        if on_left is not None:
            EDITOR_ON_LEFT = on_left # type: ignore
        filenames = self.settings.value('filenames')
        if filenames:
            if isinstance(filenames, str):
                if os.path.isfile(filenames):
                    add_tab(filenames)
            else:
                for filename in filenames:  # type: ignore
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
def close_tab(index:int) -> None:
    page = tabwidget.widget(index)
    page.save()
    page.close()
    tabwidget.removeTab(index)
    del all_windows[page.filename]
    page.setParent(None)  # type: ignore

tabwidget.tabCloseRequested.connect(close_tab)
window.setCentralWidget(tabwidget)

# this is stupid and slow and I hate it and hate everything about unicode
def byte_index_to_character_index(s:str, index:int) -> int:
    b = 0
    for n, c, in enumerate(s):
        if b == index:
            return n
        b += len(c.encode('utf-8'))
    return n


class DndSyntaxHighlighter(QSyntaxHighlighter):
    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)
        self.highlight_regions = {}  # type: Dict[int, List[Tuple[int, int, int, int]]]
        self.color_names = {
            pydndc.ATTRIBUTE: 'lightsteelblue',
            pydndc.ATTRIBUTE_ARGUMENT: 'darkkhaki',
            pydndc.CLASS: 'burlywood',
            pydndc.DOUBLE_COLON: 'darkgray',
            pydndc.HEADER: 'blue',
            pydndc.NODE_TYPE: 'lightslategray',
            # pydndc.RAW_STRING: '#000', # I should really break this up into more types
        }
    def update_regions(self, regions) -> None:
        self.highlight_regions = regions
    def highlightBlock(self, text:str) -> None:
        block = self.currentBlock()
        line = block.blockNumber()
        if line not in self.highlight_regions:
            return
        fmt = QTextCharFormat()
        color = QColor()
        names = self.color_names
        if len(text.encode('utf-8')) != len(text):
            for region in self.highlight_regions[line]:
                region_type, bytecol, _, bytelength = region
                if region_type not in names:
                    continue
                start = byte_index_to_character_index(text, bytecol)
                length = byte_index_to_character_index(text, bytecol+bytelength-1) - start + 1
                color.setNamedColor(names[region_type])
                fmt.setForeground(color)
                self.setFormat(start, length, fmt)
        else: # all ascii
            for region in self.highlight_regions[line]:
                region_type, bytecol, _, bytelength = region
                if region_type not in names:
                    continue
                color.setNamedColor(names[region_type])
                fmt.setForeground(color)
                self.setFormat(bytecol, bytelength, fmt)

class LineNumberArea(QWidget):
    def __init__(self, editor) -> None:
        super().__init__(editor)
        self.codeEditor = editor

    def sizeHint(self) -> QSize:
        return QSize(self.editor.lineNumberAreaWidth(), 0)

    def paintEvent(self, event) -> None:
        self.codeEditor.lineNumberAreaPaintEvent(event)

class DndEditor(QPlainTextEdit):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.lineNumberArea = LineNumberArea(self)
        self.blockCountChanged.connect(self.updateLineNumberAreaWidth)
        self.updateRequest.connect(self.updateLineNumberArea)
        self.cursorPositionChanged.connect(self.highlightCurrentLine)
        self.updateLineNumberAreaWidth(0)
        self.error_line: Optional[int] = None
        # Idk if this is guaranteed, but it is important that we can
        # update the syntax analysis before the highlighter
        # is called on a line.
        self.document().contentsChange.connect(self.update_syntax)
        self.highlight = DndSyntaxHighlighter(self.document())

    def update_syntax(self, *args) -> None:
        # t0 = time.time()
        new = pydndc.analyze_syntax_for_highlight(self.toPlainText())
        self.highlight.update_regions(new)
        # t1 = time.time()
        # print(f'update_syntax = {(t1-t0)*1000:.3f}ms')

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
            if self.isReadOnly():
                return
            if self.textCursor().hasSelection():
                self.alter_indent(indent=True)
                return
            self.insertPlainText('  ')
            return
        if event.key() == Qt.Key.Key_Return or event.key() == Qt.Key.Key_Enter:
            if self.isReadOnly():
                return
            block = self.textCursor().block()
            text = block.text()
            leading_space = re.match(whitespace_re, text)
            if leading_space:
                self.insertPlainText('\n' + leading_space[0])
            else:
                self.insertPlainText('\n')
            return
        if event.key() == Qt.Key.Key_Backspace:
            if self.isReadOnly():
                return
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
        if self.isReadOnly():
            return
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
    def alter_indent(self, indent:bool) -> None:
        if self.isReadOnly():
            return
        cursor = self.textCursor()
        start = cursor.selectionStart()
        end = cursor.selectionEnd()
        doc = self.document()
        first_block = doc.findBlock(start)
        end_block = doc.findBlock(end)
        block = first_block
        s = io.StringIO()
        for i in range(10000): # paranoia, use bounded loop instead of infinite loop in case of mistake
            if indent:
                s.write('  '); s.write(block.text()); s.write('\n')
            else:
                text = block.text()
                if text.startswith('  '):
                    text = text[2:]
                s.write(text); s.write('\n')
            if block.position() == end_block.position():
                break
            block = block.next()
        cursor.setPosition(first_block.position())
        cursor.setPosition(end_block.position() + len(end_block.text()), cursor.KeepAnchor)
        cursor.insertText(s.getvalue().rstrip())
    def contextMenuEvent(self, event:QContextMenuEvent) -> None:
        menu = self.createStandardContextMenu()
        action = QAction('Indent', menu)
        action.triggered.connect(lambda: self.alter_indent(True))
        action.setShortcut(QKeySequence('Ctrl+>'))
        menu.insertAction(menu.actions()[7], action)

        action = QAction('Dedent', menu)
        action.triggered.connect(lambda: self.alter_indent(False))
        action.setShortcut(QKeySequence('Ctrl+<'))
        menu.insertAction(menu.actions()[8], action)
        menu.exec_(event.globalPos())


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
                QDesktopServices.openUrl(url)
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

class SplitterHandler(QObject):
    def eventFilter(self, watched:QObject, event:QEvent) -> bool:
        if event.type() == QEvent.MouseButtonDblClick:
            handle: QSplitterHandle = watched
            if handle.splitter().widget(0).width():
                handle.splitter().setSizes([0, 1])
            else:
                handle.splitter().setSizes([1,10000])
            return True
        return False

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
        self.textedit.document().contentsChanged.connect(self.contents_changed)
        self.error_display = QPlainTextEdit()
        self.error_display.setFont(FONT)
        self.error_display.setReadOnly(True)
        self.editor_holder = QSplitter()
        self.editor_holder.setOrientation(Qt.Orientation().Vertical)
        self.editor_holder.addWidget(self.textedit)
        self.editor_holder.addWidget(self.error_display)
        self.checks = [
            QCheckBox('Auto-apply changes', self),
            QCheckBox('Read-only', self),
            ]
        self.checks[0].setCheckState(Qt.CheckState.Checked)
        self.auto_apply = True
        self.checks[0].stateChanged.connect(self.auto_apply_changed)
        self.checks[1].stateChanged.connect(self.read_only_changed)
        self.checkholder = QWidget(self)
        self.checkholder_layout = QHBoxLayout(self.checkholder)
        for check in self.checks:
            self.checkholder_layout.addWidget(check)
        self.checkholder_layout.addStretch()
        self.checkholder.setLayout(self.checkholder_layout)
        self.editor_holder.addWidget(self.checkholder)
        self.editor_holder.setStretchFactor(0, 16)
        self.editor_holder.setStretchFactor(1, 1)
        self.editor_holder.setStretchFactor(2, 1)
        self.filename = ''
        self.dependencies = set()  # type: Set[str]
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
        self.handle(1).installEventFilter(SplitterHandler(self))

    def contents_changed(self) -> None:
        if self.auto_apply:
            self.update_html()

    def auto_apply_changed(self, state:int) -> None:
        if state == Qt.CheckState.Unchecked:
            self.auto_apply = False
        if state == Qt.CheckState.Checked:
            self.auto_apply = True
            self.update_html()

    def read_only_changed(self, state:int) -> None:
        if state == Qt.CheckState.Unchecked:
            self.textedit.setReadOnly(False)
        if state == Qt.CheckState.Checked:
            self.textedit.setReadOnly(True)

    def file_changed(self, path:str) -> None:
        if path not in self.dependencies:
            return
        logger.debug("dependency '%s' changed", path)
        self.update_html()

    def clear_errors(self) -> None:
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
        if et == 'Info':
            self.error_display.appendPlainText(f'{et}: {message}')
        else:
            self.error_display.appendPlainText(f'{et}:{row+1}:{col+1}: {message}')

    def update_html(self) -> None:
        t0 = time.time()
        self.clear_errors()
        before_paths = set(FILE_CACHE.paths())
        try:
            flags = pydndc.USE_DND_URL_SCHEME
            if PRINT_STATS:
                flags |= pydndc.PRINT_STATS
            html, depends = pydndc.htmlgen(
                self.textedit.toPlainText(),
                base_dir=self.dirname,
                error_reporter=self.display_dndc_error,
                file_cache=FILE_CACHE,
                flags=flags,
                )
        except ValueError:
            # On error, the file cache can have loaded things, but we don't get those
            # dependencies.
            before = time.time()
            paths = FILE_CACHE.paths()
            for path in paths:
                if path not in before_paths:
                    window.watched.addPath(path)
            after = time.time()
            print(f'addPaths: {(after-before)*1000:.3f}ms')
            return
        t1 = time.time()
        self.webpage.setHtml(html, baseUrl=QUrl(f'https://{APPHOST}/this.html'))
        t2 = time.time()
        self.dependencies = set(depends)
        if depends:
            window.watcher.addPaths(depends)
        t3 = time.time()
        print(f'htmlgen = {(t1-t0)*1000:.3f}ms')
        print(f'sethtml = {(t2-t1)*1000:.3f}ms')
        print(f'total   = {(t3-t0)*1000:.3f}ms')

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
        logger.debug("Saving '%s'", self.filename)
        savefile = QSaveFile(self)
        savefile.setFileName(self.filename)
        savefile.open(savefile.WriteOnly)
        text = self.textedit.toPlainText().encode('utf-8')
        if not text.endswith(b'\n'):
            text += b'\n'
        savefile.write(text)  # type: ignore
        savefile.commit()
        logger.debug("Saved '%s'", self.filename)
        savefile = QSaveFile(self)
    def get_fname(self, title:str, filter:str)->Optional[str]:
        fname, _ = QFileDialog.getOpenFileName(None, title, '', filter)
        if not fname:
            return None
        if self.dirname:
            try:
                relative = os.path.relpath(fname, self.dirname)
            except: # this can throw on Windows
                pass
            else:
                if '..' not in relative:
                    fname = relative
        return fname
    def insert_image(self) -> None:
        if self.isReadOnly():
            return
        fname = self.get_fname('Choose an image file', 'PNG images (*.png)')
        if not fname:
            return
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
        fname = self.get_fname('Choose a dnd file', 'Dnd files (*.dnd)')
        if not fname:
            return
        self.textedit.insert_dnd(fname)
    def insert_css(self) -> None:
        fname = self.get_fname('Choose a css file', 'CSS files (*.css)')
        if not fname:
            return
        self.textedit.insert_css(fname)
    def insert_js(self) -> None:
        fname = self.get_fname('Choose a JavaScript file', 'JS files (*.js)')
        if not fname:
            return
        self.textedit.insert_js(fname)
    def export_as_html(self) -> None:
        try:
            html, _ = pydndc.htmlgen(self.textedit.toPlainText(), base_dir=self.dirname)
        except ValueError:
            mbox = QMessageBox()
            mbox.critical(None, 'Unable to convert current document', 'Unable to convert current document to html.\n\nSyntax Error in document (see error output).')  # type: ignore
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


def make_page_widget(filename:str, allow_fail:bool) -> Optional[QWidget]:
    if filename in all_windows:
        return None
    result = Page()
    try:
        fp = open(filename, 'r', encoding='utf-8')
    except:
        if not allow_fail:
            logger.debug("Failed to open: '%s'", filename)
            return None
        text = ''
    else:
        try:
            text = fp.read()
        except Exception as e:
            logger.exception('Problem when reading text file')
            fp.close()
            error_message = f'Unable to read data from {filename}'
            if isinstance(e, UnicodeDecodeError):
                error_message += '\n' + 'The file contains invalid utf-8 data.\nConvert the file to utf-8 first (Notepad can do this)'
            QMessageBox.critical(window, 'Problem when reading file', error_message)
            return None
        else:
            fp.close()
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

def condense(filename:str, is_windows=IS_WINDOWS) -> str:
    BUDGET = 32
    sep = '\\' if is_windows else '/'
    user = os.path.expanduser('~')
    if filename.startswith(user):
        filename = sep.join(['~', filename[len(user)+1:]])
    elif is_windows:
        drive = filename[0]
    if len(filename) < BUDGET:
        return filename
    components = filename.split(sep)
    if is_windows and filename[0] != '~':
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
    if is_windows and name[0] != '~':
        name = drive + ':\\' + name
    return name

def add_tab(filename:str, focus=True, allow_fail:bool=False) -> None:
    if sys.platform == 'win32':
        filename = filename.replace('/', '\\')
    logger.debug("adding_tab: '%s'", filename)
    if filename in all_windows:
        if focus:
            tabwidget.setCurrentWidget(all_windows[filename])
        return
    page = make_page_widget(filename, allow_fail)
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

def add_menus() -> None:
    menubar = window.menuBar()

    filemenu = menubar.addMenu('File')

    action = QAction('&Open', window)
    action.triggered.connect(open_file)
    action.setShortcut(QKeySequence('Ctrl+o'))
    filemenu.addAction(action)

    def new_file(*args) -> None:
        options = QFileDialog.Options()
        options |= QFileDialog.DontConfirmOverwrite
        if sys.platform == 'darwin':
            options |= QFileDialog.DontUseNativeDialog
        fname, _ = QFileDialog.getSaveFileName(None, 'Choose or Create a dnd file', '', 'Dnd Files (*.dnd)', initialFilter="*.dnd", options=options)  # type: ignore
        if not fname:
            return
        add_tab(fname, allow_fail=True)
    action = QAction('&New', window)
    action.triggered.connect(new_file)
    action.setShortcut(QKeySequence('Ctrl+n'))
    filemenu.addAction(action)

    def save_file(*args) -> None:
        page = tabwidget.currentWidget()
        if page:
            page.save()
    action = QAction('&Save', window)
    action.triggered.connect(save_file)
    action.setShortcut(QKeySequence('Ctrl+s'))
    filemenu.addAction(action)

    def export_file(*args) -> None:
        page: Optional[Page] = tabwidget.currentWidget()
        if page: page.export_as_html()
    action = QAction('&Export As HTML', window)
    action.triggered.connect(export_file)
    action.setShortcut(QKeySequence('Ctrl+e'))
    filemenu.addAction(action)

    def close_current_tab(*args) -> None:
        current_tab: Optional[Page] = tabwidget.currentWidget()
        if not current_tab:
            window.close()
            return
        current_tab.save()
        del all_windows[current_tab.filename]
        current_tab.setParent(None)  # type: ignore
    action = QAction('&Close', window)
    action.triggered.connect(close_current_tab)
    action.setShortcut(QKeySequence('Ctrl+w'))
    filemenu.addAction(action)

    if sys.platform != 'darwin':
        action = QAction('&Exit', window)
        action.triggered.connect(window.close)
        filemenu.addAction(action)

    editmenu = menubar.addMenu('Edit')

    def format_dnd(*args) -> None:
        current_tab: Optional[Page] = tabwidget.currentWidget()
        if not current_tab:
            return
        current_tab.format()
    action = QAction('&Format', window)
    action.triggered.connect(format_dnd)
    editmenu.addAction(action)

    def pickfont(*args) -> None:
        global FONT
        ok, font = QFontDialog.getFont(FONT)
        if ok:
            FONT = font
            for page in all_windows.values():
                page.textedit.setFont(FONT)
    action = QAction('F&ont', window)
    action.triggered.connect(pickfont)
    editmenu.addAction(action)

    def indent(*args) -> None:
        current_tab: Optional[Page] = tabwidget.currentWidget()
        if not current_tab:
            return
        current_tab.textedit.alter_indent(indent=True)
    action = QAction('&Indent', window)
    action.setShortcut(QKeySequence('Ctrl+>'))
    action.triggered.connect(indent)
    editmenu.addAction(action)

    def dedent(*args) -> None:
        current_tab: Optional[Page] = tabwidget.currentWidget()
        if not current_tab:
            return
        current_tab.textedit.alter_indent(indent=False)
    action = QAction('&Dedent', window)
    action.setShortcut(QKeySequence('Ctrl+<'))
    action.triggered.connect(dedent)
    editmenu.addAction(action)

    insert = menubar.addMenu('Insert')
    def insert_func(method):
        def insert_foo(*args) -> None:
            current_tab: Optional[Page] = tabwidget.currentWidget()
            if not current_tab:
                return
            method(current_tab)
        return insert_foo

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

    def toggle_editors(*args) -> None:
        if not all_windows:
            return
        if next(iter(all_windows.values())).editor_holder.isHidden():
            for w in all_windows.values():
                w.show_editor()
        else:
            for w in all_windows.values():
                w.hide_editor()
    action = QAction('&Toggle Editors', window)
    action.triggered.connect(toggle_editors)
    viewmenu.addAction(action)

    def toggle_errors(*args) -> None:
        if not all_windows:
            return
        if next(iter(all_windows.values())).show_errors:
            for w in all_windows.values():
                w.hide_error()
        else:
            for w in all_windows.values():
                w.show_error()
    action = QAction('Toggle &Error', window)
    action.triggered.connect(toggle_errors)
    viewmenu.addAction(action)

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
    action = QAction('&Flop Editors', window)
    action.triggered.connect(flop_editors)
    viewmenu.addAction(action)

    def refresh_highlight(*args) -> None:
        current_tab: Optional[Page] = tabwidget.currentWidget()
        if not current_tab:
            return
        current_tab.textedit.highlight.rehighlight()

    action = QAction('&Refresh Highlighting', window)
    action.triggered.connect(refresh_highlight)
    viewmenu.addAction(action)

    helpmenu = menubar.addMenu('Help')
    def show_version(*args) -> None:
        QMessageBox.about(window, 'Version',
                f'GUI version: {PYGDNDC_VERSION}\n'
                f'dndc version: {pydndc.__version__}\n')
    action = QAction('&Version', window)
    action.triggered.connect(show_version)
    helpmenu.addAction(action)

    def open_log_folder(*args) -> None:
        if IS_WINDOWS:
            url = QUrl('file:///' + LOGS_FOLDER.replace('\\', '/'))
        else:
            url = QUrl('file://'+LOGS_FOLDER)
        success = QDesktopServices.openUrl(url)
        if not success:
            logger.error("Failed to open: '%s'", url)
    action = QAction('&Open Logs Folder', window)
    action.triggered.connect(open_log_folder)
    helpmenu.addAction(action)

    def compress_logs(*args) -> None:
        with zipfile.ZipFile(LOGFILE_LOCATION+'.zip', compression=zipfile.ZIP_DEFLATED, mode='w') as z:
            z.write(LOGFILE_LOCATION)
        if IS_WINDOWS:
            url = QUrl('file:///' + LOGS_FOLDER.replace('\\', '/'))
        else:
            url = QUrl('file://'+LOGS_FOLDER)
        success = QDesktopServices.openUrl(url)
        if not success:
            logger.error("Failed to open: '%s'", url)
    action = QAction('&Compress Logs', window)
    action.triggered.connect(compress_logs)
    helpmenu.addAction(action)

    developmenu = menubar.addMenu('Developer')

    def clear_caches(*args) -> None:
        FILE_CACHE.clear()
        QWebEngineProfile.defaultProfile().clearHttpCache()
        for window in all_windows.values():
            window.update_html()
    action = QAction('&Clear Caches', window)
    action.triggered.connect(clear_caches)
    developmenu.addAction(action)

    def recalculate_html(*args) -> None:
        for window in all_windows.values():
            window.update_html()
    action = QAction('&Recalculate HTML', window)
    action.triggered.connect(recalculate_html)
    developmenu.addAction(action)

    def toggle_timings(*args) -> None:
        global PRINT_STATS
        PRINT_STATS = not PRINT_STATS
    action = QAction('&Toggle Timings', window)
    action.triggered.connect(toggle_timings)
    developmenu.addAction(action)
    return

add_menus()
window.restore_everything()
if not tabwidget.currentWidget():
    open_file()
if not tabwidget.currentWidget():
    logger.info('Exiting due to user canceling open file')
    logger.close()
    sys.exit(0)
window.show()
app.exec_()
logger.info('Exiting normally')
logger.close()
