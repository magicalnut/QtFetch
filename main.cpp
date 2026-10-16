#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("QtFetch");
    MainWindow w;
    w.show();
    return QApplication::exec();
}
