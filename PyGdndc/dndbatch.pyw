#!/usr/bin/env python3
import datetime
import sys
import os
import pydndc
import logging
from typing import List
from PySide2.QtWidgets import QApplication, QLabel, QMainWindow, QHBoxLayout, QPlainTextEdit, QWidget, QSplitter, QTabWidget, QAction, QFileDialog, QTextEdit, QFontDialog, QMessageBox, QSplitterHandle, QCheckBox, QToolButton, QPushButton, QLineEdit, QVBoxLayout, QGridLayout, QSpacerItem, QSizePolicy
from PySide2.QtGui import QFont, QKeySequence, QFontMetrics, QPainter, QColor, QTextFormat, QKeyEvent, QSyntaxHighlighter, QTextCharFormat, QImage, QDesktopServices, QContextMenuEvent, QDesktopServices, QCloseEvent
from PySide2.QtCore import Slot, Signal, QRect, QSize, Qt, QUrl, QStandardPaths, QSaveFile, QSettings, QObject, QEvent, QFileSystemWatcher, QFile, QThread, QTimer
VERSION = '0.0.1'
QApplication.setAttribute(Qt.AA_EnableHighDpiScaling)
APPNAME = 'DndBatch'
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
class Logs:
    def __init__(self) -> None:
        self.old_hook: Optional[Callable] = None
        try:
            self.stream = open(LOGFILE_LOCATION, 'a', encoding='utf-8')
        except:
            self.stream = sys.stderr
        self.LOGGER = logging.getLogger('pygdndc')
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
        self.info('%s: version is %s', APPNAME, VERSION)
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

class SingleJobPage(QWidget):
    def __init__(self):
        super().__init__()
        self.layout = QGridLayout(self)

        self.layout.setSpacing(4)
        self.layout.setMargin(4)

        self.file_lab = QLabel("Dnd File:")
        self.file_ed = QLineEdit()
        self.file_ed.setClearButtonEnabled(True)
        reldir = ''
        def file_ed_changed():
            filename = self.file_ed.text()
            dirname = os.path.dirname(filename)
            self.dir_ed.setPlaceholderText(dirname)
        self.file_ed.textChanged.connect(file_ed_changed)
        self.file_button = QPushButton("Choose")
        def pick_file() -> None:
            fname, _ = QFileDialog.getOpenFileName(None, 'Choose a dnd file', '', 'Dnd Files (*.dnd)')
            if fname:
                self.file_ed.setText(fname)


        self.file_button.clicked.connect(pick_file)

        self.layout.addWidget(self.file_lab, 0, 0, Qt.AlignTop)
        self.layout.addWidget(self.file_ed, 0, 1, Qt.AlignTop)
        self.layout.addWidget(self.file_button, 0, 2, Qt.AlignTop)

        self.dir_lab = QLabel("Relative Dir:")
        self.dir_ed = QLineEdit()
        self.dir_ed.setClearButtonEnabled(True)
        self.dir_button = QPushButton("Choose")

        self.layout.addWidget(self.dir_lab, 1, 0, Qt.AlignTop)
        self.layout.addWidget(self.dir_ed, 1, 1, Qt.AlignTop)
        self.layout.addWidget(self.dir_button, 1, 2, Qt.AlignTop)

        self.out_lab = QLabel("Output File:")
        self.out_ed = QLineEdit()
        self.out_ed.setClearButtonEnabled(True)
        self.out_button = QPushButton("Choose")
        def pick_output() -> None:
            options = QFileDialog.Options()
            options |= QFileDialog.DontConfirmOverwrite
            if sys.platform == 'darwin':
                options |= QFileDialog.DontUseNativeDialog
            fname, _ = QFileDialog.getSaveFileName(None, 'Choose where to save html', '', 'HTML files (*.html)', initialFilter="*.html", options=options)  # type: ignore
            if not fname:
                return
            if not fname.endswith('.html'):
                fname += '.html'
            self.out_ed.setText(fname)
        self.out_button.clicked.connect(pick_output)
            

        self.layout.addWidget(self.out_lab, 2, 0, Qt.AlignTop)
        self.layout.addWidget(self.out_ed, 2, 1, Qt.AlignTop)
        self.layout.addWidget(self.out_button, 2, 2, Qt.AlignTop)
        def compile() -> None:
            try:
                self.error_display.clear()
                path = self.file_ed.text().strip()
                outpath = self.out_ed.text().strip()
                if not outpath:
                    self.error_display.appendPlainText('Must choose an output file!')
                    return
                if not outpath.endswith('.html'):
                    outpath += '.html'
                dirname = self.dir_ed.text().strip()
                if not dirname:
                    dirname = os.path.dirname(path)
                try:
                    text = open(path, 'r', encoding='utf-8').read()
                except Exception as e:
                    self.error_display.appendPlainText(str(e))
                    return
                def error_reporter(error_type:int, filename:str, row:int, col:int, message:str):
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
                    et = error_types[error_type]
                    if et == 'Info':
                        self.error_display.appendPlainText(f'{et}: {message}')
                    else:
                        self.error_display.appendPlainText(f'{et}:{row+1}:{col+1}: {message}')
                try:
                    html, _ = pydndc.htmlgen(text, base_dir=dirname, error_reporter=error_reporter)
                except Exception as e:
                    self.error_display.appendPlainText(f'{e}')
                    return
                try:
                    out = open(outpath, 'w', encoding='utf-8')
                except Exception as e:
                    self.error_display.appendPlainText(f'{e}')
                    return
                try:
                    out.write(html)
                    out.flush()
                    out.close()
                except Exception as e:
                    self.error_display.appendPlainText(f'{e}')
                    return
                self.error_display.appendPlainText('Success!')
                return
            finally:
                self.error_display.repaint()

        self.do_it_button = QPushButton("Compile")
        self.do_it_button.clicked.connect(compile)
        self.open_button = QPushButton("Open Output")
        def open_output():
            try:
                path = self.out_ed.text().strip()
                if not path:
                    self.error_display.setPlainText('Must choose an output path.')
                    return
                if not os.path.isfile(path):
                    self.error_display.setPlainText(f'"{path}" does not exist.')
                    return
                success = QDesktopServices.openUrl(QUrl.fromLocalFile(path))
                if not success:
                    self.error_display.setPlainText(f'Unable to open "{path}".')
            finally:
                self.error_display.repaint()
        self.open_button.clicked.connect(open_output)
        self.bottom_buttons = QWidget()
        self.bot_layout = QHBoxLayout()
        self.bot_layout.setSpacing(0)
        self.bot_layout.setMargin(0)
        self.bot_layout.addWidget(self.do_it_button)
        self.bot_layout.addWidget(self.open_button)
        self.bottom_buttons.setLayout(self.bot_layout)
        self.layout.addWidget(self.bottom_buttons, 3, 1)
        self.error_display = QPlainTextEdit(self)
        self.layout.addWidget(self.error_display, 4, 1)
        self.layout.addItem(QSpacerItem(0, 0, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding), 5, 1)
    def get_state(self) -> dict:
        result =  dict(
                file_text = self.file_ed.text(),
                dir_text = self.dir_ed.text(),
                out_text = self.out_ed.text(),
                )
        return result

    def load_state(self, state:dict) -> None:
        self.file_ed.setText(state.get('file_text', ''))
        self.dir_ed.setText(state.get('dir_text', ''))
        self.out_ed.setText(state.get('out_text', ''))
    def state_keys(self) -> List[str]:
        return ['file_text', 'dir_text', 'out_text']

class MultiJobPage(QWidget):
    def __init__(self):
        super().__init__()
        self.layout = QGridLayout(self)

        self.layout.setSpacing(4)
        self.layout.setMargin(4)

        self.file_lab = QLabel("Dnd File:")
        self.file_ed = QLineEdit()
        self.file_ed.setClearButtonEnabled(True)
        reldir = ''
        def file_ed_changed():
            filename = self.file_ed.text()
            dirname = os.path.dirname(filename)
            self.dir_ed.setPlaceholderText(dirname)
        self.file_ed.textChanged.connect(file_ed_changed)
        self.file_button = QPushButton("Choose")
        def pick_file() -> None:
            fname, _ = QFileDialog.getOpenFileName(None, 'Choose a dnd file', '', 'Dnd Files (*.dnd)')
            if fname:
                self.file_ed.setText(fname)


        self.file_button.clicked.connect(pick_file)

        self.layout.addWidget(self.file_lab, 0, 0, Qt.AlignTop)
        self.layout.addWidget(self.file_ed, 0, 1, Qt.AlignTop)
        self.layout.addWidget(self.file_button, 0, 2, Qt.AlignTop)

        self.dir_lab = QLabel("Relative Dir:")
        self.dir_ed = QLineEdit()
        self.dir_ed.setClearButtonEnabled(True)
        self.dir_button = QPushButton("Choose")

        self.layout.addWidget(self.dir_lab, 1, 0, Qt.AlignTop)
        self.layout.addWidget(self.dir_ed, 1, 1, Qt.AlignTop)
        self.layout.addWidget(self.dir_button, 1, 2, Qt.AlignTop)

        self.out_lab = QLabel("Output Folder:")
        self.out_ed = QLineEdit()
        self.out_ed.setClearButtonEnabled(True)
        self.out_button = QPushButton("Choose")
        def pick_output() -> None:
            options = QFileDialog.Options()
            if sys.platform == 'darwin':
                options |= QFileDialog.DontUseNativeDialog
            dirname = QFileDialog.getExistingDirectory(None, 'Choose where to save html', '', options=options)  # type: ignore
            if not dirname:
                return
            self.out_ed.setText(dirname)
        self.out_button.clicked.connect(pick_output)
            

        self.layout.addWidget(self.out_lab, 2, 0, Qt.AlignTop)
        self.layout.addWidget(self.out_ed, 2, 1, Qt.AlignTop)
        self.layout.addWidget(self.out_button, 2, 2, Qt.AlignTop)
        def compile() -> None:
            try:
                self.error_display.clear()
                path = self.file_ed.text().strip()
                outpath = self.out_ed.text().strip()
                if not outpath:
                    self.error_display.appendPlainText('Must choose an output file!')
                    return
                if not outpath.endswith('.html'):
                    outpath += '.html'
                dirname = self.dir_ed.text().strip()
                if not dirname:
                    dirname = os.path.dirname(path)
                try:
                    text = open(path, 'r', encoding='utf-8').read()
                except Exception as e:
                    self.error_display.appendPlainText(str(e))
                    return
                def error_reporter(error_type:int, filename:str, row:int, col:int, message:str):
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
                    et = error_types[error_type]
                    if et == 'Info':
                        self.error_display.appendPlainText(f'{et}: {message}')
                    else:
                        self.error_display.appendPlainText(f'{et}:{row+1}:{col+1}: {message}')
                try:
                    html, _ = pydndc.htmlgen(text, base_dir=dirname, error_reporter=error_reporter)
                except Exception as e:
                    self.error_display.appendPlainText(f'{e}')
                    return
                try:
                    out = open(outpath, 'w', encoding='utf-8')
                except Exception as e:
                    self.error_display.appendPlainText(f'{e}')
                    return
                try:
                    out.write(html)
                    out.flush()
                    out.close()
                except Exception as e:
                    self.error_display.appendPlainText(f'{e}')
                    return
                self.error_display.appendPlainText('Success!')
                return
            finally:
                self.error_display.repaint()

        self.do_it_button = QPushButton("Compile")
        self.do_it_button.clicked.connect(compile)
        self.open_button = QPushButton("Open Output")
        def open_output():
            try:
                path = self.out_ed.text().strip()
                if not path:
                    self.error_display.setPlainText('Must choose an output path.')
                    return
                if not os.path.isfile(path):
                    self.error_display.setPlainText(f'"{path}" does not exist.')
                    return
                success = QDesktopServices.openUrl(QUrl.fromLocalFile(path))
                if not success:
                    self.error_display.setPlainText(f'Unable to open "{path}".')
            finally:
                self.error_display.repaint()
        self.open_button.clicked.connect(open_output)
        self.bottom_buttons = QWidget()
        self.bot_layout = QHBoxLayout()
        self.bot_layout.setSpacing(0)
        self.bot_layout.setMargin(0)
        self.bot_layout.addWidget(self.do_it_button)
        self.bot_layout.addWidget(self.open_button)
        self.bottom_buttons.setLayout(self.bot_layout)
        self.layout.addWidget(self.bottom_buttons, 3, 1)
        self.error_display = QPlainTextEdit(self)
        self.layout.addWidget(self.error_display, 4, 1)
        self.layout.addItem(QSpacerItem(0, 0, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding), 5, 1)
    def get_state(self) -> dict:
        result =  dict(
                file_text = self.file_ed.text(),
                dir_text = self.dir_ed.text(),
                out_text = self.out_ed.text(),
                )
        return result

    def load_state(self, state:dict) -> None:
        self.file_ed.setText(state.get('file_text', ''))
        self.dir_ed.setText(state.get('dir_text', ''))
        self.out_ed.setText(state.get('out_text', ''))
    def state_keys(self) -> List[str]:
        return ['file_text', 'dir_text', 'out_text']
class FolderJobPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
    def get_state(self) -> dict:
        return {}
    def load_state(self, state:dict) -> None:
        return
    def state_keys(self) -> List[str]:
        return []

class ProjectJobPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
    def get_state(self) -> dict:
        return {}
    def load_state(self, state:dict) -> None:
        return
    def state_keys(self) -> List[str]:
        return []

class MyMainWindow(QMainWindow):
    def __init__(self)->None:
        super().__init__()
        self.tabwidget = QTabWidget()
        self.setCentralWidget(self.tabwidget)
        self.single_job = SingleJobPage()
        self.folder_job = FolderJobPage()
        self.multi_job = MultiJobPage()
        self.project_job = ProjectJobPage()
        self.tabwidget.addTab(self.single_job, 'Compile Single File')
        self.tabwidget.addTab(self.multi_job, 'Compile Multiple Files')
        self.tabwidget.addTab(self.folder_job, 'Compile Folder')
        self.tabwidget.addTab(self.project_job, 'Compile JSON Project')
        self.settings = QSettings('DavidTechnology', APPNAME)
    def get_prefixed_widgets(self) -> list:
        return [
            ('singlefile',  self.single_job),
            ('multijob',    self.multi_job),
            ('folderjob',   self.folder_job),
            ('projectjob',  self.project_job),
        ]

    def restore_everything(self)->None:
        geometry = self.settings.value('window_geometry')
        if geometry is not None:
            self.restoreGeometry(geometry) # type: ignore
        else:
            self.resize(600, 400)
        for prefix, widget in self.get_prefixed_widgets():
            state_dict = {}
            for k in widget.state_keys():
                key = f'{prefix}.{k}'
                v = self.settings.value(key)
                if v is not None:
                    state_dict[k] = v
            widget.load_state(state_dict)

    def closeEvent(self, e:QCloseEvent) -> None:
        self.settings.setValue('window_geometry', self.saveGeometry())
        for prefix, widget in self.get_prefixed_widgets():
            state = widget.get_state()
            for k, v in state.items():
                key = f'{prefix}.{k}'
                self.settings.setValue(key, v)
        e.accept()

WINDOW = MyMainWindow()
WINDOW.restore_everything()
WINDOW.show()
APP.exec_()
LOGGER.info('Exiting normally')
LOGGER.close()
