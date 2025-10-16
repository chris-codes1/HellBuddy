#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "stratagempicker.h"
#include <QPoint>
#include <QDebug>
#include <QPushButton>
//#include <QJsonArray>
#include <QJsonObject>
#include <windows.h>  // WinAPI

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    //bool eventFilter(QObject *obj, QEvent *event) override;

private:
    int stringHexToInt(const QString &hexStr);
    void onStratagemClicked(int);
    void onKeybindClicked(int);
    void onHotkeyPressed(int);
    void keyPressEvent(QKeyEvent *event) override;
    void minimizeWindow();
    void closeAllWindows();
    Ui::MainWindow *ui;

    //For window dragging
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QPoint dragPosition;
    bool dragging = false;

    //Stratagem and keybind arrays
    QJsonObject qtToWinVkKeyMap;
    //QJsonArray stratagems;
    //QJsonArray keybinds;

    //For keybind setting input listening
    bool listeningForInput;
    int selectedKeybindNumber;
    QString oldKeybindBtnText;
    QPushButton *selectedKeybindBtn = nullptr;

    //Stratagem picker window
    StratagemPicker *stratagemPicker;
};

#endif // MAINWINDOW_H
