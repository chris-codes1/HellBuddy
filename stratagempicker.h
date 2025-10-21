#ifndef STRATAGEMPICKER_H
#define STRATAGEMPICKER_H

#include <QWidget>
#include <QFile>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QToolButton>

class MainWindow; // forward declaration (avoids circular include)

namespace Ui {
class StratagemPicker;
}

class StratagemPicker : public QWidget
{
    Q_OBJECT

public:
    explicit StratagemPicker(MainWindow *mainWindow = nullptr, QWidget *parent = nullptr);
    ~StratagemPicker();

private:
    Ui::StratagemPicker *ui;
    MainWindow *m_mainWindow; // store reference to your main window

    void minimizeWindow();

    //For window dragging
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QPoint dragPosition;
    bool dragging = false;
};

#endif // STRATAGEMPICKER_H
