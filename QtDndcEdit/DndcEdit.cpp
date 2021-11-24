#include "DndcEdit.h"
#include <Dndc/dndc.h>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QRegularExpressionMatch>
#include <QtCore/QSettings>
#include <QtGui/QDesktopServices>
#include <QtGui/QPalette>
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
#define QS(x) QStringLiteral(x)
const QString APPHOST = QS("invalid.");
const QString APPURL = QS("https://invalid./this.html");
const QString APPNAME = QS("DndEdit");
QString LOGS_FOLDER;
QFont* FONT;
QFontMetrics* FONTMETRICS;
int EIGHTYCHARS;
bool EDITOR_ON_LEFT = true;
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
    "  document.addEventListener('DOMContentLoaded', function(){\n"
    "    const svgs = document.getElementsByTagName('svg');\n"
    "    for(let i = 0; i < svgs.length; i++){\n"
    "      const svg = svgs[i];\n"
    "      const texts = svg.getElementsByTagName('text');\n"
    "      var text_height = 0;\n"
    "      if(texts.length){\n"
    "          const first_text = texts[0];\n"
    "          const text_height = first_text.getBBox().height || 0;\n"
    "          }\n"
    "      svg.addEventListener('click', function(e){\n"
    "        const number = prompt('Enter Room Name');\n"
    "        if(number){\n"
    "          const x_scale = svg.width.baseVal.value / svg.viewBox.baseVal.width;\n"
    "          const y_scale = svg.height.baseVal.value / svg.viewBox.baseVal.height;\n"
    "          const rect = e.currentTarget.getBoundingClientRect();\n"
    "          const true_x = ((e.clientX - rect.x)/ x_scale) | 0;\n"
    "          const true_y = (((e.clientY - rect.y)/ y_scale) + text_height/2) | 0;\n"
    "          let request = new XMLHttpRequest();\n"
    "          const combined = number + ',' + true_x + ',' + true_y;\n"
    "          request.open('PUT', 'dnd:///'+combined, true);\n"
    "          request.send();\n"
    "        }\n"
    "      });\n"
    "    }\n"
    "  });\n"
    );

QString SCROLL_RESTO_SCRIPT = QS(
    "::script\n"
    "    document.addEventListener('DOMContentLoaded', function(){\n"
    "        console.log(SCROLLRESTO);\n"
    "        for(let [key, value] of Object.entries(SCROLLRESTO)){\n"
    "            if(key == 'html'){\n"
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
    auto page_ = TABS->currentWidget();
    if(!page_) return nullptr;
    auto page = qobject_cast<Page*>(page_);
    return page;
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
    SETTINGS->setValue(QS("editor_on_left"), EDITOR_ON_LEFT);
    SETTINGS->setValue(QS("window_geometry"), saveGeometry());
    for(auto page: qAsConst(ALL_WINDOWS))
        page->save();
    e->accept();
    }

void
MainWindow::restore_everything(){
    auto bytes = SETTINGS->value(QS("window_geometry")).toByteArray();
    if(bytes.length())
     restoreGeometry(SETTINGS->value(QS("window_geometry")).toByteArray());
    else
        showMaximized();
    auto von_left = SETTINGS->value(QS("editor_on_left"));
    if(von_left.canConvert<bool>())
        EDITOR_ON_LEFT = von_left.toBool();
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
    connect(action, &QAction::triggered, [this](){
            auto page = get_current_page();
            if(!page) return;
            page->save();
        });
    action->setShortcut(QKeySequence(QS("Ctrl+s")));
    filemenu->addAction(action);

    action = new QAction(QS("&Export As HTML"), this);
    connect(action, &QAction::triggered, [this](){
            auto page = get_current_page();
            if(!page) return;
            page->export_as_html();
            });
    action->setShortcut(QKeySequence(QS("Ctrl+e")));
    filemenu->addAction(action);

    action = new QAction(QS("&Close"), this);
    connect(action, &QAction::triggered, [this](){
            auto page = get_current_page();
            if(!page){
                close();
                return;
                }
            page->save();
            ALL_WINDOWS.remove(page->filename);
            page->setParent(nullptr);
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
    connect(action, &QAction::triggered, [this](){
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
            for(auto page: qAsConst(ALL_WINDOWS))
                page->textedit->setFont(*FONT);
            });
    editmenu->addAction(action);

    action = new QAction(QS("&Indent"), this);
    connect(action, &QAction::triggered, [this](){
            auto page = get_current_page();
            if(!page) return;
            page->textedit->alter_indent(true);
            });
    action->setShortcut(QKeySequence(QS("Ctrl+>")));
    editmenu->addAction(action);

    action = new QAction(QS("&Dedent"), this);
    action->setShortcut(QKeySequence(QS("Ctrl+<")));
    connect(action, &QAction::triggered, [this](){
            auto page = get_current_page();
            if(!page) return;
            page->textedit->alter_indent(false);
            });
    editmenu->addAction(action);

    auto insert = menubar->addMenu(QS("Insert"));
    #define INSERT(name, method) do { \
        action = new QAction(QS(name), this); \
        connect(action, &QAction::triggered, [this](){ \
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
    action = new QAction("&Toggle Editors", this);
    connect(action, &QAction::triggered, [this](){
        bool first_is_hidden = false;
        bool checked = false;
        for(auto page: qAsConst(ALL_WINDOWS)){
            if(!checked){
                checked = true;
                first_is_hidden = page->isHidden();
                }
            if(first_is_hidden)
                page->show_editor();
            else
                page->hide_editor();
            }
        });
    viewmenu->addAction(action);

    action = new QAction("&Flip Editors", this);
    connect(action, &QAction::triggered, [this](){
        for(auto page: qAsConst(ALL_WINDOWS)){
            if(EDITOR_ON_LEFT) page->put_editor_right();
            else page->put_editor_left();
            }
        EDITOR_ON_LEFT = !EDITOR_ON_LEFT;
        });
    viewmenu->addAction(action);

    action = new QAction("&Refresh Highlighting", this);
    connect(action, &QAction::triggered, [this](){
        auto page = get_current_page();
        if(!page) return;
        page->textedit->highlight->rehighlight();
        });
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
    connect(action, &QAction::triggered, [this](){
            dndc_filecache_clear(b64cache);
            dndc_filecache_clear(textcache);
            QWebEngineProfile::defaultProfile()->clearHttpCache();
            for(auto page: qAsConst(ALL_WINDOWS))
                page->update_html();
        });
    developmenu->addAction(action);

    action = new QAction("&Recalculate HTML", this);
    connect(action, &QAction::triggered, [this](){
            auto page = get_current_page();
            if(!page) return;
            page->update_html();
        });
    developmenu->addAction(action);

    action = new QAction("&Toggle Timings", this);
    connect(action, &QAction::triggered, [this](){
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
    int digits = 1;
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
            "      imglinks.add_child(`${lead} = ${ctx.outfile}#${c.id} @${position}`);\n"
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
    auto menu = createStandardContextMenu();
    QAction* action;
    action = new QAction(QS("Indent"), menu);
    connect(action, &QAction::triggered, [this](){
        alter_indent(true);
        });
    action->setShortcut(QKeySequence(QS("Ctrl+>")));
    menu->insertAction(menu->actions()[7], action);

    action = new QAction(QS("Dedent"), menu);
    connect(action, &QAction::triggered, [this](){
        alter_indent(false);
        });
    action->setShortcut(QKeySequence(QS("Ctrl+<")));
    menu->insertAction(menu->actions()[8], action);
    menu->exec(event->globalPos());
    }

void append_room_with_name_at(const QString& name, int x, int y);


QWebEngineUrlScheme* DndScheme;
class DndcSchemeHandler: public QWebEngineUrlSchemeHandler {
    virtual void requestStarted(QWebEngineUrlRequestJob* request) override {
        if(request->requestMethod() == QS("PUT")){
            auto path = request->requestUrl().path();
            auto parts = path.split(',');
            auto length = parts.length();
            if(length < 3)
                return;
            auto x = parts[length-2].toInt();
            auto y = parts[length-1].toInt();
            parts.removeLast();
            parts.removeLast();
            auto name = parts.join(',');
            QTimer::singleShot(0, this, [=](){
                append_room_with_name_at(name, x, y);
                });
            return;
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
        if(path.endsWith(QS(".html"))){
            #if defined(_WIN32)
            path = path.right(path.length()-1);
            #endif
            auto filepath = QDir::cleanPath(path.left(path.length()-5)+QS(".dnd"));
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
    });
}



void
append_room_with_name_at(const QString& name, int x, int y){
    auto page = get_current_page();
    if(!page) return;
    if(page->textedit->isReadOnly())
        return;
    page->textedit->appendPlainText(QS("\n%1 ::md .room @coord(%2,%2)\n").arg(!name.contains('.')?name+'.':name).arg(x).arg(y));
    page->textedit->setFocus(Qt::FocusReason::NoFocusReason);
    }

void add_tab(const QString& filename){
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
    error_display = new QPlainTextEdit(this);
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
    connect(checks[0], &QCheckBox::stateChanged, this, &Page::auto_apply_changed);
    connect(checks[1], &QCheckBox::stateChanged, this, &Page::read_only_changed);
    connect(checks[2], &QCheckBox::stateChanged, this, &Page::coord_helper_changed);
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
    bool left = EDITOR_ON_LEFT;
    bool show_errors = true;
    // Aren't these globals? Why am I doing this
    if(ALL_WINDOWS.size()){
        auto first_window = ALL_WINDOWS.constKeyValueBegin();
        left = (*first_window).second->editor_is_on_left;
        show_errors = (*first_window).second->show_errors;
        }
    if(left)
        put_editor_left();
    else
        put_editor_right();
    if(show_errors)
        show_error();
    else
        hide_error();
    handle(1)->installEventFilter(new SplitterHandler(this));
}
Page::~Page(){
    // qDebug("Page dtor");
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
    }
void
Page::coord_helper_changed(int state){
    if(state == Qt::CheckState::Unchecked)
        coord_helper = false;
    if(state == Qt::CheckState::Checked){
        coord_helper = true;
        update_html();
        }
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
        error_display->appendPlainText(et + QS(": ") + message);
        }
    else {
        error_display->appendPlainText(QS("%1:%2:%3: %4").arg(et).arg(row+1).arg(col+1).arg(message));
        }
    }
QString
Page::get_text_for_preview(void){
    auto text = textedit->toPlainText();
    if(coord_helper && !textedit->isReadOnly())
        text += QS("\n")+COORD_HELPER_SCRIPT;
    if(scroll_pos_string.length()){
        text += QS("\n::script\n  const SCROLLRESTO = ") + scroll_pos_string + QS("\n");
        text += SCROLL_RESTO_SCRIPT;
        }
    return text + QS("\n");
    }
void
Page::update_html(void){
    if(inflight)
        return;
    inflight = true;
    webpage->runJavaScript(GET_SCROLL_POSITION_SCRIPT, 0, [this](const QVariant& x){

        set_scroll_pos(std::move(x.toString()));
        });
    }
void
Page::add_dependencies(size_t count, DndcStringView* paths){
    QSet<QString> new_deps;
    for(size_t i = 0; i < count; i++){
        auto sv = paths[i];
        QString str = QString::fromUtf8(sv.text, sv.length);
        if(!dependencies.contains(str)){
            watcher->addPath(str);
            }
        new_deps.insert(std::move(str));
        }
    dependencies = std::move(new_deps);
    }
void
Page::set_scroll_pos(QString&& x){
    // This whole set up is kind of janksville.
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
        ;
    if(PRINT_STATS)
        flags |= DNDC_PRINT_STATS;
    // SAD: can't factor this into a function as we need textbytes to outlive textls
    auto text = get_text_for_preview();
    auto textbytes = text.toUtf8();
    DndcLongString textls = {(size_t)textbytes.size(), textbytes.data()};
    auto dirbytes = dirname.toUtf8();
    DndcLongString basedir = {(size_t)dirbytes.size(), dirbytes.data()};
    auto fnbytes = filename.toUtf8();
    DndcLongString outpath = {(size_t)fnbytes.size(), fnbytes.data()};
    // DndcLongString outpath = {sizeof("this.html")-1, "this.html"};
    DndcLongString outstring;
    DndcErrorFunc* errfunc = [](
            void* user_data,
            int type,
            const char* filename, int filename_len,
            int line, int col,
            const char* message, int message_len
        ) -> void {
            auto page = (Page*)user_data;
            page->display_dndc_error(type, QString::fromUtf8(filename, filename_len), line, col, QString::fromUtf8(message, message_len));
        };
    DndcDependencyFunc* depfunc = [](
            void* user_data,
            size_t count,
            DndcStringView* paths
        )->int {
            auto page = (Page*)user_data;
            page->add_dependencies(count, paths);
            return 0;
        };

    int err = dndc_compile_dnd_file(
            flags,
            basedir, textls, outpath,
            &outstring,
            b64cache, textcache,
            errfunc, this,
            depfunc, this,
            dndc_worker);
    if(err) {
        return;
        }
    auto url = QUrl(QS("https://") + APPHOST + QS("/") + filename);
    webpage->setHtml(QString::fromUtf8(outstring.text, outstring.length), url);
    dndc_free_string(outstring);
    }
void
Page::format(void){
    auto text = get_text_for_preview();
    auto textbytes = text.toUtf8();
    DndcLongString textls = {(size_t)textbytes.size(), textbytes.data()};
    DndcErrorFunc* errfunc = [](
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
    unsigned long long flags = 0
        | DNDC_REFORMAT_ONLY
        ;
    if(PRINT_STATS)
        flags |= DNDC_PRINT_STATS;
    int err = dndc_compile_dnd_file(
            flags,
            DndcLongString{}, textls, DndcLongString{},
            &outstring,
            nullptr, nullptr,
            errfunc, this,
            nullptr, nullptr,
            nullptr);
    if(err) return;
    textedit->setPlainText(QString::fromUtf8(outstring.text, outstring.length));
    dndc_free_string(outstring);
    }
void
Page::hide_editor(void){
    editor_holder->hide();
    }
void
Page::show_editor(void){
    editor_holder->show();
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
void
Page::put_editor_right(void){
    editor_holder->setParent(nullptr);
    web->setParent(nullptr);
    addWidget(web);
    addWidget(editor_holder);
    editor_is_on_left = false;
    }
void
Page::put_editor_left(void){
    editor_holder->setParent(nullptr);
    web->setParent(nullptr);
    addWidget(editor_holder);
    addWidget(web);
    editor_is_on_left = true;
    }
void
Page::save(void){
    if(!filename.length())
        return;
    QSaveFile savefile(this);
    savefile.setFileName(filename);
    savefile.open(QSaveFile::WriteOnly);
    auto text = textedit->toPlainText();
    if(!text.endsWith(QS("\n")))
        text += '\n';
    savefile.write(text.toUtf8());
    savefile.commit();
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
    DndcLongString textls = {(size_t)textbytes.size(), textbytes.data()};
    auto dirbytes = dirname.toUtf8();
    DndcLongString basedir = {(size_t)dirbytes.size(), dirbytes.data()};
    // TODO: change to where it is going
    DndcLongString outpath = {sizeof("this.html")-1, "this.html"};
    DndcLongString outstring;
    DndcErrorFunc* errfunc = [](
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
            basedir, textls, outpath,
            &outstring,
            b64cache, textcache,
            errfunc, this,
            nullptr, nullptr,
            dndc_worker);
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
    savefile.open(savefile.WriteOnly);
    savefile.write(QByteArray(outstring.text, (qsizetype)outstring.length));
    savefile.commit();
    dndc_free_string(outstring);
    }


Page*
make_page_widget(QWidget* parent, const QString& filename, bool allow_fail){
    if(ALL_WINDOWS.contains(filename))
        return nullptr;
    QFile file(filename);
    if(!file.open(QFile::ReadOnly))
        return nullptr;
    Page* result = new Page(parent);
    QTextStream stream(&file);
    auto text = stream.readAll();
    if(text.endsWith(QS("\n")))
        text.chop(1);
    result->textedit->setPlainText(text);
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
add_tab(const QString& filename, bool focus, bool allow_fail){
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
    LOGFILE->open(QFile::Append|QFile::WriteOnly|QFile::Text|QFile::Unbuffered);
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
    QObject::connect(TABS, &QTabWidget::tabCloseRequested, [](int index){
        // close_tab
        auto page = qobject_cast<Page*>(TABS->widget(index));
        page->save();
        page->close();
        TABS->removeTab(index);
        ALL_WINDOWS.remove(page->filename);
        page->setParent(nullptr);
        page->deleteLater();
    });
    w.setCentralWidget(TABS);
    w.restore_everything();
    if(!TABS->currentWidget())
        w.open_file();
    w.add_menus();
    w.show();
    auto ret = a.exec();
    qDebug("Shutdown normal.");
    LOGFILE->flush();
    LOGFILE->close();
    return ret;
}
