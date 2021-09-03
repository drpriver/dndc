#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemWatcher>
#include <QSettings>
#include <Dndc/dndc.h>

class MainWindow : public QMainWindow
{
    Q_OBJECT
    QFileSystemWatcher* watcher;
    QSettings* settings;
    DndcFileCache* b64cache;
    DndcFileCache* textcache;
    void file_changed(const QString&);
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
};
#endif // MAINWINDOW_H
