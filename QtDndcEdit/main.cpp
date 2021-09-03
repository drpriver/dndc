#include "mainwindow.h"
#include "DndcEdit.h"
#include <Dndc/dndc.h>

#include <QApplication>

int main(int argc, char *argv[])
{
    dndc_init_python();

    QApplication a(argc, argv);
    a.setApplicationDisplayName("DndcEdit");
    a.setApplicationName("DndcEdit");
    DndcEdit::create_scheme();
    MainWindow w;
    w.show();
    return a.exec();
}
