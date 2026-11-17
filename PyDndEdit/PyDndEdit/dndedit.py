#!/usr/bin/env python3
from __future__ import annotations

#
# Copyright © 2021-2024, David Priver <david@davidpriver.com>
#
PYGDNDC_VERSION = '1.1.0'
__version__ = PYGDNDC_VERSION
import os
import html as html_escape
# os.environ["QT_AUTO_SCREEN_SCALE_FACTOR"] = "1"
os.environ['QT_ENABLE_HIGHDPI_SCALING'] = '1'
import sys
from PySide6.QtWidgets import QApplication, QLabel, QMainWindow, QHBoxLayout, QPlainTextEdit, QWidget, QSplitter, QTabWidget, QFileDialog, QTextEdit, QTextBrowser, QFontDialog, QMessageBox, QSplitterHandle, QCheckBox, QToolButton, QTabBar
from PySide6.QtWebEngineWidgets import QWebEngineView
from PySide6.QtWebEngineCore import QWebEngineUrlScheme, QWebEngineUrlSchemeHandler, QWebEngineUrlRequestJob, QWebEnginePage, QWebEngineProfile
from PySide6.QtGui import QFont, QKeySequence, QFontMetrics, QPainter, QColor, QTextFormat, QKeyEvent, QSyntaxHighlighter, QTextCharFormat, QImage, QDesktopServices, QContextMenuEvent, QDesktopServices, QCloseEvent, QAction, QTextCursor, QMouseEvent, QPaintEvent
from PySide6.QtCore import Slot, Signal, QRect, QSize, Qt, QUrl, QStandardPaths, QSaveFile, QSettings, QObject, QEvent, QFileSystemWatcher, QFile, QThread, QTimer, QIODevice, QIODeviceBase
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
QApplication.setAttribute(Qt.ApplicationAttribute.AA_EnableHighDpiScaling)
APPNAME = 'DndEdit'
APP = QApplication(sys.argv)
APP.setApplicationName(APPNAME)
APP.setApplicationDisplayName(APPNAME)

IS_WINDOWS = sys.platform == 'win32'
APPLOCAL = QStandardPaths.writableLocation(QStandardPaths.StandardLocation.AppLocalDataLocation)
if IS_WINDOWS:
    APPLOCAL = APPLOCAL.replace('/', '\\')
APPFOLDER = os.path.join(APPLOCAL, APPNAME)
LOGS_FOLDER = os.path.join(APPFOLDER, 'Logs')
os.makedirs(LOGS_FOLDER, exist_ok=True)
LOGFILE_LOCATION = os.path.join(LOGS_FOLDER, datetime.datetime.now().strftime('%Y-%m-%d.txt'))
SCHEME = QWebEngineUrlScheme(b'dnd')
SCHEME.setFlags(
        QWebEngineUrlScheme.Flag.SecureScheme
        | QWebEngineUrlScheme.Flag.LocalAccessAllowed
        | QWebEngineUrlScheme.Flag.CorsEnabled
      )
SCHEME.setSyntax(QWebEngineUrlScheme.Syntax.Path)
QWebEngineUrlScheme.registerScheme(SCHEME)

def append_room_with_name_at(name:str, x:int, y:int) -> None:
    page: Optional[Page] = get_current_tab()
    if not page:
        return
    if page.textedit.isReadOnly():
        return
    if '.' not in name:
        name = name + '.'
    page.textedit.appendPlainText(f'\n{name} ::md .room @coord({x}, {y})\n')
    page.textedit.setFocus(Qt.FocusReason.NoFocusReason)

def change_coord(id:int, x:int, y:int) -> None:
    page: Optional[Page] = get_current_tab()
    if not page: return
    if page.textedit.isReadOnly(): return
    text = page.textedit.toPlainText()
    try:
        ctx = pydndc.Context()
        ctx.root.parse(text)
        ctx.node_from_int(id).attributes['coord'] =  f'{x},{y}'
        text = ctx.format_tree()
        page.textedit.replace_source(text)
    except:
        LOGGER.exception('Problem while doing change coord')

def scroll_to_id(nid:str) -> None:
    page: Optional[Page] = get_current_tab()
    if not page: return
    text = page.textedit.toPlainText()
    try:
        ctx = pydndc.Context()
        ctx.root.parse(text)
        n = ctx.node_by_id(nid)
        if n is None: return
        loc:pydndc.Location = n.location
        page.scroll_to_line(loc.row)

    except:
        LOGGER.exception('Problem while doing scroll_to_id')


class SCHEME_Handler(QWebEngineUrlSchemeHandler):
    def requestStarted(self, request:QWebEngineUrlRequestJob) -> None:
        if request.requestMethod() == b'PUT':
            path = request.requestUrl().path()
            components = path.split('/')
            try:
                if components[1] == 'roommove':
                    parts = components[-1].split('.')
                    if len(parts) != 3: return
                    id = int(parts[0])
                    x = int(parts[1])
                    y = int(parts[2])
                    QTimer.singleShot(0, lambda: change_coord(id, x, y))
                    return
                elif components[1] == 'roomclick':
                    if len(components) != 4:
                        return
                    name = components[2]
                    parts = components[3].split('.')
                    if len(parts) != 2: return
                    x = int(parts[0])
                    y = int(parts[1])
                    QTimer.singleShot(0, lambda: append_room_with_name_at(name, x, y))
                    return
                elif components[1] == 'scrolltoid':
                    if len(components) != 3:
                        return
                    nid = components[2]
                    QTimer.singleShot(0, lambda: scroll_to_id(nid))
            except:
                LOGGER.exception('Unable to handle PUT from: %r', path)
            return
        if request.requestMethod() != b'GET':
            LOGGER.debug(f'Not GET: {request.requestMethod()=}')
            request.fail(QWebEngineUrlRequestJob.Error.RequestDenied)
            return
        url = request.requestUrl()
        imgpath = url.path()
        if not os.path.isfile(imgpath):
            LOGGER.debug('imgpath does not exist: %s', imgpath)
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
                request.reply(types[imgtype], file)
                return
        request.fail(QWebEngineUrlRequestJob.Error.RequestDenied)


class Logs:
    def __init__(self) -> None:
        self.old_hook: Optional[Callable] = None
        self.stream = sys.stderr
        try:
            self.stream = open(LOGFILE_LOCATION, 'a', encoding='utf-8')
        except:
            pass
        self.LOGGER = logging.getLogger('DndEdit')
        self.LOGGER.setLevel(logging.DEBUG)
        handler = logging.StreamHandler(stream=self.stream)
        handler.setFormatter(logging.Formatter(
            fmt='[%(levelname)s] %(asctime)s L%(lineno)d: %(message)s',
            datefmt='%H:%M:%S',
            ))
        self.LOGGER.addHandler(handler)
        self.error = self.LOGGER.error
        self.info = self.LOGGER.info
        self.warn = self.LOGGER.warn
        self.debug = self.LOGGER.debug
        self.exception = self.LOGGER.exception
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

LOGGER = Logs()
LOGGER.install()

whitespace_re = re.compile(r'^\s+')

# https://tools.ietf.org/html/rfc6761 says we can use invalid. as a specially
# recognized app domain.
APPHOST = 'invalid.'


handler = SCHEME_Handler()
QWebEngineProfile.defaultProfile().installUrlSchemeHandler(b'dnd', handler)
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
fonts = []
if sys.platform == 'darwin':
    fonts.append('Menlo')
elif sys.platform == 'win32':
    fonts.extend(['Cascadia Mono', 'Consolas'])
else:
    fonts.append('Ubuntu Mono')
FONT.setFamilies(fonts)
FONTMETRICS = QFontMetrics(FONT)
EIGHTYCHARS = FONTMETRICS.horizontalAdvance('M')*80
PRINT_STATS = False
FILE_CACHE = pydndc.FileCache()

# A document-owned editor window, attached to its preview window.
if sys.platform == 'darwin':
    import ctypes
    _objc = ctypes.CDLL('/usr/lib/libobjc.A.dylib')
    _objc.sel_registerName.argtypes = [ctypes.c_char_p]
    _objc.sel_registerName.restype = ctypes.c_void_p
    _address = ctypes.cast(_objc.objc_msgSend, ctypes.c_void_p).value

    def _send(receiver, selector, restype, argtypes=(), args=()):
        call = ctypes.CFUNCTYPE(restype, ctypes.c_void_p, ctypes.c_void_p, *argtypes)(_address)
        return call(receiver, _objc.sel_registerName(selector), *args)

    def _native_window(widget):
        # Qt's Cocoa winId is the NSView, whose window owns it.
        return _send(int(widget.winId()), b'window', ctypes.c_void_p)


class EditorWindow(QMainWindow):
    def __init__(self, page, owner):
        super().__init__(owner, Qt.WindowType.Window | Qt.WindowType.CustomizeWindowHint
                         | Qt.WindowType.WindowTitleHint | Qt.WindowType.WindowCloseButtonHint)
        self.page = page
        self.owner = owner
        self.attached = False
        self.disposed = False
        self.owner_position = owner.pos()
        self.setAttribute(Qt.WidgetAttribute.WA_QuitOnClose, False)
        self.setCentralWidget(page.editor_holder)
        self.setWindowTitle(page.filename + ' — Editor[*]')
        self.setWindowModified(page.textedit.document().isModified())
        page.textedit.document().modificationChanged.connect(self.setWindowModified)
        self.resize(800, 1000)
        screen = owner.screen().availableGeometry()
        self.resize(min(self.width(), screen.width()), min(self.height(), screen.height()))
        self.move(screen.center() - self.rect().center())
        self.visibility_timer = QTimer(self)
        self.visibility_timer.setSingleShot(True)
        self.visibility_timer.timeout.connect(self.sync_visibility)
        # Never filter every application event in Python: WebEngine's internal
        # Qt Quick objects may be in destruction when Shiboken wraps the receiver.
        owner.installEventFilter(self)
        escape = QAction(self)
        escape.setShortcut(QKeySequence(Qt.Key.Key_Escape))
        escape.setShortcutContext(Qt.ShortcutContext.WidgetWithChildrenShortcut)
        escape.triggered.connect(page.hide_editor)
        self.addAction(escape)

    def attach(self):
        if sys.platform == 'darwin' and not self.attached:
            _send(_native_window(self.owner), b'addChildWindow:ordered:', None,
                  (ctypes.c_void_p, ctypes.c_long), (_native_window(self), 1))
            self.attached = True

    def detach(self):
        if sys.platform == 'darwin' and self.attached:
            _send(_native_window(self.owner), b'removeChildWindow:', None,
                  (ctypes.c_void_p,), (_native_window(self),))
            self.attached = False

    def sync_visibility(self, focus=False):
        if self.disposed:
            return
        visible = (self.page.editor_open and self.page.isVisible()
                   and self.owner.isVisible() and not self.owner.isMinimized())
        if visible:
            if not self.isVisible():
                self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating, not focus)
                # A monitor may have disappeared while the editor was hidden.
                if not any(screen.availableGeometry().intersects(self.frameGeometry())
                           for screen in QApplication.screens()):
                    area = self.owner.screen().availableGeometry()
                    self.move(area.center() - self.rect().center())
                self.show()
                self.attach()
            if focus:
                self.raise_()
                self.activateWindow()
                self.page.textedit.setFocus()
        else:
            self.detach()
            self.hide()

    def closeEvent(self, event):
        self.page.hide_editor()
        event.ignore()  # Closing the editor does not close or destroy the document.

    def eventFilter(self, watched, event):
        if watched is self.owner:
            if event.type() == QEvent.Type.Move and sys.platform != 'darwin':
                # Move hidden editors too, so their remembered offsets stay correct.
                if not self.owner.isMinimized():
                    self.move(self.pos() + self.owner.pos() - self.owner_position)
                    self.owner_position = self.owner.pos()
            elif event.type() in (QEvent.Type.Hide, QEvent.Type.Show, QEvent.Type.WindowStateChange):
                self.visibility_timer.start(0)
            elif event.type() == QEvent.Type.WindowActivate and self.isVisible():
                self.raise_()  # Preserve focus in the preview.
        return super().eventFilter(watched, event)

    def dispose(self):
        self.disposed = True
        self.visibility_timer.stop()
        self.detach()
        self.hide()
        self.owner.removeEventFilter(self)
        self.deleteLater()


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
        # Atomic replacement can remove the watch on the previous file.
        if os.path.isfile(path) and path not in self.watcher.files():
            self.watcher.addPath(path)
        for page in all_windows.values():
            page.file_changed(path)

    def restore_everything(self)->None:
        geometry = self.settings.value('window_geometry')
        if geometry is not None:
            self.restoreGeometry(geometry) # type: ignore
        else:
            self.showMaximized()
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
        timer = QTimer(self)
        timer.timeout.connect(self.save_windows)
        timer.start(10000)
    def save_windows(self, *args) -> None:
        LOGGER.debug('Saving Window Positions and Settings.')
        filenames = list(all_windows.keys())
        self.settings.setValue('filenames', filenames)
        self.settings.setValue('window_geometry', self.saveGeometry())

    def closeEvent(self, e:QCloseEvent) -> None:
        self.save_windows()
        for page in all_windows.values():
            if not page.save():
                e.ignore()
                return
        for page in all_windows.values():
            page.dispose_editor()
        e.accept()

class DndTabBar(QTabBar):
    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.MiddleButton:
            self.tabCloseRequested.emit(self.tabAt(event.pos()))
        else:
            super().mouseReleaseEvent(event)

WINDOW = DndMainWindow()
TABWIDGET = QTabWidget()
def get_current_tab() -> Optional[Page]:
    active = APP.activeWindow()
    if isinstance(active, EditorWindow) and active.page in all_windows.values():
        return active.page
    current_tab = TABWIDGET.currentWidget()
    if not current_tab: return None
    assert isinstance(current_tab, Page)
    return current_tab

TABWIDGET.setTabBar(DndTabBar())
TABWIDGET.setDocumentMode(True)
TABWIDGET.setTabsClosable(True)
def close_tab(index:int) -> None:
    page: Page = TABWIDGET.widget(index) # type: ignore
    if not page or not page.save():
        return
    page.dispose_editor()
    del all_windows[page.filename]
    TABWIDGET.removeTab(index)
    page.deleteLater()

def sync_editors(index=None):
    current = TABWIDGET.currentWidget()
    for page in all_windows.values():
        if page is not current and page.editor_window:
            page.editor_window.sync_visibility()
    if current and current.editor_window:
        current.editor_window.sync_visibility(APP.applicationState() == Qt.ApplicationState.ApplicationActive)

TABWIDGET.currentChanged.connect(sync_editors)
TABWIDGET.tabCloseRequested.connect(close_tab)
WINDOW.setCentralWidget(TABWIDGET)

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
        self.highlight_regions = {}  # type: Dict[int, List[pydndc.SyntaxRegion]]
        SynType = pydndc.SynType
        self.color_names = {
            SynType.ATTRIBUTE: 'lightsteelblue',
            SynType.DIRECTIVE: 'lightsteelblue',
            SynType.ATTRIBUTE_ARGUMENT: 'darkkhaki',
            SynType.CLASS: 'burlywood',
            SynType.DOUBLE_COLON: 'darkgray',
            SynType.HEADER: 'blue',
            SynType.NODE_TYPE: 'lightslategray',
            # SynType.RAW_STRING: '#000', # I should really break this up into more types
            SynType.JS_COMMENT: 'gray',
            SynType.JS_STRING: '#74AB04',
            SynType.JS_REGEX: 'darkred',
            SynType.JS_NUMBER: '#74AB04',
            SynType.JS_KEYWORD: '#0D85CC',
            SynType.JS_KEYWORD_VALUE: '#74AB04',
            SynType.JS_VAR: '#0D85CC',
            # SynType.JS_IDENTIFIER: ''
            SynType.JS_BUILTIN: '#AE6000',
            SynType.JS_NODETYPE: '#AE6000',
            # SynType.JS_BRACE: 'darkteal',
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
    def __init__(self, editor: DndEditor) -> None:
        super().__init__(editor)
        self.codeEditor = editor

    def sizeHint(self) -> QSize:
        return QSize(self.codeEditor.lineNumberAreaWidth(), 0)

    def paintEvent(self, event: QPaintEvent) -> None:
        self.codeEditor.lineNumberAreaPaintEvent(event)

COORD_HELPER_SCRIPT= (
    "\n"
    "::script\n"
    "  document.addEventListener(\"DOMContentLoaded\", function(){\n"
    "    const svgs = document.getElementsByTagName(\"svg\");\n"
    "    let moving = false;\n"
    "    function mapPoint(svg, event){\n"
    "      const matrix = svg.getScreenCTM();\n"
    "      if(!matrix) return null;\n"
    "      const point = svg.createSVGPoint();\n"
    "      point.x = event.clientX;\n"
    "      point.y = event.clientY;\n"
    "      return point.matrixTransform(matrix.inverse());\n"
    "    }\n"
    "    for(let i = 0; i < svgs.length; i++){\n"
    "      const svg = svgs[i];\n"
    "      const texts = svg.getElementsByTagName(\"text\");\n"
    "      let suppressClick = false;\n"
    "      svg.addEventListener('pointerdown', function(){ suppressClick = false; }, true);\n"
    "      svg.addEventListener('click', function(e){\n"
    "          if(!suppressClick) return;\n"
    "          suppressClick = false;\n"
    "          e.preventDefault();\n"
    "          e.stopImmediatePropagation();\n"
    "      }, true);\n"
    "      for(const anchor of texts){\n"
    "          const link = anchor.parentNode;\n"
    "          link.addEventListener(\"click\", function(e){\n"
    "              e.preventDefault();\n"
    "              e.stopPropagation();\n"
    "          });\n"
    "          link.addEventListener(\"pointerdown\", function(e){\n"
    "              if(e.button !== 0 || moving) return;\n"
    "              const start = mapPoint(svg, e);\n"
    "              if(!start) return;\n"
    "              e.stopPropagation();\n"
    "              e.preventDefault();\n"
    "              moving = true;\n"
    "              const pointerId = e.pointerId;\n"
    "              // Move decorations with the text, including marker backgrounds.\n"
    "              const transforms = Array.from(link.children)\n"
    "                  .filter(child => child.transform && child.transform.baseVal.numberOfItems)\n"
    "                  .map(child => {\n"
    "                      const matrix = child.transform.baseVal.consolidate().matrix;\n"
    "                      return {matrix, x: matrix.e, y: matrix.f};\n"
    "                  });\n"
    "              let changed = false;\n"
    "              function move(e){\n"
    "                  if(e.pointerId !== pointerId) return;\n"
    "                  const point = mapPoint(svg, e);\n"
    "                  if(!point) return;\n"
    "                  const dx = point.x - start.x;\n"
    "                  const dy = point.y - start.y;\n"
    "                  changed = dx !== 0 || dy !== 0;\n"
    "                  for(const t of transforms){\n"
    "                      t.matrix.e = t.x + dx;\n"
    "                      t.matrix.f = t.y + dy;\n"
    "                  }\n"
    "              }\n"
    "              function finish(e){\n"
    "                  if(e.pointerId !== pointerId) return;\n"
    "                  if(e.type === 'pointerup') move(e);\n"
    "                  suppressClick = true;\n"
    "                  moving = false;\n"
    "                  e.stopPropagation();\n"
    "                  e.preventDefault();\n"
    "                  window.removeEventListener('pointermove', move);\n"
    "                  window.removeEventListener('pointerup', finish);\n"
    "                  window.removeEventListener('pointercancel', finish);\n"
    "                  if(link.hasPointerCapture(pointerId)) link.releasePointerCapture(pointerId);\n"
    "                  if(e.type === 'pointercancel'){\n"
    "                      for(const t of transforms){\n"
    "                          t.matrix.e = t.x;\n"
    "                          t.matrix.f = t.y;\n"
    "                      }\n"
    "                      return;\n"
    "                  }\n"
    "                  if(!changed) return;\n"
    "                  const href = link.href.baseVal;\n"
    "                  let internal_id = 0;\n"
    "                  const sp = href.split('#');\n"
    "                  if(sp.length > 1) internal_id = _coords[sp[1]];\n"
    "                  const mapCoords = _coords2[svg.dataset.coordMap] || {};\n"
    "                  if(!internal_id) internal_id = mapCoords[href];\n"
    "                  if(!internal_id) internal_id = mapCoords[anchor.textContent.trim()];\n"
    "                  if(!internal_id) return;\n"
    "                  const matrix = anchor.transform.baseVal.consolidate().matrix;\n"
    "                  const new_x = Math.round(matrix.e);\n"
    "                  const new_y = Math.round(matrix.f);\n"
    "                  const combo = `${internal_id}.${new_x}.${new_y}`;\n"
    "                  const request = new XMLHttpRequest();\n"
    "                  request.open(\"PUT\", \"dnd:///roommove/\"+combo, true);\n"
    "                  request.send();\n"
    "              }\n"
    "              link.setPointerCapture(pointerId);\n"
    "              window.addEventListener('pointermove', move);\n"
    "              window.addEventListener('pointerup', finish);\n"
    "              window.addEventListener('pointercancel', finish);\n"
    "          });\n"
    "      }\n"
    "      svg.dataset.coordMap = i;\n"
    "      let text_height = 0;\n"
    "      if(texts.length){\n"
    "        const first_text = texts[0];\n"
    "        text_height = first_text.getBBox().height || 0;\n"
    "      }\n"
    "      svg.addEventListener(\"click\", function(e){\n"
    "        let name = prompt('Enter Room Name');\n"
    "        if(name){\n"
    "          const point = mapPoint(svg, e);\n"
    "          if(!point) return;\n"
    "          const baseline = texts.length ? getComputedStyle(texts[0]).dominantBaseline : \"\";\n"
    "          const offset = baseline === 'middle' || baseline === 'central' ? 0 : text_height/2;\n"
    "          const true_x = Math.round(point.x);\n"
    "          const true_y = Math.round(point.y + offset);\n"
    "          let request = new XMLHttpRequest();\n"
    "          if(!name.includes(\".\")){\n"
    "            name += \".\";\n"
    "          }\n"
    "          request.open(\"PUT\", \"dnd:///roomclick/\"+name+\"/\"+true_x+\".\"+true_y, true);\n"
    "          request.send();\n"
    "        }\n"
    "      });\n"
    "    }\n"
    "  });\n"
    "::js\n"
    "  let coords = ctx.select_nodes({attributes:[\"coord\"]});\n"
    "  let s = ctx.root.make_child(NodeType.SCRIPTS);\n"
    "  let o = {};\n"
    "  for(let co of coords){\n"
    "      o[co.id] = co.internal_id;\n"
    "  }\n"
    "  s.make_child(NodeType.STRING, {header:`let _coords = ${JSON.stringify(o)};`});\n"
    "  let imglinks = ctx.select_nodes({type:NodeType.IMGLINKS});\n"
    "  let o2 = {};\n"
    "  for(let i = 0; i < imglinks.length; i++){\n"
    "      const il = imglinks[i];\n"
    "      o2[i] = {};\n"
    "      for(let ch of il.children){\n"
    "          if(ch.type != NodeType.STRING) continue;\n"
    "          let lead = ch.header.split('=')[0].trim();\n"
    "          o2[i][lead] = ch.internal_id;\n"
    "      }\n"
    "  }\n"
    "  s.make_child(NodeType.STRING, {header:`let _coords2 = ${JSON.stringify(o2)};`});\n"
    )
SCROLL_RESTO_SCRIPT='''
::script
    document.addEventListener('DOMContentLoaded', function(){
        for(let [key, value] of Object.entries(SCROLLRESTO)){
            if(key == '__document'){
                document.dispatchEvent(new CustomEvent('dnd:restore-state', {detail:value}));
            }
            else if(key == "html"){
                const html = document.getElementsByTagName("html")[0];
                if(html){
                    html.scrollLeft = value[0];
                    html.scrollTop = value[1];
                    }
            }
            else {
                let thing = document.getElementById(key);
                if(!thing){
                    let things = document.getElementsByClassName(key);
                    if(things.length)
                        thing = things[0];
                }
                if(thing){
                    thing.scrollLeft = value[0];
                    thing.scrollTop = value[1];
                }
            }
        }
    });
'''
EXTRA_CSS='''
::css
  body {
    /* the overscroll effect looks really bad in the webview */
    overscroll-behavior: none;
  }
'''
GET_SCROLL_POSITION_SCRIPT='''
(function(){
    const result = {};
    const html = document.getElementsByTagName("html")[0];
    if(!html)
        return null;
    if(html.scrollLeft || html.scrollTop)
        result.html = [html.scrollLeft, html.scrollTop];
    function get_scroll(ident){
        let thing = document.getElementById(ident);
        if(!thing){
            let things = document.getElementsByClassName(ident);
            if(things.length)
                thing = things[0];
        }
        if(thing && (thing.scrollLeft || thing.scrollTop)){
            result[ident] = [thing.scrollLeft, thing.scrollTop];
        }
    }
    get_scroll("left");
    get_scroll("center");
    get_scroll("right");
    const documentState = {};
    document.dispatchEvent(new CustomEvent('dnd:save-state', {detail:documentState}));
    if(Object.keys(documentState).length)
        result.__document = documentState;
    if(Object.keys(result).length){
        return JSON.stringify(result);
    }
    return null;
}());
'''
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

    def replace_source(self, text: str) -> None:
        """Apply compiler output as one undoable edit without losing prior edits."""
        if self.isReadOnly():
            return
        # Compiler output has a final line terminator; editor buffers do not.
        if text.endswith('\n'):
            text = text[:-1]
        old = self.toPlainText()
        if old == text:
            return
        start = 0
        while start < min(len(old), len(text)) and old[start] == text[start]:
            start += 1
        suffix = 0
        while (suffix < min(len(old), len(text)) - start
               and old[len(old)-suffix-1] == text[len(text)-suffix-1]):
            suffix += 1
        old_end, new_end = len(old)-suffix, len(text)-suffix
        # QTextCursor positions count UTF-16 units, unlike Python string indices.
        start_units = len(old[:start].encode('utf-16-le')) // 2
        old_end_units = len(old[:old_end].encode('utf-16-le')) // 2
        new_end_units = len(text[:new_end].encode('utf-16-le')) // 2
        cursor = self.textCursor()
        position, anchor = cursor.position(), cursor.anchor()
        vertical, horizontal = self.verticalScrollBar().value(), self.horizontalScrollBar().value()
        cursor.beginEditBlock()
        cursor.setPosition(start_units)
        cursor.setPosition(old_end_units, QTextCursor.MoveMode.KeepAnchor)
        cursor.insertText(text[start:new_end])
        cursor.endEditBlock()

        def map_position(pos):
            if pos <= start_units:
                return pos
            if pos >= old_end_units:
                return pos + new_end_units - old_end_units
            return start_units + min(pos-start_units, new_end_units-start_units)

        cursor.setPosition(map_position(anchor))
        cursor.setPosition(map_position(position), QTextCursor.MoveMode.KeepAnchor)
        self.setTextCursor(cursor)
        self.verticalScrollBar().setValue(vertical)
        self.horizontalScrollBar().setValue(horizontal)

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

    def updateLineNumberArea(self, rect:QRect, dy:int) -> None:
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
        key = event.key()
        if key == Qt.Key.Key_Tab:
            if self.isReadOnly():
                return
            if self.textCursor().hasSelection():
                self.alter_indent(indent=True)
                return
            self.insertPlainText('  ')
            return
        if key == Qt.Key.Key_Return or key == Qt.Key.Key_Enter:
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
        if key == Qt.Key.Key_Backspace:
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
        if sys.platform != 'darwin':
            if event.modifiers() == Qt.KeyboardModifier.ControlModifier:
                if key == Qt.Key.Key_H:
                    cursor.deletePreviousChar()
                    return
                if key == Qt.Key.Key_D:
                    cursor.deleteChar()
                    return
                if event.key() == Qt.Key.Key_B:
                    self.moveCursor(QTextCursor.MoveOperation.Left)
                    return
                if event.key() == Qt.Key.Key_F:
                    self.moveCursor(QTextCursor.MoveOperation.Right)
                    return
                if event.key() == Qt.Key.Key_N:
                    self.moveCursor(QTextCursor.MoveOperation.Down)
                    return
                if event.key() == Qt.Key.Key_P:
                    self.moveCursor(QTextCursor.MoveOperation.Up)
                    return
                if event.key() == Qt.Key.Key_A:
                    self.moveCursor(QTextCursor.MoveOperation.StartOfLine)
                    return
                if event.key() == Qt.Key.Key_E:
                    self.moveCursor(QTextCursor.MoveOperation.EndOfLine)
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

        painter.fillRect(event.rect(), Qt.GlobalColor.lightGray)

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
                    painter.fillRect(QRect(0, int(top), self.lineNumberArea.width(), height), Qt.GlobalColor.yellow)
                if blockNumber == self.error_line:
                    painter.fillRect(QRect(0, int(top), self.lineNumberArea.width(), height), Qt.GlobalColor.red)

                number = str(blockNumber + 1)
                painter.setPen(Qt.GlobalColor.black)
                painter.drawText(0, int(top), self.lineNumberArea.width(), height, Qt.AlignmentFlag.AlignRight, number)

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
    def insert_script(self, fname:str) -> None:
        self.insert_dnd_block('::script\n  ' + fname + '\n')
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
            r"  ::js",
            r"    // this is an example of how to script the imglinks",
            r"    let imglinks = node.parent;",
            r"    let coord_nodes = ctx.select_nodes({attributes:['coord']});",
            r"    for(let c of coord_nodes){",
            r"      let lead = c.header;",
            r"      let position = c.attributes.get('coord');",
            r"      imglinks.add_child(`${lead} = #${c.id} @${position}`);",
            r"    }"
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
        cursor.setPosition(end_block.position() + len(end_block.text()), QTextCursor.MoveMode.KeepAnchor)
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
        menu.exec(event.globalPos())


class DndWebPage(QWebEnginePage):
    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)
        self.basedir = ''

    def acceptNavigationRequest(self, url:QUrl|str, navtype:QWebEnginePage.NavigationType, isMainFrame:bool) -> bool:
        if isinstance(url, str):
            return False
        if url.scheme() == 'data':
            return True
        if navtype == QWebEnginePage.NavigationType.NavigationTypeLinkClicked:
            path = url.path()
            host = url.host()
            if host != APPHOST:
                QDesktopServices.openUrl(url)
                return False
            if path.endswith(('.dnd', '.html')):
                if path.endswith('.html'):
                    path = path[:-len('.html')] + '.dnd'
                filepath = os.path.normpath(path)
                # this is kind of a hack, but I don't want to learn how to encode windows style
                # drive letters into a URL properly and this basically works.
                if IS_WINDOWS and filepath.startswith('/') and len(filepath) > 2 and filepath[2] == ':':
                    filepath = filepath[1:]
                if os.path.isfile(filepath):
                    add_tab(filepath)
                else:
                    LOGGER.debug('Checking to create: %s, url: %s', filepath, url)
                    answer = QMessageBox.question(None, "Create file?", f'{filepath} does not exist. Create and open the file?', defaultButton=QMessageBox.StandardButton.Yes) # type: ignore # ???
                    if answer == QMessageBox.StandardButton.Yes:
                        LOGGER.debug('creating: %s', filepath)
                        open(filepath, 'w').close()
                        add_tab(filepath)
                return False
            return False
        return False
        result = super().acceptNavigationRequest(url, navtype, isMainFrame)
        return result

class SplitterHandler(QObject):
    def eventFilter(self, watched:QObject, event:QEvent) -> bool:
        if event.type() == QEvent.Type.MouseButtonDblClick:
            handle: QSplitterHandle = watched # type: ignore
            if handle.splitter().widget(0).width():
                handle.splitter().setSizes([0, 1])
            else:
                handle.splitter().setSizes([1,10000])
            return True
        return False

class Page(QSplitter):
    def keyPressEvent(self, event: QKeyEvent) -> None:
        # idk if this is the right spot for this.
        if not (event.modifiers() & Qt.ControlModifier): # type: ignore # type defs should subclass int
            return super().keyPressEvent(event)
        try:
            v = int(chr(event.key()))
        except:
            pass
        else:
            if v == 0:
                v = 10
            v -= 1
            if v < TABWIDGET.count():
                TABWIDGET.setCurrentIndex(v)
                return
        return super().keyPressEvent(event)
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.closing = False
        self.editor_window = None
        self.editor_open = False
        self.inflight = False
        self.webpage = DndWebPage(self)
        self.web = QWebEngineView()
        self.web.setPage(self.webpage)
        self.webpage.setHtml(' ', baseUrl=QUrl(f'https://{APPHOST}/this.html'))
        # no idea why this is needed
        self.web.resize(400, 400)
        self.textedit = DndEditor('')
        self.textedit.setFont(FONT)
        self.textedit.setMinimumSize(int(EIGHTYCHARS*1.1), 200)
        self.dirname = '.'
        self.textedit.document().contentsChanged.connect(self.contents_changed)
        self.error_display = QTextBrowser()
        self.error_display.setOpenLinks(False)
        self.error_display.anchorClicked.connect(self.open_error)
        self.error_display.setFont(FONT)
        self.error_display.setReadOnly(True)
        self.editor_holder = QSplitter(WINDOW)
        self.editor_holder.setOrientation(Qt.Orientation.Vertical)
        self.editor_holder.addWidget(self.textedit)
        self.editor_holder.addWidget(self.error_display)
        self.checks = [
            QCheckBox('Auto-apply changes', self),
            QCheckBox('Read-only', self),
            QCheckBox('Coord helper', self),
        ]
        self.checks[0].setCheckState(Qt.CheckState.Checked)
        self.auto_apply = True
        self.checks[0].stateChanged.connect(self.auto_apply_changed)
        self.checks[1].stateChanged.connect(self.read_only_changed)
        self.checks[2].stateChanged.connect(self.coord_helper_changed)
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
        self.scroll_pos_string = ''
        self.show_errors = True
        self.coord_helper = False
        self.editor_holder.hide()
        self.addWidget(self.web)

    def open_error(self, url):
        filename = url.toLocalFile()
        add_tab(filename)
        page = all_windows.get(filename)
        if page:
            line, column = map(int, url.fragment().split(':'))
            page.show_editor()
            page.scroll_to_line(line, column)

    def scroll_to_line(self, line, column=0):
        # Preview navigation updates the selection without changing window visibility.
        block = self.textedit.document().findBlockByLineNumber(max(0, line-1))
        if block.isValid():
            cursor = QTextCursor(block)
            cursor.setPosition(block.position() + min(max(0, column), max(0, block.length()-1)))
            self.textedit.setTextCursor(cursor)
            self.textedit.ensureCursorVisible()

    def scroll_into_view(self) -> None:
        line = self.textedit.textCursor().blockNumber()
        line += 1
        text = self.textedit.toPlainText()
        try:
            ctx = pydndc.Context(filename='')
            ctx.root.parse(text, filename='a')
            node: pydndc.Node| None = ctx.node_by_approximate_location('a', line)
            if not node:
                return
            while node and node.handle != ctx.root.handle and not node.id:
                node = node.parent
            if not node or node.handle == ctx.root.handle:
                return
            script=('''
            (function(){
              let node = document.getElementById('%s');
              if(!node) return;
              node.scrollIntoView(true);
            })();
            ''' % node.id)
            self.webpage.runJavaScript(script)
        except:
            LOGGER.exception('Problem while doing change scroll into view')

    def contents_changed(self) -> None:
        if self.auto_apply:
            self.update_html()

    def auto_apply_changed(self, state:int) -> None:
        state = Qt.CheckState(state)
        if state == Qt.CheckState.Unchecked:
            self.auto_apply = False
        if state == Qt.CheckState.Checked:
            self.auto_apply = True
            self.update_html()

    def read_only_changed(self, state:int) -> None:
        state = Qt.CheckState(state)
        if state == Qt.CheckState.Unchecked:
            self.textedit.setReadOnly(False)
        if state == Qt.CheckState.Checked:
            self.textedit.setReadOnly(True)
        if self.coord_helper:
            self.update_html()

    def coord_helper_changed(self, state:int) -> None:
        state = Qt.CheckState(state)
        if state == Qt.CheckState.Unchecked:
            self.coord_helper = False
        if state == Qt.CheckState.Checked:
            self.coord_helper = True
        self.update_html()

    def file_changed(self, path:str) -> None:
        if path not in self.dependencies:
            return
        LOGGER.debug("dependency '%s' changed", path)
        self.update_html()

    def clear_errors(self) -> None:
        self.error_display.clear()
        self.textedit.error_line = None

    def display_dndc_error(self, error_type:int, filename:str, row:int, col:int, message:str) -> None:
        error_types = (
            'Error',
            'Warning',
            'System Error',
            'Info',
            'Debug',
            )
        if error_type < 0 or error_type >= len(error_types):
            LOGGER.error('unrecognized error type: %d', error_type)
            return
        if error_type == 0:
            self.textedit.error_line = row
        et = error_types[error_type]
        if et == 'Info':
            self.error_display.append(html_escape.escape(f'{et}: {message}'))
        else:
            path = filename or self.filename
            if not os.path.isabs(path):
                path = os.path.join(self.dirname, path)
            url = QUrl.fromLocalFile(os.path.normpath(path))
            url.setFragment(f'{row+1}:{col}')
            label = html_escape.escape(f'{et}:{row+1}:{col+1}')
            self.error_display.append(f'<a href="{html_escape.escape(url.toString(), quote=True)}">{label}</a>: {html_escape.escape(message)}')

    def get_text_for_preview(self) -> str:
        text = self.textedit.toPlainText()
        if self.coord_helper and not self.textedit.isReadOnly():
            text += COORD_HELPER_SCRIPT
        if not self.coord_helper:
            text += ( "\n"
                    "::script\n"
                    "  document.addEventListener('DOMContentLoaded', function(){\n"
                    "    const anchors = document.getElementsByTagName('a');\n"
                    "    function add_interceptor(a){\n"
                    "      let href = a.href;\n"
                    "      if(href.baseVal) href = href.baseVal;\n"
                    "      const targetURL = new URL(href, document.baseURI);\n"
                    "      const currentURL = new URL(document.baseURI);\n"
                    "      const target = decodeURIComponent(targetURL.hash.slice(1));\n"
                    "      targetURL.hash = currentURL.hash = '';\n"
                    "      if(target && targetURL.href === currentURL.href){\n"
                    "        a.onclick = function(e){\n"
                    "          let t = document.getElementById(target);\n"
                    "          if(t){\n"
                    "            t.scrollIntoView();\n"
                    "            e.preventDefault();\n"
                    "            e.stopPropagation();\n"
                    "            let request = new XMLHttpRequest();\n"
                    "            request.open('PUT', 'dnd:///scrolltoid/'+encodeURIComponent(target), true);\n"
                    "            request.send();\n"
                    "            return false;\n"
                    "          }\n"
                    "        };\n"
                    "      }\n"
                    "    }\n"
                    "    for(let a of anchors){\n"
                    "      add_interceptor(a);\n"
                    "    }\n"
                    "    function add_scroller(h){\n"
                    "        if(!h.id) return;\n"
                    "        h.style.cursor = 'pointer';\n"
                    "        h.onclick = function(e){\n"
                    "            e.preventDefault();\n"
                    "            e.stopPropagation();\n"
                    "            let request = new XMLHttpRequest();\n"
                    "            request.open('PUT', 'dnd:///scrolltoid/'+encodeURIComponent(h.id), true);\n"
                    "            request.send();\n"
                    "            return false;\n"
                    "        };\n"
                    "    }\n"
                    "    for(let h of document.getElementsByTagName('h2'))\n"
                    "        add_scroller(h);\n"
                    "    for(let h of document.getElementsByTagName('h3'))\n"
                    "        add_scroller(h);\n"
                    "  });\n"
                    )

        if self.scroll_pos_string:
            text += '\n::script\n  const SCROLLRESTO = {}\n'.format(self.scroll_pos_string)
            text += SCROLL_RESTO_SCRIPT
        text += EXTRA_CSS
        return text

    def update_html(self) -> None:
        if self.closing:
            return
        # t0 = time.time()
        # print(f'{t0=}')
        if self.inflight:
            return
        self.inflight = True
        self.webpage.runJavaScript(GET_SCROLL_POSITION_SCRIPT, 0, self.set_scroll_pos)
    def set_scroll_pos(self, x:str) -> None:
        if self.closing:
            return
        self.scroll_pos_string = x
        self.inflight = False
        # t1 = time.time()
        # print(f'{t1=}')
        self.clear_errors()
        before_paths = set(FILE_CACHE.paths())
        Flags = pydndc.Flags
        flags = Flags.USE_DND_URL_SCHEME
        if PRINT_STATS:
            flags |= Flags.PRINT_STATS
        flags |= Flags.DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ctx = pydndc.Context(flags=flags, filename=self.filename, filecache=FILE_CACHE)
        ctx.logger = self.display_dndc_error
        ctx.base_dir = self.dirname
        try:
            ctx.root.parse(self.get_text_for_preview())
            ctx.resolve_imports()
            ctx.execute_js()
            ctx.resolve_links()
            ctx.build_toc()
            html = ctx.render()
        except (ValueError, RuntimeError):
            # I am not sure if this comment is still valid.

            # On error, the file cache can have loaded things, but we don't get those
            # dependencies.
            before = time.time()
            paths = FILE_CACHE.paths()
            for path in paths:
                if path not in before_paths:
                    WINDOW.watcher.addPath(path)
            after = time.time()
            # print(f'addPaths: {(after-before)*1000:.3f}ms')
            return
        depends = ctx.dependencies
        del ctx

        # t1 = time.time()
        u = QUrl()
        u.setScheme('https')
        u.setHost(APPHOST)
        path = self.filename.replace('\\', '/') if IS_WINDOWS else self.filename
        if not path.startswith('/'):
            path = '/' + path
        u.setPath(path)
        self.webpage.setHtml(html, baseUrl=u)
        # t2 = time.time()
        self.dependencies = depends
        if depends:
            WINDOW.watcher.addPaths(list(depends))
        # t3 = time.time()
        # print(f'htmlgen = {(t1-t0)*1000:.3f}ms')
        # print(f'sethtml = {(t2-t1)*1000:.3f}ms')
        # print(f'total   = {(t3-t0)*1000:.3f}ms')

    def format(self) -> None:
        if self.textedit.isReadOnly():
            return
        try:
            text = pydndc.reformat(self.textedit.toPlainText(), logger=self.display_dndc_error)
        except ValueError:
            return
        self.textedit.replace_source(text)
    def hide_editor(self) -> None:
        self.editor_open = False
        if self.editor_window:
            focused = self.editor_window.isActiveWindow()
            self.editor_window.sync_visibility()
            if focused:
                WINDOW.activateWindow()
                self.web.setFocus()

    def show_editor(self) -> None:
        TABWIDGET.setCurrentWidget(self)
        self.editor_open = True
        if self.editor_window is None:
            self.editor_window = EditorWindow(self, WINDOW)
            self.editor_holder.show()
        self.editor_window.sync_visibility(True)

    def dispose_editor(self) -> None:
        if self.closing:
            return
        self.closing = True
        self.editor_open = False
        if self.editor_window:
            self.editor_window.dispose()
            self.editor_window = None
        else:
            self.editor_holder.deleteLater()

    def show_error(self) -> None:
        self.error_display.show()
        self.show_errors = True
    def hide_error(self) -> None:
        self.clear_errors()
        self.error_display.hide()
        self.show_errors = False
    def save(self) -> bool:
        if not self.filename or not self.textedit.document().isModified():
            return True
        LOGGER.debug("Saving '%s'", self.filename)
        savefile = QSaveFile(self)
        savefile.setFileName(self.filename)
        if not savefile.open(QIODeviceBase.OpenModeFlag.WriteOnly):
            QMessageBox.critical(self.editor_window or WINDOW, 'Unable to save', savefile.errorString())
            return False
        text = self.textedit.toPlainText().encode('utf-8')
        if not text.endswith(b'\n'):
            text += b'\n'
        if savefile.write(text) != len(text) or not savefile.commit():
            QMessageBox.critical(self.editor_window or WINDOW, 'Unable to save', savefile.errorString())
            return False
        self.textedit.document().setModified(False)
        LOGGER.debug("Saved '%s'", self.filename)
        return True
    def get_fname(self, title:str, filter:str)->Optional[str]:
        fname, _ = QFileDialog.getOpenFileName(None, title, '', filter) # type: ignore # ???
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
        if self.textedit.isReadOnly():
            return
        fname = self.get_fname('Choose an image file', 'PNG images (*.png)')
        if not fname:
            return
        self.textedit.insert_image(fname)
    def insert_image_links(self)-> None:
        if self.textedit.isReadOnly():
            return
        fullname, _ = QFileDialog.getOpenFileName(None, 'Choose an image file', '', 'PNG images (*.png)') # type: ignore # ???
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
    def insert_script(self) -> None:
        fname = self.get_fname('Choose a JavaScript file', 'JS files (*.js)')
        if not fname:
            return
        self.textedit.insert_script(fname)
    def export_as_html(self) -> None:
        try:
            html = pydndc.htmlgen(self.textedit.toPlainText(), base_dir=self.dirname)
        except ValueError:
            mbox = QMessageBox()
            mbox.critical(None, 'Unable to convert current document', 'Unable to convert current document to html.\n\nSyntax Error in document (see error output).')  # type: ignore
            return
        options = QFileDialog.Option.DontConfirmOverwrite
        # if sys.platform == 'darwin':
            # options |= QFileDialog.Option.DontUseNativeDialog
        fname, _ = QFileDialog.getSaveFileName(None, 'Choose where to save html', '', 'HTML files (*.html)', selectedFilter="*.html", options=options)  # type: ignore
        if not fname:
            return
        if not fname.endswith('.html'):
            fname += '.html'
        savefile = QSaveFile(self)
        savefile.setFileName(fname)
        savefile.open(QIODeviceBase.OpenModeFlag.WriteOnly)
        text = html.encode('utf-8')
        if not text.endswith(b'\n'):
            text += b'\n'
        savefile.write(text)
        savefile.commit()


def make_page_widget(filename:str, allow_fail:bool) -> Optional[QWidget]:
    if filename in all_windows:
        return None
    new_file = False
    try:
        fp = open(filename, 'r', encoding='utf-8')
    except OSError:
        if not allow_fail or os.path.exists(filename):
            LOGGER.debug("Failed to open: '%s'", filename)
            return None
        new_file = True
        text = ''
    else:
        try:
            text = fp.read()
        except Exception as e:
            LOGGER.exception('Problem when reading text file')
            fp.close()
            error_message = f'Unable to read data from {filename}'
            if isinstance(e, UnicodeDecodeError):
                error_message += '\n' + 'The file contains invalid utf-8 data.\nConvert the file to utf-8 first (Notepad can do this)'
            QMessageBox.critical(WINDOW, 'Problem when reading file', error_message)
            return None
        else:
            fp.close()
    result = Page()
    # Qt uses newlines as separators, not terminators.
    # We'll add a newline back when we save.
    if text.endswith('\n'):
        text = text[:-1]
    result.textedit.setPlainText(text)
    result.textedit.document().setModified(new_file)
    dirname = os.path.dirname(filename)
    result.dirname = dirname
    result.filename = filename
    result.webpage.basedir = dirname
    result.update_html()
    all_windows[filename]= result
    return result

def condense(filename:str, is_windows=IS_WINDOWS) -> str:
    while filename.startswith('//'):
        filename = filename[1:]
    filename = os.path.normpath(filename)
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
        if not p:
            break
        parts.append(p[0])
    name = sep.join(reversed(parts))
    if is_windows and name[0] != '~':
        name = drive + ':\\' + name
    return name

def add_tab(filename:str, focus=True, allow_fail:bool=False) -> None:
    if sys.platform == 'win32':
        filename = filename.replace('/', '\\')
    while filename.startswith('//'):
        filename = filename[1:]
    filename = os.path.abspath(filename)
    LOGGER.debug("adding_tab: '%s'", filename)
    if filename in all_windows:
        if focus:
            TABWIDGET.setCurrentWidget(all_windows[filename])
        return
    page = make_page_widget(filename, allow_fail)
    if page is None:
        return
    TABWIDGET.addTab(page, condense(filename))
    if focus:
        TABWIDGET.setCurrentWidget(page)

def open_file(*args) -> None:
    fname, _ = QFileDialog.getOpenFileName(None, 'Choose a dnd file', '', 'Dnd Files (*.dnd)') # type: ignore # ???
    if not fname:
        return
    add_tab(fname)

def add_menus() -> None:
    menubar = WINDOW.menuBar()

    filemenu = menubar.addMenu('File')

    action = QAction('&Open', WINDOW)
    action.triggered.connect(open_file)
    action.setShortcut(QKeySequence('Ctrl+o'))
    filemenu.addAction(action)

    def new_file(*args) -> None:
        options = QFileDialog.Option.DontConfirmOverwrite
        # if sys.platform == 'darwin':
            # options |= QFileDialog.DontUseNativeDialog
        fname, _ = QFileDialog.getSaveFileName(None, 'Choose or Create a dnd file', '', 'Dnd Files (*.dnd)', selectedFilter="*.dnd", options=options)  # type: ignore
        if not fname:
            return
        add_tab(fname, allow_fail=True)
    action = QAction('&New', WINDOW)
    action.triggered.connect(new_file)
    action.setShortcut(QKeySequence('Ctrl+n'))
    filemenu.addAction(action)

    def save_file(*args) -> None:
        page: Optional[Page]= get_current_tab()
        if page:
            page.save()
    action = QAction('&Save', WINDOW)
    action.triggered.connect(save_file)
    action.setShortcut(QKeySequence('Ctrl+s'))
    filemenu.addAction(action)

    def export_file(*args) -> None:
        page: Optional[Page] = get_current_tab()
        if page: page.export_as_html()
    action = QAction('&Export As HTML', WINDOW)
    action.triggered.connect(export_file)
    filemenu.addAction(action)

    def close_current_tab(*args) -> None:
        active = APP.activeWindow()
        if isinstance(active, EditorWindow):
            active.page.hide_editor()
        elif TABWIDGET.currentIndex() >= 0:
            close_tab(TABWIDGET.currentIndex())
        else:
            WINDOW.close()
    action = QAction('&Close', WINDOW)
    action.triggered.connect(close_current_tab)
    action.setShortcut(QKeySequence('Ctrl+w'))
    filemenu.addAction(action)

    if sys.platform != 'darwin':
        action = QAction('&Exit', WINDOW)
        action.triggered.connect(WINDOW.close)
        filemenu.addAction(action)

    editmenu = menubar.addMenu('Edit')

    def format_dnd(*args) -> None:
        current_tab: Optional[Page] = get_current_tab()
        if not current_tab:
            return
        current_tab.format()
    action = QAction('&Format', WINDOW)
    action.triggered.connect(format_dnd)
    editmenu.addAction(action)

    def pickfont(*args) -> None:
        global FONT
        ok, font = QFontDialog.getFont(FONT)
        if ok:
            FONT = font
            for page in all_windows.values():
                page.textedit.setFont(FONT)
    action = QAction('F&ont', WINDOW)
    action.triggered.connect(pickfont)
    editmenu.addAction(action)

    def indent(*args) -> None:
        current_tab: Optional[Page] = get_current_tab()
        if not current_tab:
            return
        current_tab.textedit.alter_indent(indent=True)
    action = QAction('&Indent', WINDOW)
    action.setShortcut(QKeySequence('Ctrl+>'))
    action.triggered.connect(indent)
    editmenu.addAction(action)

    def dedent(*args) -> None:
        current_tab: Optional[Page] = get_current_tab()
        if not current_tab:
            return
        current_tab.textedit.alter_indent(indent=False)
    action = QAction('&Dedent', WINDOW)
    action.setShortcut(QKeySequence('Ctrl+<'))
    action.triggered.connect(dedent)
    editmenu.addAction(action)

    insert = menubar.addMenu('Insert')
    def insert_func(method):
        def insert_foo(*args) -> None:
            current_tab: Optional[Page] = get_current_tab()
            if not current_tab:
                return
            method(current_tab)
        return insert_foo

    action = QAction('&Image', WINDOW)
    action.triggered.connect(insert_func(Page.insert_image))
    insert.addAction(action)

    action = QAction('Image &Links', WINDOW)
    action.triggered.connect(insert_func(Page.insert_image_links))
    insert.addAction(action)

    action = QAction('&Dnd Import', WINDOW)
    action.triggered.connect(insert_func(Page.insert_dnd))
    insert.addAction(action)

    action = QAction('&JavaScript', WINDOW)
    action.triggered.connect(insert_func(Page.insert_script))
    insert.addAction(action)

    action = QAction('&CSS', WINDOW)
    action.triggered.connect(insert_func(Page.insert_css))
    insert.addAction(action)

    viewmenu = menubar.addMenu('View')

    def toggle_editors(*args) -> None:
        page = get_current_tab()
        if page:
            if page.editor_open:
                page.hide_editor()
            else:
                page.show_editor()
    action = QAction('&Edit it', WINDOW)
    action.setShortcut(QKeySequence('Ctrl+e'))
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
    action = QAction('Toggle &Error', WINDOW)
    action.triggered.connect(toggle_errors)
    viewmenu.addAction(action)

    def scroll_into_view(*args) -> None:
        current_tab: Optional[Page] = get_current_tab()
        LOGGER.debug('scrolling into view')
        if not current_tab:
            return
        current_tab.scroll_into_view()

    action = QAction('Scroll Into View', WINDOW)
    action.triggered.connect(scroll_into_view)
    action.setShortcut(QKeySequence('Ctrl+\r'))
    viewmenu.addAction(action)

    def refresh_highlight(*args) -> None:
        current_tab: Optional[Page] = get_current_tab()
        if not current_tab:
            return
        current_tab.textedit.highlight.rehighlight()

    action = QAction('&Refresh Highlighting', WINDOW)
    action.triggered.connect(refresh_highlight)
    viewmenu.addAction(action)

    helpmenu = menubar.addMenu('Help')

    thisdir = os.path.dirname(os.path.abspath(__file__))
    def add_help(fn:str, title:str) -> None:
        def open_fn(*args) -> None:
            fq = os.path.join(thisdir, fn)
            add_tab(fq)
        action = QAction(title, WINDOW)
        action.triggered.connect(open_fn)
        helpmenu.addAction(action)
    add_help('changelog.dnd', 'Changelog')
    add_help('Manual.dnd', 'Manual')
    add_help('REFERENCE.dnd', 'Reference')
    add_help('jsdoc.dnd', 'JavaScript API')
    if 0:
        add_help('OVERVIEW.dnd', 'Overview')

    def show_version(*args) -> None:
        QMessageBox.about(WINDOW, 'Version',
                f'GUI version: {PYGDNDC_VERSION}\n'
                f'dndc version: {pydndc.__version__}\n')
    action = QAction('&Version', WINDOW)
    action.triggered.connect(show_version)
    helpmenu.addAction(action)

    def open_log_folder(*args) -> None:
        if IS_WINDOWS:
            url = QUrl('file:///' + LOGS_FOLDER.replace('\\', '/'))
        else:
            url = QUrl('file://'+LOGS_FOLDER)
        success = QDesktopServices.openUrl(url)
        if not success:
            LOGGER.error("Failed to open: '%s'", url)
    action = QAction('&Open Logs Folder', WINDOW)
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
            LOGGER.error("Failed to open: '%s'", url)
    action = QAction('&Compress Logs', WINDOW)
    action.triggered.connect(compress_logs)
    helpmenu.addAction(action)

    developmenu = menubar.addMenu('Developer')

    def clear_caches(*args) -> None:
        FILE_CACHE.clear()
        QWebEngineProfile.defaultProfile().clearHttpCache()
        for window in all_windows.values():
            window.update_html()
    action = QAction('&Clear Caches', WINDOW)
    action.triggered.connect(clear_caches)
    developmenu.addAction(action)

    def recalculate_html(*args) -> None:
        for window in all_windows.values():
            window.update_html()
    action = QAction('&Recalculate HTML', WINDOW)
    action.triggered.connect(recalculate_html)
    developmenu.addAction(action)

    def toggle_timings(*args) -> None:
        global PRINT_STATS
        PRINT_STATS = not PRINT_STATS
    action = QAction('&Toggle Timings', WINDOW)
    action.triggered.connect(toggle_timings)
    developmenu.addAction(action)
    # These commands must remain available while a separate editor has focus.
    for action in WINDOW.findChildren(QAction):
        if not action.shortcut().isEmpty():
            action.setShortcutContext(Qt.ShortcutContext.ApplicationShortcut)
    for index in range(10):
        action = QAction(WINDOW)
        action.setShortcut(QKeySequence(f'Ctrl+{(index+1)%10}'))
        action.setShortcutContext(Qt.ShortcutContext.ApplicationShortcut)
        def select_tab(checked=False, index=index):
            if index < TABWIDGET.count():
                TABWIDGET.setCurrentIndex(index)
                sync_editors()
        action.triggered.connect(select_tab)
        WINDOW.addAction(action)
    return

add_menus()
WINDOW.restore_everything()
if not get_current_tab():
    open_file()
if not get_current_tab():
    LOGGER.info('Exiting due to user canceling open file')
    LOGGER.close()
    sys.exit(0)
WINDOW.show()
APP.exec()
LOGGER.info('Exiting normally')
LOGGER.close()
