#!/usr/bin/env python3
import datetime
import sys
import os
# os.environ["QT_AUTO_SCREEN_SCALE_FACTOR"] = "1"
os.environ['QT_ENABLE_HIGHDPI_SCALING'] = '1'
import install_deps
have_deps = install_deps.ensure_deps(False)
import sys
if not have_deps:
    sys.exit(1)
import pydndc
import logging
import glob
import json
from typing import List
from PySide2.QtWidgets import QApplication, QLabel, QMainWindow, QHBoxLayout, QPlainTextEdit, QWidget, QSplitter, QTabWidget, QAction, QFileDialog, QTextEdit, QFontDialog, QMessageBox, QSplitterHandle, QCheckBox, QToolButton, QPushButton, QLineEdit, QVBoxLayout, QGridLayout, QSpacerItem, QSizePolicy, QListWidget, QListWidgetItem
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

class FolderJobPage(QWidget):
    def __init__(self):
        super().__init__()
        self.layout = QGridLayout(self)

        self.layout.setSpacing(4)
        self.layout.setMargin(4)

        self.folder_lab = QLabel("Dnd Folder:")
        self.folder_ed = QLineEdit()
        self.folder_ed.setClearButtonEnabled(True)
        self.folder_button = QPushButton("Choose")
        def pick_folder() -> None:
            options = QFileDialog.Options()
            if sys.platform == 'darwin':
                options |= QFileDialog.DontUseNativeDialog
            dirname = QFileDialog.getExistingDirectory(None, 'Choose Folder of Dnd Files', '', options=options)  # type: ignore
            if not dirname:
                return
            self.folder_ed.setText(dirname)

        self.folder_button.clicked.connect(pick_folder)

        self.layout.addWidget(self.folder_lab, 0, 0, Qt.AlignTop)
        self.layout.addWidget(self.folder_ed, 0, 1, Qt.AlignTop)
        self.layout.addWidget(self.folder_button, 0, 2, Qt.AlignTop)

        self.out_lab = QLabel("Output Folder:")
        self.out_ed = QLineEdit()
        self.out_ed.setClearButtonEnabled(True)
        self.out_button = QPushButton("Choose")
        def pick_output() -> None:
            options = QFileDialog.Options()
            if sys.platform == 'darwin':
                options |= QFileDialog.DontUseNativeDialog
            dirname = QFileDialog.getExistingDirectory(None, 'Choose Folder of Dnd Files', '', options=options)  # type: ignore
            if not dirname:
                return
            self.out_ed.setText(dirname)

        self.out_button.clicked.connect(pick_output)


        self.layout.addWidget(self.out_lab, 1, 0, Qt.AlignTop)
        self.layout.addWidget(self.out_ed, 1, 1, Qt.AlignTop)
        self.layout.addWidget(self.out_button, 1, 2, Qt.AlignTop)
        def compile_one(infile:str, basedir:str, outfile:str) -> None:
            try:
                text = open(infile, 'r', encoding='utf-8').read()
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
                    self.error_display.appendPlainText(f'{infile}: {et}: {message}')
                else:
                    self.error_display.appendPlainText(f'{infile}: {et}:{row+1}:{col+1}: {message}')
            try:
                html, _ = pydndc.htmlgen(text, base_dir=basedir, error_reporter=error_reporter)
            except Exception as e:
                self.error_display.appendPlainText(f'{infile}: {e}')
                return
            try:
                out = open(outfile, 'w', encoding='utf-8')
            except Exception as e:
                self.error_display.appendPlainText(f'{infile}: {e}')
                return
            try:
                out.write(html)
                out.flush()
                out.close()
            except Exception as e:
                self.error_display.appendPlainText(f'{infile}: {e}')
                return
            self.error_display.appendPlainText(f'{os.path.basename(infile)}: Success!')

        def compile() -> None:
            self.error_display.clear()
            try:
                infolder = self.folder_ed.text().strip()
                outfolder = self.out_ed.text().strip()
                pattern = os.path.join(infolder, '*.dnd')
                files = glob.glob(pattern)
                for f in files:
                    outfile = os.path.join(outfolder, os.path.basename(f))
                    outfile = outfile[:-3] + 'html'
                    compile_one(f, infolder, outfile)
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
                if not os.path.isdir(path):
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
        self.layout.addWidget(self.bottom_buttons, 2, 1)
        self.error_display = QPlainTextEdit(self)
        self.layout.addWidget(self.error_display, 3, 1)
        self.layout.addItem(QSpacerItem(0, 0, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding), 5, 1)
    def get_state(self) -> dict:
        result =  dict(
                folder_text = self.folder_ed.text(),
                out_text = self.out_ed.text(),
                )
        return result

    def load_state(self, state:dict) -> None:
        self.folder_ed.setText(state.get('folder_text', ''))
        self.out_ed.setText(state.get('out_text', ''))
    def state_keys(self) -> List[str]:
        return ['folder_text', 'out_text']


class FileItemWidg(QWidget):
    def __init__(self, name, parent=None) -> None:
        super().__init__(parent)
        self.name = name
        self.out_ed = QLineEdit()
        self.out_ed.setText(self.name)
        def change():
            self.name = self.out_ed.text()
        self.out_ed.textChanged.connect(change)
        self.del_button = QPushButton('Remove')
        self.row = QHBoxLayout()
        self.row.setSpacing(0)
        self.row.setMargin(0)
        self.row.addWidget(self.out_ed)
        self.row.addWidget(self.del_button)
        self.setLayout(self.row)
class MyListItem(QListWidgetItem):
    pass

class FileListWidg(QListWidget):
    def add_file(self, filename:str) -> None:
        # item = QListWidgetItem(self)
        item = MyListItem(self)
        self.addItem(item)
        widg = FileItemWidg(filename, self)
        item.setSizeHint(widg.minimumSizeHint())
        self.setItemWidget(item, widg)
        item.widg = widg
        def remove():
            index = self.row(item)
            self.takeItem(index)
        item.widg.del_button.clicked.connect(remove)
    def del_file(self, filename:str) -> None:
        for i in range(self.count()):
            item = self.item(i)
            widg = item.widg # type: ignore
            if isinstance(widg, FileItemWidg):
                if widg.name == filename:
                    self.takeItem(i)
                    # self.removeItemWidget(item)
                    return
    def del_all(self) -> None:
        count = self.count()
        while count:
            count -= 1
            self.takeItem(count)
    def files(self) -> List[str]:
        result = []
        for i in range(self.count()):
            widg = self.item(i).widg # type: ignore
            if isinstance(widg, FileItemWidg):
                result.append(widg.name)
        return result



class ProjectJobPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.layout = QGridLayout(self)

        self.layout.setSpacing(4)
        self.layout.setMargin(4)

        self.file_lab = QLabel("Project File:")
        self.file_ed = QLineEdit()
        self.file_ed.setClearButtonEnabled(True)
        self.file_button = QPushButton("Load")
        def pick_file() -> None:
            oldname = self.file_ed.text().strip()
            fname, _ = QFileDialog.getOpenFileName(None, 'Choose a JSON file', '', 'JSON file (*.json)')
            if not fname:
                return
            if oldname == fname:
                self.setmess('Same project file chosen.')
                return
            loaded = self.load_project_file(fname, check=self.to_json())
            if loaded:
                self.file_ed.setText(fname)
        self.file_button.clicked.connect(pick_file)
        self.layout.addWidget(self.file_lab, 0, 0, Qt.AlignTop)
        self.layout.addWidget(self.file_ed, 0, 1, Qt.AlignTop)
        self.layout.addWidget(self.file_button, 0, 2, Qt.AlignTop)
        self.file_list_widg = FileListWidg(self)
        self.file_list_widg.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.layout.addWidget(self.file_list_widg, 2, 0, 1, 3)
        self.build_lab = QLabel("Output Dir:")
        self.build_ed = QLineEdit()
        self.build_ed.setClearButtonEnabled(True)
        self.build_button = QPushButton("Choose")
        def pick_output() -> None:
            options = QFileDialog.Options()
            if sys.platform == 'darwin':
                options |= QFileDialog.DontUseNativeDialog
            dirname = QFileDialog.getExistingDirectory(None, 'Choose Output Folder', '', options=options)  # type: ignore
            if not dirname:
                return
            dirname = self.relfile(dirname)
            self.build_ed.setText(dirname)

        self.build_button.clicked.connect(pick_output)

        self.buttons_widget = QWidget()
        bw_layout = QHBoxLayout(self.buttons_widget)
        self.add_button = QPushButton('Add File')
        def add_file():
            fname, _ = QFileDialog.getOpenFileName(None, 'Choose a DND file', '', 'DND file (*.dnd)')
            if not fname:
                return
            fname = self.relfile(fname)
            projfile = self.file_ed.text().strip()
            if projfile:
                basedir = os.path.dirname(projfile)
                # this can throw on windows
                try:
                    rel = os.path.relpath(fname, basedir)
                except:
                    pass
                else:
                    if '..' not in rel:
                        fname = rel
            self.file_list_widg.add_file(fname)
            return
        self.add_button.clicked.connect(add_file)

        self.save_button = QPushButton('Save')
        self.save_button.clicked.connect(self.save)
        self.reload_button = QPushButton('Reload')
        def reload():
            self.load_project_file(self.file_ed.text().strip())
        self.reload_button.clicked.connect(reload)
        self.compile_button = QPushButton('Compile')
        self.compile_button.clicked.connect(self.compile)
        self.open_button  = QPushButton('Open Output')
        def op():
            proj = self.file_ed.text().strip()
            base = os.path.dirname(proj)
            build = self.build_ed.text().strip()
            if not build: return
            build = os.path.join(base, build)
            if os.path.isfile(os.path.join(build, 'index.html')):
                build = os.path.join(build, 'index.html')
            success = QDesktopServices.openUrl(QUrl.fromLocalFile(build))
        self.open_button.clicked.connect(op)
        self.clear_button = QPushButton('Clear')
        def clear():
            self.file_list_widg.del_all()
            self.build_ed.setText('')
            self.file_ed.setText('')
        self.clear_button.clicked.connect(clear)

        bw_layout.addWidget(self.add_button)
        bw_layout.addWidget(self.save_button)
        bw_layout.addWidget(self.reload_button)
        bw_layout.addWidget(self.compile_button)
        bw_layout.addWidget(self.open_button)
        bw_layout.addWidget(self.clear_button)
        self.layout.addWidget(self.buttons_widget, 3, 0, 1, 3)

        self.layout.addWidget(self.build_lab, 1, 0, Qt.AlignTop)
        self.layout.addWidget(self.build_ed, 1, 1, Qt.AlignTop)
        self.layout.addWidget(self.build_button, 1, 2, Qt.AlignTop)
        self.error_display = QPlainTextEdit(self)
        self.error_display.setFixedHeight(200)
        self.layout.addWidget(self.error_display, 4, 1)

    def relfile(self, fname:str) -> str:
        projfile = self.file_ed.text().strip()
        if not projfile:
            return fname
        basedir = os.path.dirname(projfile)
        # this can throw on windows
        try:
            rel = os.path.relpath(fname, basedir)
        except:
            return fname
        if '..' in rel:
            return fname
        return rel

    def to_json(self):
        result = {}
        files = list(set(self.file_list_widg.files()))
        files.sort()
        if files:
            result['files'] = files
        build = self.build_ed.text().strip()
        if build:
            result['outdir'] = build
        return result
    def save(self) -> bool:
        js = self.to_json()

        outfile = self.file_ed.text()
        if not outfile:
            if not js:
                return False
            outfile, _ = QFileDialog.getSaveFileName(None, 'Choose a JSON file', '', 'JSON file (*.json)')
            if not outfile:
                return False
            self.file_ed.setText(outfile)
        try:
            LOGGER.debug('saving to %s', outfile)
            json.dump(js, open(outfile, 'w', encoding='utf-8'), indent=2)
        except:
            self.setmess(f"Failed to save to '{outfile}'")
            LOGGER.debug("Failed to save to '%s'", outfile)
            return False
        else:
            self.setmess(f"Saved to '{outfile}'")
            LOGGER.debug("Saved to '%s'", outfile)
            return True
    def compile(self):
        self.clearmess()
        def compile_one(infile:str, basedir:str, outfile:str) -> None:
            try:
                text = open(infile, 'r', encoding='utf-8').read()
            except Exception as e:
                self.addmess(f'{infile}: {e}')
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
                    self.addmess(f'{infile}: {et}: {message}')
                else:
                    self.addmess(f'{infile}: {et}:{row+1}:{col+1}: {message}')
            try:
                html, _= pydndc.htmlgen(text, base_dir=basedir, error_reporter=error_reporter)
            except Exception as e:
                self.addmess(f'{infile}: {e}')
                return
            try:
                out = open(outfile, 'w', encoding='utf-8')
            except Exception as e:
                self.addmess(f'{infile}: {e}')
                return
            try:
                out.write(html)
                out.flush()
                out.close()
            except Exception as e:
                self.addmess(f'{infile}: {e}')
                return
            self.addmess(f'{os.path.basename(infile)}: Success!')
            return
        outdir = self.build_ed.text().strip()
        if not outdir:
            self.setmess('Need an output folder to compile a project')
            return
        base = os.path.dirname(self.file_ed.text())
        files = self.file_list_widg.files()
        for f in files:

            if f.endswith('.dnd'):
                outf = os.path.basename(f[:-4])
            else:
                outf = os.path.basename(f)
            outf += '.html'
            compile_one(os.path.join(base, f), base, os.path.join(base, outdir, outf))
    def load_project_file(self, filename:str, check=False, gag=False) -> bool:
        try:
            fp = open(filename, 'r', encoding='utf-8')
            js = json.load(fp)
        except:
            if not gag:
                self.setmess(f"Failed to load from '{filename}'")
            return False
        if check:
            old_js = self.to_json()
            if old_js != js:
                msgBox = QMessageBox();
                msgBox.setText("The project file has unsaved changes.")
                msgBox.setInformativeText("Do you want to save your changes?")
                msgBox.setStandardButtons(QMessageBox.Save | QMessageBox.Discard | QMessageBox.Cancel)
                msgBox.setDefaultButton(QMessageBox.Save);
                ret = msgBox.exec_()
                if ret == QMessageBox.Save:
                    saved = self.save()
                    if not saved:
                        return False
                if ret == QMessageBox.Cancel:
                    return False
                # Do nothing if discard

        self.file_list_widg.del_all()
        filenames = js.get('files', [])
        outdir = js.get('outdir', 'Build')
        for fn in filenames:
            self.file_list_widg.add_file(fn)
        self.build_ed.setText(outdir)
        self.setmess(f"Loaded from '{filename}'")
        return True

    def setmess(self, mess:str) -> None:
        self.error_display.setPlainText(mess)
        self.error_display.repaint()

    def addmess(self, mess:str) -> None:
        self.error_display.appendPlainText(mess)
        self.error_display.repaint()
    def clearmess(self) -> None:
        self.error_display.clear()

    def get_state(self) -> dict:
        return {'projfile':self.file_ed.text().strip()}
    def load_state(self, state:dict) -> None:
        self.file_ed.setText(state.get('projfile', ''))
        self.load_project_file(self.file_ed.text(), gag=True)
        return
    def state_keys(self) -> List[str]:
        return ['projfile']

class MyMainWindow(QMainWindow):
    def __init__(self)->None:
        super().__init__()
        self.tabwidget = QTabWidget()
        self.setCentralWidget(self.tabwidget)
        self.single_job = SingleJobPage()
        self.folder_job = FolderJobPage()
        self.project_job = ProjectJobPage()
        self.tabwidget.addTab(self.single_job, 'Compile Single File')
        self.tabwidget.addTab(self.folder_job, 'Compile Folder')
        self.tabwidget.addTab(self.project_job, 'Compile Project File')
        self.settings = QSettings('DavidTechnology', APPNAME)
    def get_prefixed_widgets(self) -> list:
        return [
            ('singlefile',  self.single_job),
            ('folderjob',   self.folder_job),
            ('projectjob',  self.project_job),
        ]

    def restore_everything(self)->None:
        geometry = self.settings.value('window_geometry')
        if geometry is not None:
            self.restoreGeometry(geometry) # type: ignore
        else:
            self.resize(800, 400)
        for prefix, widget in self.get_prefixed_widgets():
            state_dict = {}
            for k in widget.state_keys():
                key = f'{prefix}.{k}'
                v = self.settings.value(key)
                if v is not None:
                    state_dict[k] = v
            widget.load_state(state_dict)
        whichtab = self.settings.value('whichtab')
        if whichtab:
            self.tabwidget.setCurrentIndex(int(whichtab))

    def closeEvent(self, e:QCloseEvent) -> None:
        self.settings.setValue('window_geometry', self.saveGeometry())
        for prefix, widget in self.get_prefixed_widgets():
            state = widget.get_state()
            for k, v in state.items():
                key = f'{prefix}.{k}'
                self.settings.setValue(key, v)
        self.settings.setValue('whichtab', self.tabwidget.currentIndex())
        self.project_job.save()
        e.accept()

WINDOW = MyMainWindow()
WINDOW.restore_everything()
WINDOW.show()
APP.exec_()
LOGGER.info('Exiting normally')
LOGGER.close()
