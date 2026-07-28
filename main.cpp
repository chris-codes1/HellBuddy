#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icons/HellBuddyIcon.ico"));

    a.setStyleSheet(
        "QToolTip {"                    // Mouse hover tool tip
        "   background-color: #1e1f22;" // Background color
        "   color: #ffffff;"            // Text color
        "   border: 1px solid #ffaa00;" // Accent lines
        "   padding: 5px;"
        "   font-family: 'Segoe UI', Arial;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "}"
        );

    MainWindow w;
    w.show();
    return a.exec();
}
