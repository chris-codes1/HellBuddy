#ifndef STRATAGEMPICKER_H
#define STRATAGEMPICKER_H

#include <QWidget>

namespace Ui {
class StratagemPicker;
}

class StratagemPicker : public QWidget
{
    Q_OBJECT

public:
    explicit StratagemPicker(QWidget *parent = nullptr);
    ~StratagemPicker();

private:
    Ui::StratagemPicker *ui;
    void minimizeWindow();

    void onStratagemClicked(QString);

    //For window dragging
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QPoint dragPosition;
    bool dragging = false;
};

#endif // STRATAGEMPICKER_H
