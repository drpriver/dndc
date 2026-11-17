#include "DndcEdit.h"
#include <utility>
#include <memory>
#include <Dndc/dndc.h>
#include <Dndc/dndc_ast.h>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QRegularExpressionMatch>
#include <QtCore/QSettings>
#include <QtCore/QPointer>
#include <QtGui/QDesktopServices>
#include <QtGui/QPalette>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtGui/QMoveEvent>
#ifdef __APPLE__
#include <dlfcn.h>
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#include <QtWebEngineCore/QWebEngineProfile>
#else
#include <QtWebEngineWidgets/QWebEngineProfile>
#endif
#include <QtWebEngineCore/QtWebEngineCore>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFontDialog>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStyle>
#include <QtWidgets/QScrollBar>
#include "compiler_warnings.h"
// #pragma GCC diagnostic ignored "-Wunused-lambda-capture"
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#define QS(x) QStringLiteral(x)
const QString APPHOST = QS("invalid.");
const QString APPURL = QS("https://invalid./this.html");
const QString APPNAME = QS("DndEdit");
QString LOGS_FOLDER;
QFont* FONT;
QFontMetrics* FONTMETRICS;
int EIGHTYCHARS;
bool PRINT_STATS = false;
QSettings* SETTINGS;
QFileSystemWatcher* watcher;
DndcFileCache *b64cache, *textcache;
DndcWorkerThread* dndc_worker;
QTabWidget* TABS;
QHash<QString, Page*> ALL_WINDOWS;
QRegularExpression* WHITESPACE_RE;

QFile* LOGFILE;
QTextStream* LOGSTREAM;

struct CtxWrapper {
    DndcContext* ctx;
    ~CtxWrapper(){ if(ctx) dndc_ctx_destroy(ctx); }
    static CtxWrapper
    create(
        unsigned long long flags = 0,
        DndcFileCache* b64  = nullptr,
        DndcFileCache* text = nullptr){
        return {dndc_create_ctx(flags, b64, text)};
    }
};

void
DNDC_LOGIT(QtMsgType type, const QMessageLogContext& context, const QString& msg){
    static const QString Unknown =  QS("[UNKNOWN]");
    static const QString Debug =    QS("[DEBUG]");
    static const QString Info =     QS("[INFO]");
    static const QString Warn =     QS("[WARN]");
    static const QString Critical = QS("[CRITICAL]");
    static const QString Fatal =    QS("[FATAL]");
    const QString* type_string = &Unknown;
    switch(type){
        case QtDebugMsg:    type_string = &Debug;    break;
        case QtInfoMsg:     type_string = &Info;     break;
        case QtWarningMsg:  type_string = &Warn;     break;
        case QtCriticalMsg: type_string = &Critical; break;
        case QtFatalMsg:    type_string = &Fatal;    break;
    }

    LOGFILE->write(QS("%1 %2 %3:%4: %5\n").arg(QDateTime::currentDateTime().toString(), *type_string, QUrl(context.file).fileName()).arg(context.line).arg(msg).toUtf8());
}


QString COORD_HELPER_SCRIPT = QS(
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
    );

QString SCROLL_RESTO_SCRIPT = QS(
    "::script\n"
    "    document.addEventListener('DOMContentLoaded', function(){\n"
    "        for(let [key, value] of Object.entries(SCROLLRESTO)){\n"
    "            if(key == '__document'){\n"
    "                document.dispatchEvent(new CustomEvent('dnd:restore-state', {detail:value}));\n"
    "            }\n"
    "            else if(key == 'html'){\n"
    "                const html = document.getElementsByTagName('html')[0];\n"
    "                if(html){\n"
    "                    html.scrollLeft = value[0];\n"
    "                    html.scrollTop = value[1];\n"
    "                    }\n"
    "            }\n"
    "            else {\n"
    "                let thing = document.getElementById(key);\n"
    "                if(!thing){\n"
    "                    let things = document.getElementsByClassName(key);\n"
    "                    if(things.length)\n"
    "                        thing = things[0];\n"
    "                }\n"
    "                if(thing){\n"
    "                    thing.scrollLeft = value[0];\n"
    "                    thing.scrollTop = value[1];\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "    });\n"
    );

QString EXTRA_CSS=QS("\n"
    "::css\n"
    "  body {\n"
    "    /* the overscroll effect looks really bad in the webview */\n"
    "    overscroll-behavior: none;\n"
    "  }\n"
    );
QString GET_SCROLL_POSITION_SCRIPT = QS(
    "(function(){\n"
    "    const result = {};\n"
    "    const html = document.getElementsByTagName('html')[0];\n"
    "    if(!html)\n"
    "        return null;\n"
    "    if(html.scrollLeft || html.scrollTop)\n"
    "        result.html = [html.scrollLeft, html.scrollTop];\n"
    "    function get_scroll(ident){\n"
    "        let thing = document.getElementById(ident);\n"
    "        if(!thing){\n"
    "            let things = document.getElementsByClassName(ident);\n"
    "            if(things.length)\n"
    "                thing = things[0];\n"
    "        }\n"
    "        if(thing && (thing.scrollLeft || thing.scrollTop)){\n"
    "            result[ident] = [thing.scrollLeft, thing.scrollTop];\n"
    "        }\n"
    "    }\n"
    "    get_scroll('left');\n"
    "    get_scroll('center');\n"
    "    get_scroll('right');\n"
    "    const documentState = {};\n"
    "    document.dispatchEvent(new CustomEvent('dnd:save-state', {detail:documentState}));\n"
    "    if(Object.keys(documentState).length)\n"
    "        result.__document = documentState;\n"
    "    if(Object.keys(result).length){\n"
    "        return JSON.stringify(result);\n"
    "    }\n"
    "    return null;\n"
    "}());\n"
    );

QSize LineNumberArea::sizeHint() const{
    return QSize(qobject_cast<DndEditor*>(codeEditor)->lineNumberAreaWidth(), 0);
}
void LineNumberArea::paintEvent(QPaintEvent* event){
    qobject_cast<DndEditor*>(codeEditor)->lineNumberAreaPaintEvent(event);
}

Page*
get_current_page(void){
    if(auto editor = dynamic_cast<EditorWindow*>(QApplication::activeWindow()))
        return editor->page;
    auto page_ = TABS->currentWidget();
    if(!page_) return nullptr;
    auto page = qobject_cast<Page*>(page_);
    return page;
}

#ifdef __APPLE__
static void* cocoa_selector(const char* name){
    static auto sel = reinterpret_cast<void*(*)(const char*)>(dlsym(RTLD_DEFAULT, "sel_registerName"));
    return sel(name);
}
static void* cocoa_window(QWidget* widget){
    static auto send = reinterpret_cast<void*(*)(void*, void*)>(dlsym(RTLD_DEFAULT, "objc_msgSend"));
    return send(reinterpret_cast<void*>(widget->winId()), cocoa_selector("window"));
}
#endif

EditorWindow::EditorWindow(Page* p, QWidget* parent):
    QMainWindow(parent, Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
    page(p), owner(parent), owner_position(parent->pos())
{
    setAttribute(Qt::WA_QuitOnClose, false);
    setCentralWidget(page->editor_holder);
    setWindowTitle(page->filename + QS(" — Editor[*]"));
    setWindowModified(page->textedit->document()->isModified());
    connect(page->textedit->document(), &QTextDocument::modificationChanged, this, &QWidget::setWindowModified);
    auto screen = owner->windowHandle() ? owner->windowHandle()->screen() : QGuiApplication::primaryScreen();
    auto area = screen->availableGeometry();
    resize(qMin(800, area.width()), qMin(1000, area.height()));
    move(area.center() - rect().center());
    owner->installEventFilter(this);
    auto escape = new QAction(this);
    escape->setShortcut(QKeySequence(Qt::Key_Escape));
    escape->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QAction::triggered, page, &Page::hide_editor);
    addAction(escape);
}
EditorWindow::~EditorWindow(){
    detach();
    owner->removeEventFilter(this);
}
void EditorWindow::attach(){
#ifdef __APPLE__
    if(!attached){
        static auto send = reinterpret_cast<void(*)(void*, void*, void*, long)>(dlsym(RTLD_DEFAULT, "objc_msgSend"));
        send(cocoa_window(owner), cocoa_selector("addChildWindow:ordered:"), cocoa_window(this), 1);
        attached = true;
    }
#endif
}
void EditorWindow::detach(){
#ifdef __APPLE__
    if(attached){
        static auto send = reinterpret_cast<void(*)(void*, void*, void*)>(dlsym(RTLD_DEFAULT, "objc_msgSend"));
        send(cocoa_window(owner), cocoa_selector("removeChildWindow:"), cocoa_window(this));
        attached = false;
    }
#endif
}
void EditorWindow::sync_visibility(bool focus){
    bool visible = page->editor_open && page->isVisible() && owner->isVisible() && !owner->isMinimized();
    if(visible){
        if(!isVisible()){
            setAttribute(Qt::WA_ShowWithoutActivating, !focus);
            bool on_screen = false;
            for(auto screen: QGuiApplication::screens())
                on_screen |= screen->availableGeometry().intersects(frameGeometry());
            if(!on_screen){
                auto screen = owner->windowHandle()->screen();
                move(screen->availableGeometry().center() - rect().center());
            }
            show();
            attach();
        }
        if(focus){
            raise();
            activateWindow();
            page->textedit->setFocus();
        }
    }
    else {
        detach();
        hide();
    }
}
void EditorWindow::closeEvent(QCloseEvent* event){
    page->hide_editor();
    event->ignore();
}
bool EditorWindow::eventFilter(QObject* watched, QEvent* event){
    if(watched == owner){
#ifndef __APPLE__
        if(event->type() == QEvent::Move){
            if(!owner->isMinimized()){
                move(pos() + owner->pos() - owner_position);
                owner_position = owner->pos();
            }
        }
#endif
        if(event->type() == QEvent::Hide || event->type() == QEvent::Show || event->type() == QEvent::WindowStateChange)
            QTimer::singleShot(0, this, [this](){ sync_visibility(); });
        else if(event->type() == QEvent::WindowActivate && isVisible())
            raise();
    }
    return QMainWindow::eventFilter(watched, event);
}
static void sync_editors(){
    auto current = qobject_cast<Page*>(TABS->currentWidget());
    for(auto page: std::as_const(ALL_WINDOWS))
        if(page != current && page->editor_window)
            page->editor_window->sync_visibility();
    if(current && current->editor_window)
        current->editor_window->sync_visibility(QGuiApplication::applicationState() == Qt::ApplicationActive);
}
static void close_tab(int index){
    auto page = qobject_cast<Page*>(TABS->widget(index));
    if(!page || !page->save()) return;
    page->dispose_editor();
    ALL_WINDOWS.remove(page->filename);
    TABS->removeTab(index);
    page->deleteLater();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
}

MainWindow::~MainWindow()
{
}

void MainWindow::closeEvent(QCloseEvent* e){
    QStringList filenames = QStringList(ALL_WINDOWS.keys());
    SETTINGS->setValue(QS("filenames"), filenames);
    SETTINGS->setValue(QS("window_geometry"), saveGeometry());
    for(auto page: std::as_const(ALL_WINDOWS)){
        if(!page->save()){
            e->ignore();
            return;
        }
    }
    for(auto page: std::as_const(ALL_WINDOWS))
        page->dispose_editor();
    e->accept();
}

void
MainWindow::restore_everything(){
    auto bytes = SETTINGS->value(QS("window_geometry")).toByteArray();
    if(bytes.length())
     restoreGeometry(SETTINGS->value(QS("window_geometry")).toByteArray());
    else
        showMaximized();
    auto vfiles = SETTINGS->value(QS("filenames"));
    if(vfiles.canConvert<QStringList>()){
        QStringList l = vfiles.toStringList();
        for(const auto& filename: l){
            auto info = QFileInfo(filename);
            if(!info.exists())
                continue;
            add_tab(filename);
        }
    }
    else if(vfiles.canConvert<QString>()){
        QString filename = vfiles.toString();
        auto info = QFileInfo(filename);
        if(info.exists())
            add_tab(filename);
    }
}

void
MainWindow::open_file(void){
    auto filename = QFileDialog::getOpenFileName(this, QS("Choose a dnd file"), QString(), QS("Dnd Files (*.dnd)"));
    if(filename.isNull())
        return;
    add_tab(filename);
}

void
MainWindow::add_menus(void){
    auto menubar = menuBar();
    auto filemenu = menubar->addMenu(QS("File"));
    QAction* action;
    action = new QAction(QS("&Open"), this);
    connect(action, &QAction::triggered, [this](){
        open_file();
        });
    action->setShortcut(QKeySequence(QS("Ctrl+o")));
    filemenu->addAction(action);

    action = new QAction(QS("&New"), this);
    connect(action, &QAction::triggered, [this](){
        auto options = QFileDialog::DontConfirmOverwrite;
        auto filename = QFileDialog::getSaveFileName(this, QS("Choose or Create a dnd file"), QString(), QS("Dnd Files (*.dnd)"), nullptr, options);
        if(filename.length())
            add_tab(filename);
        });
    action->setShortcut(QKeySequence(QS("Ctrl+n")));
    filemenu->addAction(action);

    action = new QAction(QS("&Save"), this);
    connect(action, &QAction::triggered, [](){
            auto page = get_current_page();
            if(!page) return;
            page->save();
        });
    action->setShortcut(QKeySequence(QS("Ctrl+s")));
    filemenu->addAction(action);

    action = new QAction(QS("&Export As HTML"), this);
    connect(action, &QAction::triggered, [](){
            auto page = get_current_page();
            if(!page) return;
            page->export_as_html();
            });
    filemenu->addAction(action);

    action = new QAction(QS("&Close"), this);
    connect(action, &QAction::triggered, [this](){
            auto page = get_current_page();
            if(!page){
                close();
                return;
                }
            if(auto editor = dynamic_cast<EditorWindow*>(QApplication::activeWindow()))
                editor->page->hide_editor();
            else
                close_tab(TABS->currentIndex());
            });
    action->setShortcut(QKeySequence(QS("Ctrl+w")));
    filemenu->addAction(action);
    #ifndef __APPLE__
        action = new QAction(QS("&Exit"), this);
        connect(action, &QAction::triggered, this, &QMainWindow::close);
        filemenu->addAction(action);
    #endif
    auto editmenu = menubar->addMenu(QS("Edit"));
    action = new QAction(QS("&Format"), this);
    connect(action, &QAction::triggered, [](){
            auto page = get_current_page();
            if(!page) return;
            page->format();
        });
    editmenu->addAction(action);

    action = new QAction(QS("F&ont"), this);
    connect(action, &QAction::triggered, [this](){
            bool ok;
            QFont font = QFontDialog::getFont(&ok, *FONT, this);
            if(!ok) return;
            *FONT = font;
            for(auto page: std::as_const(ALL_WINDOWS))
                page->textedit->setFont(*FONT);
            });
    editmenu->addAction(action);

    action = new QAction(QS("&Indent"), this);
    connect(action, &QAction::triggered, [](){
            auto page = get_current_page();
            if(!page) return;
            page->textedit->alter_indent(true);
            });
    action->setShortcut(QKeySequence(QS("Ctrl+>")));
    editmenu->addAction(action);

    action = new QAction(QS("&Dedent"), this);
    action->setShortcut(QKeySequence(QS("Ctrl+<")));
    connect(action, &QAction::triggered, [](){
            auto page = get_current_page();
            if(!page) return;
            page->textedit->alter_indent(false);
            });
    editmenu->addAction(action);

    auto insert = menubar->addMenu(QS("Insert"));
    #define INSERT(name, method) do { \
        action = new QAction(QS(name), this); \
        connect(action, &QAction::triggered, [](){ \
                auto page = get_current_page(); \
                if(!page) return; \
                page->method(); \
                }); \
        insert->addAction(action); } while(0)
    INSERT("&Image", insert_image);
    INSERT("Image &Links", insert_image_links);
    INSERT("&Dnd Import", insert_dnd);
    INSERT("&JavaScript", insert_script);
    INSERT("&CSS", insert_css);
    #undef INSERT

    auto viewmenu = menubar->addMenu(QS("View"));
    action = new QAction("&Edit it", this);
    action->setShortcut(QKeySequence(QS("Ctrl+e")));
    connect(action, &QAction::triggered, [](){
        auto page = get_current_page();
        if(!page) return;
        if(page->editor_open) page->hide_editor();
        else page->show_editor();
    });
    viewmenu->addAction(action);

    action = new QAction("&Refresh Highlighting", this);
    connect(action, &QAction::triggered, [](){
        auto page = get_current_page();
        if(!page) return;
        page->textedit->highlight->rehighlight();
        });
    viewmenu->addAction(action);

    action = new QAction("Scroll Into View", this);
    connect(action, &QAction::triggered, [](){
        auto page = get_current_page();
        if(!page) return;
        page->scroll_selection_into_view();
        });
    action->setShortcut(QKeySequence("Ctrl+\r"));
    viewmenu->addAction(action);

    auto helpmenu = menubar->addMenu("Help");
    action = new QAction("&Version", this);
    connect(action, &QAction::triggered, [this](){
            QMessageBox::about(this, "Version",
                    "Dndc Version: " DNDC_VERSION "\n");
            });
    helpmenu->addAction(action);

    action = new QAction("&Open Logs Folder", this);
    connect(action, &QAction::triggered, [this](){
        auto url = QUrl::fromLocalFile(QDir::fromNativeSeparators(LOGS_FOLDER));
        auto success = QDesktopServices::openUrl(url);
        if(!success)
            QMessageBox::warning(this, "Unable to open log folder.", "Unable to open the log folder (it might not exist).\nTried to open " + LOGS_FOLDER);
        });
    helpmenu->addAction(action);

    // Not sure this is even needed anymore, and I am too lazy
    // to do the work of getting this working.
#if 0
    action = new QAction("&Compress Logs", this);
    connect(action, &QAction::triggered, [this](){
        auto url = QUrl(LOGS_FOLDER);
        QDesktopServices.openUrl(url);
        });
#endif

    auto developmenu = menubar->addMenu("Developer");
    action = new QAction("&Clear Caches", this);
    connect(action, &QAction::triggered, [](){
            dndc_filecache_clear(b64cache);
            dndc_filecache_clear(textcache);
            QWebEngineProfile::defaultProfile()->clearHttpCache();
            for(auto page: std::as_const(ALL_WINDOWS))
                page->update_html();
        });
    developmenu->addAction(action);

    action = new QAction("&Recalculate HTML", this);
    connect(action, &QAction::triggered, [](){
            auto page = get_current_page();
            if(!page) return;
            page->update_html();
        });
    developmenu->addAction(action);

    action = new QAction("&Toggle Timings", this);
    connect(action, &QAction::triggered, [](){
        PRINT_STATS = !PRINT_STATS;
        });
    developmenu->addAction(action);
}

DndSyntaxHighlighter::~DndSyntaxHighlighter(){
}

LineNumberArea::~LineNumberArea(){
}


DndEditor::DndEditor(QWidget* parent): QPlainTextEdit(parent){
        this->lineNumberArea = new LineNumberArea(this);
        connect(this, &QPlainTextEdit::blockCountChanged, this, &DndEditor::updateLineNumberAreaWidth);
        connect(this, &DndEditor::updateRequest, this, &DndEditor::updateLineNumberArea);
        connect(this, &QPlainTextEdit::cursorPositionChanged, this, &DndEditor::highlightCurrentLine);
        updateLineNumberAreaWidth(0);
        // Idk if this is guaranteed, but it is important that we can
        // update the syntax analysis before the highlighter
        // is called on a line.
        connect(this->document(), &QTextDocument::contentsChange, this, &DndEditor::update_syntax);
        highlight = new DndSyntaxHighlighter(this->document());
}
DndEditor::~DndEditor(){
}

void
DndEditor::replace_source(QString text){
    if(isReadOnly()) return;
    if(text.endsWith('\n')) text.chop(1);
    const auto old = toPlainText();
    if(old == text) return;
    int start = 0;
    while(start < old.size() && start < text.size() && old[start] == text[start]) start++;
    if(start && old[start-1].isHighSurrogate()) start--;
    int suffix = 0;
    while(suffix < old.size()-start && suffix < text.size()-start &&
          old[old.size()-suffix-1] == text[text.size()-suffix-1]) suffix++;
    if(suffix && old[old.size()-suffix].isLowSurrogate()) suffix--;
    const int old_end = old.size()-suffix;
    const int new_end = text.size()-suffix;
    auto cursor = textCursor();
    const int position = cursor.position(), anchor = cursor.anchor();
    const int vertical = verticalScrollBar()->value(), horizontal = horizontalScrollBar()->value();
    cursor.beginEditBlock();
    cursor.setPosition(start);
    cursor.setPosition(old_end, QTextCursor::KeepAnchor);
    cursor.insertText(text.mid(start, new_end-start));
    cursor.endEditBlock();
    auto map_position = [=](int pos){
        if(pos <= start) return pos;
        if(pos >= old_end) return pos + new_end-old_end;
        return start + qMin(pos-start, new_end-start);
    };
    cursor.setPosition(map_position(anchor));
    cursor.setPosition(map_position(position), QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    verticalScrollBar()->setValue(vertical);
    horizontalScrollBar()->setValue(horizontal);
}

void
DndEditor::update_syntax(void){
    auto text = toPlainText();
    DndcStringViewUtf16 textu16 = {(size_t)text.size(), text.utf16()};
    QHash<int, QList<HighlightRegion>> highlight_regions;
    DndcSyntaxFuncUtf16* synf = [](void* user_data, int type, int line, int col, const unsigned short* begin, size_t length){
        auto regions = (QHash<int, QList<HighlightRegion>>*)user_data;
        HighlightRegion region = {type, col, (int)length};
        (*regions)[line].append(region);
    };
    dndc_analyze_syntax_utf16(textu16, synf, &highlight_regions);
    highlight->update_regions(std::move(highlight_regions));
}

int
DndEditor::lineNumberAreaWidth(void){
    auto max_value = std::max(1, blockCount());
    int space = 3 + fontMetrics().horizontalAdvance(QString::number(max_value*10));
    return space;
}

void
DndEditor::updateLineNumberAreaWidth(int){
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void
DndEditor::updateLineNumberArea(QRect rect, int dy){
    if(dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    if(rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void
DndEditor::resizeEvent(QResizeEvent*event){
    QPlainTextEdit::resizeEvent(event);
    auto cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void
DndEditor::keyPressEvent(QKeyEvent* event){
    if(event->key() == Qt::Key::Key_Tab){
        if(isReadOnly())
            return;
        if(textCursor().hasSelection()){
            alter_indent(true);
            return;
        }
        insertPlainText(QS("  "));
        return;
    }
    if(event->key() == Qt::Key::Key_Return || event->key() == Qt::Key::Key_Enter){
        if(isReadOnly())
            return;
        auto block = textCursor().block();
        auto text = block.text();
        auto leading_space = WHITESPACE_RE->match(text);
        if(leading_space.hasMatch()){
            insertPlainText(QS("\n") + leading_space.captured());
        }
        else{
            insertPlainText(QS("\n"));
        }
        return;
    }
    if(event->key() == Qt::Key::Key_Backspace){
        if(isReadOnly())
            return;
        auto cursor = textCursor();
        auto block = cursor.block();
        auto text = block.text();
        auto position = cursor.positionInBlock();
        if((position & 1) != 1 && position >= 2){
            if(text[position-1] == ' ' && text[position-2] == ' '){
                cursor.deletePreviousChar();
                cursor.deletePreviousChar();
                return;
            }
        }
    }
    QPlainTextEdit::keyPressEvent(event);
}
void
DndEditor::highlightCurrentLine(void){
    // ... what?
    return;
}
void
DndEditor::lineNumberAreaPaintEvent(QPaintEvent* event){
    QPainter painter(lineNumberArea);
    QPalette palette = QApplication::palette(this);
    painter.fillRect(event->rect(), palette.base());
    auto block = firstVisibleBlock();
    auto blockNumber = block.blockNumber();
    auto cursor_number = textCursor().blockNumber();
    auto top = blockBoundingGeometry(block).translated(contentOffset()).top();
    auto bottom = top + blockBoundingRect(block).height();

    // Just to make sure I use the right font
    auto height = fontMetrics().height();
    while(block.isValid() && top <= event->rect().bottom()){
        if(block.isVisible() && bottom >= event->rect().top()){
            if(blockNumber == cursor_number){
                painter.fillRect(QRect(0, top, lineNumberArea->width(), height), Qt::yellow);
            }
            if(blockNumber == error_line){
                painter.fillRect(QRect(0, top, lineNumberArea->width(), height), Qt::red);
            }
            QString number = QString::number(blockNumber + 1);
            painter.setPen(palette.button().color());
            painter.drawText(0, top, lineNumberArea->width()-5, height, Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + blockBoundingRect(block).height();
        blockNumber++;
    }
}

void
DndEditor::insert_dnd_block(const QString& dndtext){
    if(isReadOnly())
        return;
    auto block = textCursor().block();
    auto text = block.text();
    auto leading_space = WHITESPACE_RE->match(text);
    if(leading_space.hasMatch()){
        auto lead = leading_space.captured();
        auto parts = dndtext.split('\n');
        if(lead.length() == text.length()){
            auto new_string = parts.join(QS("\n") + lead);
            insertPlainText(new_string);
        }
        else {
            auto new_string = QS("\n") + lead + parts.join(QS("\n") + lead);
            insertPlainText(new_string);
        }
    }
    else if(text.length()){
        insertPlainText(QS("\n") + dndtext);
    }
    else {
        insertPlainText(dndtext);
    }
}

void
DndEditor::insert_image(const QString& filename){
    insert_dnd_block(QS("::img\n  ") + filename + QS("\n"));
}

void
DndEditor::insert_dnd(const QString& filename){
    insert_dnd_block(QS("::import\n  ") + filename + QS("\n"));
}

void
DndEditor::insert_css(const QString& filename){
    insert_dnd_block(QS("::css\n  ") + filename + QS("\n"));
}

void
DndEditor::insert_script(const QString& filename){
    insert_dnd_block(QS("::script\n  ") + filename + QS("\n"));
}

void
DndEditor::insert_image_links(const QString& fullname, const QString& fname){
    auto img = QImage();
    img.load(fullname);
    auto size = img.size();
    auto w = size.width();
    auto h = size.height();
    auto scale = w>h? 800./w : 800./h;
    insert_dnd_block(QS(
            "::imglinks\n"
            "  %1\n"
            "  width = %2\n"
            "  height = %3\n"
            "  viewBox = 0 0 %4 %5\n"
            "  ::js\n"
            "    // this is an example of how to script the imglinks\n"
            "    let imglinks = node.parent;\n"
            "    let coord_nodes = ctx.select_nodes({attributes:['coord']});\n"
            "    for(let c of coord_nodes){\n"
            "      let lead = c.header;\n"
            "      let position = c.attributes.get('coord');\n"
            "      imglinks.add_child(`${lead} = #${c.id} @${position}`);\n"
            "    }\n"
          )
        .arg(fname)
        .arg((int)(w*scale))
        .arg((int)(h*scale))
        .arg(w)
        .arg(h));
}

void
DndEditor::alter_indent(bool indent){
    if(isReadOnly())
        return;
    auto cursor = textCursor();
    auto start = cursor.selectionStart();
    auto end = cursor.selectionEnd();
    auto doc = document();
    auto first_block = doc->findBlock(start);
    auto end_block = doc->findBlock(end);
    auto block = first_block;
    // Idk if this is the best way to do this, but I am just going to
    // build a list then join it.
    QStringList s;
    // use bounded loop out of paranoia
    for(int i = 0; i < 10000; i++){
        if(indent){
            s.append(QS("  "));
            s.append(block.text());
            s.append(QS("\n"));
        }
        else {
            auto text = block.text();
            if(text.startsWith(QS("  "))){
                s.append(text.right(text.size()-2));
            }
            else
                s.append(text);
            s.append(QS("\n"));
        }
        if(block.position() == end_block.position())
            break;
        block = block.next();
    }
    cursor.setPosition(first_block.position());
    cursor.setPosition(end_block.position() + end_block.text().size(), QTextCursor::KeepAnchor);
    auto str = s.join(QS(""));
    while(str.endsWith('\n') || str.endsWith(' '))
        str.chop(1);
    cursor.insertText(str);
}

void
DndEditor::contextMenuEvent(QContextMenuEvent* event){
    std::unique_ptr<QMenu> menu(createStandardContextMenu());
    QAction* action;
    action = new QAction(QS("Indent"), menu.get());
    connect(action, &QAction::triggered, [this](){
        alter_indent(true);
        });
    action->setShortcut(QKeySequence(QS("Ctrl+>")));
    menu->insertAction(menu->actions()[7], action);

    action = new QAction(QS("Dedent"), menu.get());
    connect(action, &QAction::triggered, [this](){
        alter_indent(false);
        });
    action->setShortcut(QKeySequence(QS("Ctrl+<")));
    menu->insertAction(menu->actions()[8], action);
    menu->exec(event->globalPos());
}

void append_room_with_name_at(const QString& name, int x, int y);
void change_coord(int id, int x, int y);
void scroll_to_id(QString nid);


QWebEngineUrlScheme* DndScheme;
class DndcSchemeHandler: public QWebEngineUrlSchemeHandler {
    virtual void requestStarted(QWebEngineUrlRequestJob* request) override {
        if(request->requestMethod() == QS("PUT")){
            auto path = request->requestUrl().path();
            // qDebug() << "path: " << path << "\n";
            auto components = path.split('/');
            if(components[1] == QS("roommove")){
                auto parts = components[components.length()-1].split('.');
                if(parts.length() != 3) return;
                auto id = parts[0].toInt();
                auto x = parts[1].toInt();
                auto y = parts[2].toInt();
                QTimer::singleShot(0, this, [=](){
                    change_coord(id, x, y);
                });
                return;
            }
            else if(components[1] == QS("roomclick")){
                if(components.length() != 4) return;
                auto name = components[2];
                auto parts = components[3].split('.');
                if(parts.length() != 2) return;
                auto x = parts[0].toInt();
                auto y = parts[1].toInt();
                QTimer::singleShot(0, this, [=](){
                    append_room_with_name_at(name, x, y);
                });
                return;
            }
            else if(components[1] == QS("scrolltoid")){
                if(components.length() != 3) return;
                auto nid = components[2];
                QTimer::singleShot(0, this, [=](){
                    scroll_to_id(nid);
                });
            }
            else {
                return;
            }
        }
        if(request->requestMethod() != QS("GET")){
            request->fail(QWebEngineUrlRequestJob::Error::RequestDenied);
            return;
        }
        auto url = request->requestUrl();
        auto imgpath = url.path();
        auto info = QFileInfo(imgpath);
        if(!info.exists()){
            request->fail(QWebEngineUrlRequestJob::Error::UrlNotFound);
            return;
        }
        QMimeDatabase db;
        QMimeType type = db.mimeTypeForFile(imgpath);
        auto file = new QFile(imgpath, request);
        request->reply(type.name().toUtf8(), file);
        return;
    }
};
void
create_scheme(void){
    DndScheme = new QWebEngineUrlScheme("dnd");
    DndScheme->setFlags(
        QWebEngineUrlScheme::Flag::SecureScheme
        | QWebEngineUrlScheme::Flag::LocalAccessAllowed
        #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        | QWebEngineUrlScheme::Flag::CorsEnabled
        #endif
        );
    DndScheme->setSyntax(QWebEngineUrlScheme::Syntax::Path);
    QWebEngineUrlScheme::registerScheme(*DndScheme);
    QWebEngineProfile::defaultProfile()->installUrlSchemeHandler("dnd", new DndcSchemeHandler());
}

DndWebPage::~DndWebPage(){
    // qDebug("DndWebPage dtor");
}

bool
DndWebPage::acceptNavigationRequest(const QUrl& url, QWebEnginePage::NavigationType navtype, bool isMainFrame){
    if(url.scheme() == QS("data"))
        return true;
    if(navtype == QWebEnginePage::NavigationType::NavigationTypeLinkClicked){
        auto path = url.path();
        auto host = url.host();
        if(host != APPHOST){
            QDesktopServices::openUrl(url);
            return false;
        }
        if(path.endsWith(QS(".dnd")) || path.endsWith(QS(".html"))){
            #if defined(_WIN32)
            if(path.startsWith('/') && path.size() > 2 && path[2] == ':')
                path.remove(0, 1);
            #endif
            if(path.endsWith(QS(".html")))
                path = path.left(path.length()-5) + QS(".dnd");
            auto filepath = QDir::cleanPath(path);
            auto info = QFileInfo(filepath);
            if(info.exists())
                add_tab(filepath);
            else {
                auto answer = QMessageBox::question(NULL, QS("Create file?"), filepath + QS(" does not exist. Create and open the file?"), QMessageBox::StandardButton::Yes|QMessageBox::StandardButton::No, QMessageBox::StandardButton::Yes);
                if(answer == QMessageBox::StandardButton::Yes){
                    QFile file(filepath);
                    if(!file.open(QFile::WriteOnly)){
                        return false;
                    }
                    file.close();
                    add_tab(filepath);
                    return false;
                }
            }
            return false;
        }
        return false;
    }
    return false;
}

bool
SplitterHandler::eventFilter(QObject* watched, QEvent* event){
    if(event->type() == QEvent::MouseButtonDblClick){
        auto handle = qobject_cast<QSplitterHandle*>(watched);
        if(handle->splitter()->widget(0)->width()){
            handle->splitter()->setSizes({0, 1});
        }
        else{
            handle->splitter()->setSizes({1, 10000});
        }
        return true;
    }
    return false;
}

SplitterHandler::~SplitterHandler(){
}

void
create_caches(void){
    watcher = new QFileSystemWatcher();
    b64cache = dndc_create_filecache();
    textcache = dndc_create_filecache();
    dndc_worker = dndc_worker_thread_create();
    QObject::connect(watcher, &QFileSystemWatcher::fileChanged, [](const QString& path){
        if(path.endsWith(QS("png"))){
            QWebEngineProfile::defaultProfile()->clearHttpCache();
        }
        auto bytes = path.toUtf8();
        DndcStringView sv = {(size_t)bytes.length(), bytes.data()};
        dndc_filecache_remove(b64cache, sv);
        dndc_filecache_remove(textcache, sv);
        if(QFileInfo::exists(path) && !watcher->files().contains(path))
            watcher->addPath(path);
        for(auto page: std::as_const(ALL_WINDOWS))
            page->file_changed(path);
    });
}


void
change_coord(int id, int x, int y){

    auto page = get_current_page();
    if(!page) return;
    if(page->textedit->isReadOnly()) return;
    auto text = page->textedit->toPlainText() + QS("\n");
    auto textbytes = text.toUtf8();
    DndcStringView textsv = {(size_t)textbytes.size(), textbytes.data()};
    CtxWrapper ctx = {dndc_create_ctx(0, NULL, NULL)};
    DndcNodeHandle root = dndc_ctx_make_root(ctx.ctx, {});
    int err = dndc_ctx_parse_string(ctx.ctx, root, {}, textsv);
    if(err) return;
    char buff[128];
    int nchars = snprintf(buff, sizeof buff, "%d,%d", x, y);
    DndcStringView sv = {(size_t)nchars, buff};
    err = dndc_node_set_attribute(ctx.ctx, id, {sizeof("coord")-1, "coord"}, sv);
    if(err) return;
    DndcLongString outstring;
    err = dndc_ctx_format_tree(ctx.ctx, &outstring);
    if(err) return;
    page->textedit->replace_source(QString::fromUtf8(outstring.text, outstring.length));
    dndc_free_string(outstring);
}

void
append_room_with_name_at(const QString& name, int x, int y){
    auto page = get_current_page();
    if(!page) return;
    if(page->textedit->isReadOnly())
        return;
    page->textedit->appendPlainText(QS("\n%1 ::md .room @coord(%2,%3)\n").arg(!name.contains('.')?name+'.':name).arg(x).arg(y));
    page->textedit->setFocus(Qt::FocusReason::NoFocusReason);
}

void
scroll_to_id(QString nid){
    // qDebug() << "nid: " << nid << "\n";
    auto page = get_current_page();
    if(!page) return;
    auto text = page->textedit->toPlainText() + QS("\n");
    auto textbytes = text.toUtf8();
    DndcStringView textsv = {(size_t)textbytes.size(), textbytes.data()};
    CtxWrapper ctx = {dndc_create_ctx(0, NULL, NULL)};
    DndcNodeHandle root = dndc_ctx_make_root(ctx.ctx, {});
    int err = dndc_ctx_parse_string(ctx.ctx, root, {}, textsv);
    if(err) return;

    auto nidbytes = nid.toUtf8();
    DndcStringView nidsv = {(size_t)nidbytes.size(), nidbytes.data()};
    DndcNodeHandle target = dndc_ctx_node_by_id(ctx.ctx, nidsv);
    if(target == DNDC_NODE_HANDLE_INVALID) return;
    DndcNodeLocation loc;
    err = dndc_node_location(ctx.ctx, target, &loc);
    if(err) return;
    // qDebug() << "loc.row: " << loc.row;
    page->scroll_to_line(loc.row);
}

Page::Page(QWidget*parent): QSplitter(parent) {
    webpage = new DndWebPage(this);
    web = new QWebEngineView(this);
    web->setPage(webpage);
    webpage->setHtml(QS(" "), QUrl(APPURL));
    // no idea why this is needed
    web->resize(400, 400);
    textedit = new DndEditor(this);
    textedit->setFont(*FONT);
    textedit->setMinimumSize(EIGHTYCHARS*1.01, 200);
    dirname = QS(".");
    connect(textedit->document(), &QTextDocument::contentsChanged, this, &Page::contents_changed);
    error_display = new QTextBrowser(this);
    error_display->setOpenLinks(false);
    connect(error_display, &QTextBrowser::anchorClicked, this, [](const QUrl& url){
        auto filename = url.toLocalFile();
        add_tab(filename);
        auto page = ALL_WINDOWS.value(filename, nullptr);
        auto location = url.fragment().split(':');
        if(page && location.size() == 2){
            page->show_editor();
            page->scroll_to_line(location[0].toInt(), location[1].toInt());
        }
    });
    error_display->setFont(*FONT);
    error_display->setReadOnly(true);
    editor_holder = new QSplitter(this);
    editor_holder->setOrientation(Qt::Orientation::Vertical);
    editor_holder->addWidget(textedit);
    editor_holder->addWidget(error_display);
    checks.append(new QCheckBox(QS("Auto-apply changes"), this));
    checks.append(new QCheckBox(QS("Read-only"), this));
    checks.append(new QCheckBox(QS("Coord helper"), this));
    checks[0]->setCheckState(Qt::CheckState::Checked);
    connect(checks[0], &QCheckBox::toggled, this, [this](bool checked){
        auto_apply_changed(checked ? Qt::Checked : Qt::Unchecked);
    });
    connect(checks[1], &QCheckBox::toggled, this, [this](bool checked){
        read_only_changed(checked ? Qt::Checked : Qt::Unchecked);
    });
    connect(checks[2], &QCheckBox::toggled, this, [this](bool checked){
        coord_helper_changed(checked ? Qt::Checked : Qt::Unchecked);
    });
    checkholder = new QWidget(this);
    checkholder_layout = new QHBoxLayout(checkholder);
    for(auto check : checks)
        checkholder_layout->addWidget(check);
    checkholder_layout->addStretch();
    checkholder->setLayout(checkholder_layout);
    editor_holder->addWidget(checkholder);
    editor_holder->setStretchFactor(0, 16);
    editor_holder->setStretchFactor(1, 1);
    editor_holder->setStretchFactor(2, 1);
    editor_holder->setParent(nullptr);
    editor_holder->hide();
    addWidget(web);
}

Page::~Page(){
    dispose_editor();
}

void
Page::keyPressEvent(QKeyEvent* event){
    // Idk if this is where I should be intercepting these.
    if(!(event->modifiers() & Qt::ControlModifier)){
        QSplitter::keyPressEvent(event);
        return;
    }
    auto key = event->key();
    if(key <= '9' && key >= '0'){
        auto v = key - '0';
        if(v == 0)
            v = 10;
        v--;
        if(TABS->count() > v){
            TABS->setCurrentIndex(v);
            return;
        }
    }
    QSplitter::keyPressEvent(event);
    return;
}
void
Page::contents_changed(void){
    if(auto_apply)
        update_html();
}

void
Page::auto_apply_changed(int state){
    if(state == Qt::CheckState::Unchecked)
        auto_apply = false;
    if(state == Qt::CheckState::Checked){
        auto_apply = true;
        update_html();
    }
}
void
Page::read_only_changed(int state){
    if(state == Qt::CheckState::Unchecked)
        textedit->setReadOnly(false);
    if(state == Qt::CheckState::Checked)
        textedit->setReadOnly(true);
    if(coord_helper) update_html();
}
void
Page::coord_helper_changed(int state){
    if(state == Qt::CheckState::Unchecked)
        coord_helper = false;
    if(state == Qt::CheckState::Checked)
        coord_helper = true;
    update_html();
}
void
Page::file_changed(const QString& path){
    if(!dependencies.contains(path))
        return;
    update_html();
}
void
Page::clear_errors(void){
    error_display->setPlainText(QS(""));
    textedit->error_line = -1;
}

void
Page::display_dndc_error(int error_type, const QString& filename, int row, int col, const QString& message){
    static const QString error_types[5] = {
        [DNDC_ERROR_MESSAGE]     = QS("Error"),
        [DNDC_WARNING_MESSAGE]   = QS("Warning"),
        [DNDC_NODELESS_MESSAGE]  = QS("Error"),
        [DNDC_STATISTIC_MESSAGE] = QS("Info"),
        [DNDC_DEBUG_MESSAGE]     = QS("Debug"),
    };
    if(error_type < 0 || error_type > 4)
        return;
    if(error_type == 0)
        textedit->error_line = row;
    const auto& et = error_types[error_type];
    if(error_type == DNDC_STATISTIC_MESSAGE){
        error_display->append((et + QS(": ") + message).toHtmlEscaped());
    }
    else {
        auto path = filename.isEmpty() ? this->filename : filename;
        if(QDir::isRelativePath(path)) path = QDir(dirname).absoluteFilePath(path);
        auto url = QUrl::fromLocalFile(QDir::cleanPath(path));
        url.setFragment(QS("%1:%2").arg(row+1).arg(col));
        error_display->append(QS("<a href=\"%1\">%2:%3:%4</a>: %5")
            .arg(url.toString().toHtmlEscaped()).arg(et).arg(row+1).arg(col+1).arg(message.toHtmlEscaped()));
    }
}

QString
Page::get_text_for_preview(void){
    auto text = textedit->toPlainText();
    if(coord_helper && !textedit->isReadOnly())
        text += QS("\n")+COORD_HELPER_SCRIPT;
    if(!coord_helper){
        text += QS("\n"
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
                    "            request.open('PUT', 'dnd:///scrolltoid/'+encodeURIComponent(target), true)\n"
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
        );
    }
    if(scroll_pos_string.length()){
        text += QS("\n::script\n  const SCROLLRESTO = ") + scroll_pos_string + QS("\n");
        text += SCROLL_RESTO_SCRIPT;
    }
    text += EXTRA_CSS;
    return text + QS("\n");
}

void
Page::update_html(void){
    // qDebug() << "In flight: " << inflight << "\n";
    if(inflight || closing){
        return;
    }
    inflight = true;
    QPointer<Page> page(this);
    webpage->runJavaScript(GET_SCROLL_POSITION_SCRIPT, 0, [page](const QVariant& x){
        if(page && !page->closing) page->set_scroll_pos(x.toString());
    });
}

void
Page::scroll_selection_into_view(void){
    int line = textedit->textCursor().blockNumber();
    line++;
    QString text = textedit->toPlainText() + QS("\n");
    auto textbytes = text.toUtf8();
    DndcStringView textsv = {(size_t)textbytes.size(), textbytes.data()};
    CtxWrapper ctx = {dndc_create_ctx(0, NULL, NULL)};
    DndcStringView fn = {1, "a"};
    DndcNodeHandle root = dndc_ctx_make_root(ctx.ctx, {});
    int err = dndc_ctx_parse_string(ctx.ctx, root, fn, textsv);
    if(err) return;
    DndcNodeHandle nh = dndc_ctx_node_by_approximate_location(ctx.ctx, fn, line, 0);
    while(nh != DNDC_NODE_HANDLE_INVALID && nh != root && !dndc_node_has_id(ctx.ctx, nh)){
        nh = dndc_node_get_parent(ctx.ctx, nh);
    }
    if(nh == DNDC_NODE_HANDLE_INVALID) return;
    if(nh == root) return;
    DndcStringView idstr;
    if(dndc_node_get_id(ctx.ctx, nh, &idstr) != 0) return;
    if(idstr.length > 509) return;
    char buff[512];
    size_t buffused = 0;
    if(dndc_kebab(idstr, buff, sizeof buff, &buffused) != 0) return;
    if(!buffused) return;
    buff[buffused] = 0;
    webpage->runJavaScript(QS(
            "(function(){\n"
            "  let node = document.getElementById('%1');\n"
            "  if(!node) return;\n"
            "  node.scrollIntoView(true);\n"
            "})();\n").arg(QString(buff)));
}

void
Page::set_scroll_pos(QString&& x){
    // This whole set up is kind of janksville.
    // qDebug() << "Set scroll pos\n";;
    scroll_pos_string = x;
    inflight = false;
    clear_errors();
    // TODO: public api doesn't give the delta in paths.
    // In theory though I should be doing all the file loading
    // as the host instead of dndc.
    // before_paths = set(FILE_CACHE.paths())
    unsigned long long flags = 0
        | DNDC_USE_DND_URL_SCHEME
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    if(PRINT_STATS)
        flags |= DNDC_PRINT_STATS;
    // SAD: can't factor this into a function as we need textbytes to outlive textls
    auto text = get_text_for_preview();
    auto textbytes = text.toUtf8();
    DndcStringView textsv = {(size_t)textbytes.size(), textbytes.data()};
    auto dirbytes = dirname.toUtf8();
    DndcStringView basedir = {(size_t)dirbytes.size(), dirbytes.data()};
    auto fnbytes = filename.toUtf8();
    DndcStringView srcpath = {(size_t)fnbytes.size(), fnbytes.data()};
    DndcLongString outstring;
    DndcLogFunc* errfunc = [](
            void* user_data,
            int type,
            const char* filename, int filename_len,
            int line, int col,
            const char* message, int message_len
        ) -> void {
            auto page = (Page*)user_data;
            // qDebug() <<  type << " " << QString::fromUtf8(filename, filename_len) <<  ":" << line << ":" <<col<< ":" << QString::fromUtf8(message, message_len);

            page->display_dndc_error(type, QString::fromUtf8(filename, filename_len), line, col, QString::fromUtf8(message, message_len));
        };
    CtxWrapper ctx = CtxWrapper::create(flags, b64cache, textcache);
    dndc_ctx_set_logger(ctx.ctx, errfunc, this);
    dndc_ctx_set_base(ctx.ctx, basedir);
    DndcNodeHandle root = dndc_ctx_make_root(ctx.ctx, srcpath);
    int err = 0;
    err = dndc_ctx_parse_string(ctx.ctx, root, srcpath, textsv);
    if(err) return;
    err = dndc_ctx_resolve_imports(ctx.ctx);
    if(err) return;
    err = dndc_ctx_execute_js(ctx.ctx, {0, ""});
    if(err) return;
    err = dndc_ctx_resolve_links(ctx.ctx);
    if(err) return;
    err = dndc_ctx_build_toc(ctx.ctx);
    if(err) return;
    err = dndc_ctx_render_to_html(ctx.ctx, &outstring);
    if(err) return;

    {
        QSet<QString> new_deps;
        constexpr size_t deplen = 64;
        DndcStringView deps[deplen];
        size_t n = 0;
        size_t cookie = 0;
        while((n=dndc_ctx_get_dependencies(ctx.ctx, deps, deplen, &cookie))){
            for(size_t i = 0; i < n; i++){
                auto dep = deps[i];
                QString str = QString::fromUtf8(dep.text, dep.length);
                if(!dependencies.contains(str))
                    watcher->addPath(str);
                new_deps.insert(std::move(str));
            }
        }
        dependencies = std::move(new_deps);
    }

    // qDebug() << "err:" << err;
    QUrl url;
    url.setScheme(QS("https"));
    url.setHost(APPHOST);
    auto path = QDir::fromNativeSeparators(filename);
    if(!path.startsWith('/')) path.prepend('/');
    url.setPath(path);
    webpage->setHtml(QString::fromUtf8(outstring.text, outstring.length), url);
    dndc_free_string(outstring);
}

void
Page::format(void){
    if(textedit->isReadOnly()) return;
    auto text = textedit->toPlainText() + QS("\n");
    auto textbytes = text.toUtf8();
    DndcStringView textsv = {(size_t)textbytes.size(), textbytes.data()};
    DndcLogFunc* errfunc = [](
            void* user_data,
            int type,
            const char* filename, int filename_len,
            int line, int col,
            const char* message, int message_len
        ) -> void {
            auto page = (Page*)user_data;
            page->display_dndc_error(type, QString::fromUtf8(filename, filename_len), line, col, QString::fromUtf8(message, message_len));
        };
    DndcLongString outstring;
    auto pathbytes = filename.toUtf8();
    DndcStringView source_path = {(size_t)pathbytes.size(), pathbytes.data()};
    int err = dndc_format2(textsv, source_path, &outstring, errfunc, this);
    if(err) return;
    textedit->replace_source(QString::fromUtf8(outstring.text, outstring.length));
    dndc_free_string(outstring);
}
void
Page::hide_editor(void){
    editor_open = false;
    if(editor_window){
        bool focused = editor_window->isActiveWindow();
        editor_window->sync_visibility();
        if(focused){
            TABS->window()->activateWindow();
            web->setFocus();
        }
    }
}
void
Page::show_editor(void){
    TABS->setCurrentWidget(this);
    editor_open = true;
    if(!editor_window){
        editor_window = new EditorWindow(this, TABS->window());
        editor_holder->show();
    }
    editor_window->sync_visibility(true);
}
void
Page::dispose_editor(void){
    if(closing) return;
    closing = true;
    editor_open = false;
    if(editor_window){
        editor_window->detach();
        delete editor_window;
        editor_window = nullptr;
        editor_holder = nullptr;
    }
    else if(editor_holder){
        delete editor_holder;
        editor_holder = nullptr;
    }
}
void
Page::scroll_to_line(int line, int column){
    // Preview navigation updates the selection without changing window visibility.
    auto block = textedit->document()->findBlockByLineNumber(qMax(0, line-1));
    if(block.isValid()){
        QTextCursor cursor(block);
        cursor.setPosition(block.position() + qBound(0, column, block.length()-1));
        textedit->setTextCursor(cursor);
        textedit->ensureCursorVisible();
    }
}
void
Page::show_error(void){
    error_display->show();
    show_errors = true;
}
void
Page::hide_error(void){
    clear_errors();
    error_display->hide();
    show_errors = false;
}
bool
Page::save(void){
    if(filename.isEmpty() || !textedit->document()->isModified()) return true;
    QSaveFile savefile(this);
    savefile.setFileName(filename);
    if(!savefile.open(QSaveFile::WriteOnly)){
        QMessageBox::critical(editor_window ? static_cast<QWidget*>(editor_window) : TABS->window(), QS("Unable to save"), savefile.errorString());
        return false;
    }
    auto text = textedit->toPlainText();
    if(!text.endsWith('\n')) text += '\n';
    auto bytes = text.toUtf8();
    if(savefile.write(bytes) != bytes.size() || !savefile.commit()){
        QMessageBox::critical(editor_window ? static_cast<QWidget*>(editor_window) : TABS->window(), QS("Unable to save"), savefile.errorString());
        return false;
    }
    textedit->document()->setModified(false);
    return true;
}
bool
Page::get_fname(const QString& title, const QString& filter, QString* out){
    auto filename = QFileDialog::getOpenFileName(this, title, QString(), filter);
    if(filename.length()){
        *out = std::move(filename);
        return true;
        }
    return false;
    }

void
Page::insert_image(void){
    QString fname;
    if(!get_fname(QS("Choose an image file"), QS("PNG images (*.png)"), &fname))
        return;
    textedit->insert_image(fname);
    }

void
Page::insert_image_links(void){
    QString fname;
    if(!get_fname(QS("Choose an image file"), QS("PNG images (*.png)"), &fname))
        return;
    textedit->insert_image_links(fname, fname);
    }

void
Page::insert_dnd(void){
    QString fname;
    if(!get_fname(QS("Choose a dnd file"), QS("Dnd files (*.dnd)"), &fname))
        return;
    textedit->insert_dnd(fname);
    }
void
Page::insert_css(void){
    QString fname;
    if(!get_fname(QS("Choose a css file"), QS("CSS files (*.css)"), &fname))
        return;
    textedit->insert_css(fname);
    }
void
Page::insert_script(void){
    QString fname;
    if(!get_fname(QS("Choose a JavaScript file"), QS("JS files (*.js)"), &fname))
        return;
    textedit->insert_script(fname);
    }

void
Page::export_as_html(void){
    clear_errors();
    unsigned long long flags = 0
        | DNDC_ALLOW_BAD_LINKS
        ;
    // SAD: can't factor this into a function as we need textbytes to outlive textls
    // Get the text directly, without any of the helpers inserted.
    auto text = textedit->toPlainText();
    auto textbytes = text.toUtf8();
    DndcStringView textsv = {(size_t)textbytes.size(), textbytes.data()};
    auto dirbytes = dirname.toUtf8();
    DndcStringView basedir = {(size_t)dirbytes.size(), dirbytes.data()};
    // TODO: change to where it is going
    DndcLongString outstring;
    DndcLogFunc* errfunc = [](
            void* user_data,
            int type,
            const char* filename, int filename_len,
            int line, int col,
            const char* message, int message_len
        ) -> void {
            auto page = (Page*)user_data;
            page->display_dndc_error(type, QString::fromUtf8(filename, filename_len), line, col, QString::fromUtf8(message, message_len));
        };

    int err = dndc_compile_dnd_file(
            flags,
            basedir,
            textsv,
            DndcStringView{},
            &outstring,
            b64cache, textcache,
            errfunc, this,
            dndc_worker, {0, ""});
    if(err) {
        QMessageBox mbox;
        mbox.critical(
            this,
            QS("Unable to convert current document"),
            QS("Unable to convert current document to html.\n\nSyntax Error in document (see error output)."));
        return;
        }
    auto options = QFileDialog::DontConfirmOverwrite;
    auto fname = QFileDialog::getSaveFileName(this, QS("Choose where to save html"), QS(""), QS("HTML files (*.html)"), nullptr, options);
    if(!fname.length())
        return;
    if(!fname.endsWith(QS(".html")))
        fname += QS(".html");
    QSaveFile savefile(this);
    savefile.setFileName(fname);
    if(!savefile.open(QSaveFile::WriteOnly)){
        QMessageBox::critical(this, QS("Unable to export"), savefile.errorString());
        dndc_free_string(outstring);
        return;
    }
    auto bytes = QByteArray(outstring.text, (qsizetype)outstring.length);
    if(savefile.write(bytes) != bytes.size() || !savefile.commit())
        QMessageBox::critical(this, QS("Unable to export"), savefile.errorString());
    dndc_free_string(outstring);
    }


Page*
make_page_widget(QWidget* parent, const QString& filename, bool allow_fail){
    if(ALL_WINDOWS.contains(filename))
        return nullptr;
    QFile file(filename);
    QString text;
    bool new_file = false;
    if(file.open(QFile::ReadOnly)){
        QTextStream stream(&file);
        text = stream.readAll();
        if(stream.status() != QTextStream::Ok) return nullptr;
    }
    else if(allow_fail && !QFileInfo::exists(filename)){
        new_file = true;
    }
    else {
        QMessageBox::critical(parent, QS("Unable to open"), file.errorString());
        return nullptr;
    }
    Page* result = new Page(parent);
    if(text.endsWith(QS("\n")))
        text.chop(1);
    result->textedit->setPlainText(text);
    result->textedit->document()->setModified(new_file);
    auto info = QFileInfo(file);
    result->dirname = info.absoluteDir().absolutePath();
    result->filename = filename;
    result->webpage->basedir = result->dirname;
    result->update_html();
    ALL_WINDOWS[filename] = result;
    return result;
    }

QString
condense(const QString& filename){
    return filename;
    }

void
add_tab(const QString& path, bool focus, bool allow_fail){
    const auto filename = QFileInfo(path).absoluteFilePath();
    if(ALL_WINDOWS.contains(filename)){
        if(focus)
            TABS->setCurrentWidget(ALL_WINDOWS[filename]);
        return;
        }
    auto page = make_page_widget(TABS, filename, allow_fail);
    if(!page){
        qDebug() << "Failed to make tab for" << filename;
        return;
    }
    qDebug() << "Opened tab for" << filename;
    auto url = QUrl(filename);
    TABS->addTab(page, url.fileName());
    if(focus)
        TABS->setCurrentWidget(page);
    }




int
main(int argc, char** argv)
{
    auto logfolder = QDir(
            QStandardPaths::writableLocation(QStandardPaths::StandardLocation::AppLocalDataLocation)
            + '/'
            + APPNAME
            + '/'
            + QS("Logs")
            );
    if(!logfolder.mkpath(".")){
        QMessageBox::critical(
                nullptr,
                QS("Unable to create log directory"),
                QS("Unable to create log directory at ") + logfolder.absolutePath());
        }
    else {
        }
    LOGS_FOLDER = logfolder.absolutePath();
    auto logfile = QDir(LOGS_FOLDER + '/' + QDate::currentDate().toString(Qt::ISODate)+QS(".txt")).absolutePath();


    LOGFILE = new QFile(logfile);
    if(!LOGFILE->open(QFile::Append|QFile::WriteOnly|QFile::Text|QFile::Unbuffered)){
        fprintf(stderr, "Unable to open log: %s\n", qPrintable(LOGFILE->errorString()));
        return 1;
    }
    LOGSTREAM = new QTextStream(LOGFILE);
    qInstallMessageHandler(DNDC_LOGIT);
    qInfo("--------------------");
    qInfo("Starting new session");
    qInfo("Dndc Version: %s", DNDC_VERSION);
    QApplication a(argc, argv);
    a.setApplicationDisplayName(QS("DndcEdit"));
    a.setApplicationName(QS("DndcEdit"));
    create_scheme();
    create_caches();
    WHITESPACE_RE = new QRegularExpression(QS("^\\s+"));
    FONT = new QFont();
    #ifdef _WIN32
        FONT->setPointSize(8);
    #else
        FONT->setPointSize(11);
    #endif
    FONT->setFixedPitch(true);
    #if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
        QStringList fonts = {QS("Menlo"), QS("Cascadia Mono"), QS("Consolas"), QS("Ubuntu Mono"), QS("Mono")};
        FONT->setFamilies(fonts);
    #else
        FONT->setFamily(QS("Menlo"));
    #endif
    FONTMETRICS = new QFontMetrics(*FONT);
    EIGHTYCHARS = FONTMETRICS->horizontalAdvance('M')*80;
    SETTINGS = new QSettings(QS("DavidTechnology"), APPNAME);

    MainWindow w;
    TABS = new QTabWidget(&w);
    TABS->setTabsClosable(true);
    TABS->setDocumentMode(true);
    QObject::connect(TABS, &QTabWidget::tabCloseRequested, close_tab);
    QObject::connect(TABS, &QTabWidget::currentChanged, [](){ sync_editors(); });
    w.setCentralWidget(TABS);
    w.restore_everything();
    if(!TABS->currentWidget())
        w.open_file();
    w.add_menus();
    for(auto action: w.findChildren<QAction*>())
        if(!action->shortcut().isEmpty()) action->setShortcutContext(Qt::ApplicationShortcut);
    for(int index = 0; index < 10; index++){
        auto action = new QAction(&w);
        action->setShortcut(QKeySequence(QS("Ctrl+%1").arg((index+1)%10)));
        action->setShortcutContext(Qt::ApplicationShortcut);
        QObject::connect(action, &QAction::triggered, [index](){
            if(index < TABS->count()){
                TABS->setCurrentIndex(index);
                sync_editors();
            }
        });
        w.addAction(action);
    }
    w.show();
    auto ret = a.exec();
    qDebug("Shutdown normal.");
    LOGFILE->flush();
    LOGFILE->close();
    return ret;
}
