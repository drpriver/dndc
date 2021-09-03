#include <Dndc/dndc.h>
#include <QMainWindow>
#include <QSplitter>
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QPainter>
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void restore_everything();
    virtual void closeEvent(QCloseEvent* e) override;
    void add_menus(void);
    void open_file(void);
};

struct HighlightRegion {
    int type, col, length;
    };
class DndSyntaxHighlighter: public QSyntaxHighlighter
{
    Q_OBJECT
    QHash<int, QList<HighlightRegion>> highlight_regions;
    QString light_color_names[8] = {
        [0] = "salmon",
        [DNDC_SYNTAX_DOUBLE_COLON] =  "darkgray",
        [DNDC_SYNTAX_HEADER] = "blue",
        [DNDC_SYNTAX_NODE_TYPE] = "lightslategray",
        [DNDC_SYNTAX_ATTRIBUTE] = "lightsteelblue",
        [DNDC_SYNTAX_ATTRIBUTE_ARGUMENT] = "darkkhaki",
        [DNDC_SYNTAX_CLASS] = "burlywood",
        [DNDC_SYNTAX_RAW_STRING] = "gold",
    };

public:
    ~DndSyntaxHighlighter();
        DndSyntaxHighlighter(QTextDocument* parent): QSyntaxHighlighter(parent), highlight_regions(){
            }
        virtual void highlightBlock(const QString& text) override{
            auto block = currentBlock();
            auto line = block.blockNumber();
            if(!highlight_regions.contains(line))
                return;
            auto fmt = QTextCharFormat();
            auto color = QColor();
            auto& color_names = light_color_names;
            for(const auto& region: highlight_regions[line]){
                color.setNamedColor(color_names[region.type]);
                fmt.setForeground(color);
                setFormat(region.col, region.length, fmt);
                }
            }
        void update_regions(QHash<int, QList<HighlightRegion>>&& new_regions){
            highlight_regions = new_regions;
            }
};

class LineNumberArea : public QWidget {
    Q_OBJECT
    QPlainTextEdit* codeEditor;
public:
    ~LineNumberArea();
    LineNumberArea(QPlainTextEdit* parent):
        QWidget(parent), codeEditor(parent) { }
    virtual QSize sizeHint() const override;
    void paintEvent(QPaintEvent* event) override;
};

class DndEditor: public QPlainTextEdit
{
    Q_OBJECT
    LineNumberArea* lineNumberArea;
public:
    DndSyntaxHighlighter* highlight;
    int error_line = -1;
    DndEditor(QWidget* parent=nullptr);
    ~DndEditor();
    void update_syntax(void);
    int lineNumberAreaWidth(void);
    void updateLineNumberAreaWidth(int);
    void updateLineNumberArea(QRect rect, int dy);
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    void highlightCurrentLine(void);
    void lineNumberAreaPaintEvent(QPaintEvent* event);
    void insert_dnd_block(const QString&);
    void insert_image(const QString&);
    void insert_dnd(const QString&);
    void insert_css(const QString&);
    void insert_script(const QString&);
    void insert_image_links(const QString&, const QString&);
    void alter_indent(bool indent);
    virtual void contextMenuEvent(QContextMenuEvent*event) override;
};

class DndWebPage : public QWebEnginePage {
    Q_OBJECT
public:
    DndWebPage(QWidget* parent): QWebEnginePage(parent) {}
    ~DndWebPage();
    QString basedir = "";
    virtual bool acceptNavigationRequest(const QUrl& url, QWebEnginePage::NavigationType navtype, bool isMainFrame) override;
};

class SplitterHandler : public QObject {
    Q_OBJECT
    public:
        ~SplitterHandler();
        SplitterHandler(QWidget* parent): QObject(parent) {}
        virtual bool eventFilter(QObject* watched, QEvent* event) override;
};


class Page: public QSplitter
{
    Q_OBJECT
    QWebEngineView* web;
    QPlainTextEdit* error_display;
    QSplitter* editor_holder;
    bool inflight = false;
    bool auto_apply = true;
    QList<QCheckBox*> checks;
    QWidget* checkholder;
    QHBoxLayout* checkholder_layout;
    QSet<QString> dependencies;
    QString scroll_pos_string;
    bool show_errors = true;
    bool editor_is_on_left = true;
    bool coord_helper = false;

public:
    DndWebPage* webpage;
    DndEditor* textedit;
    QString filename;
    QString dirname;
    Page(QWidget*parent=nullptr);
    ~Page();
    void save(void);
    void put_editor_left(void);
    void put_editor_right(void);
    void contents_changed(void);
    void auto_apply_changed(int state);
    void read_only_changed(int state);
    void coord_helper_changed(int state);
    void file_changed(const QString& path);
    void clear_errors(void);
    // void display_dndc_error(int error_type, QString
    QString get_text_for_preview(void);
    void update_html(void);
    void set_scroll_pos(QString&& x);
    void format(void);
    void hide_editor(void);
    void show_editor(void);
    void show_error(void);
    void hide_error(void);
    bool get_fname(const QString& title, const QString& filter, QString* out);
    void insert_image(void);
    void insert_image_links(void);
    void insert_dnd(void);
    void insert_css(void);
    void insert_script(void);
    void export_as_html(void);
    void display_dndc_error(int error_type, const QString& filename, int row, int col, const QString& message);
    void add_dependencies(size_t count, DndcStringView* paths);

};

void add_tab(const QString&, bool focus=true, bool allow_fail=false);
