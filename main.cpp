#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icons/HellBuddyIcon.ico"));
    MainWindow w;
    w.show();
    return a.exec();
}
