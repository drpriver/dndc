#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    this->watcher  = new QFileSystemWatcher(this);
    this->settings = new QSettings("DavidTechnology", "DndcEdit", this);
    connect(this->watcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::file_changed);
    this->b64cache = dndc_create_filecache();
    this->textcache = dndc_create_filecache();
}

MainWindow::~MainWindow()
{
    dndc_filecache_destroy(b64cache);
    dndc_filecache_destroy(textcache);
}

void
MainWindow::file_changed(const QString& path){
    if(path.endsWith("png")){
    }
    auto bytes = path.toUtf8();
    DndcStringView sv = {(size_t)bytes.length(), bytes.data()};
    dndc_filecache_remove(b64cache, sv);
    dndc_filecache_remove(textcache, sv);
}

