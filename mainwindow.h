#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMouseEvent>
#include <QApplication>
#include <QFile>
#include <QMainWindow>
#include "stratagempicker.h"
#include <QPoint>
#include <QDebug>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>
#include <QVector>
#include <QThread>
#include <windows.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setStratagem(const QString &stratagemName);

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    //bool eventFilter(QObject *obj, QEvent *event) override;

private:
    int stringHexToInt(const QString &hexStr);
    void onStratagemClicked(int);
    void onKeybindClicked(int);
    void onHotkeyPressed(int, int);
    void keyPressEvent(QKeyEvent *event) override;
    void minimizeWindow();
    void closeAllWindows();
    WORD getVkCode(const QString &keyStr);
    Ui::MainWindow *ui;

    //For window dragging
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QPoint dragPosition;
    bool dragging = false;

    //Stratagem setting
    int selectedStratagemNumber;

    //Stratagem and keybind arrays
    QJsonObject qtToWinVkKeyMap;
    QVector<QString> equippedStratagems;
    QHash<QString, QVector<QString>> stratagems;

    //For keybind setting input listening
    bool listeningForInput;
    int selectedKeybindNumber;
    QString oldKeybindBtnText;
    QPushButton *selectedKeybindBtn = nullptr;

    //Stratagem picker window
    StratagemPicker *stratagemPicker;

    //Key code hash map
    QHash<QString, WORD> keyMap;
};

#endif // MAINWINDOW_H
