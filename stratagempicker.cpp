#include "stratagempicker.h"
#include "ui_stratagempicker.h"
#include <QFile>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QToolButton>

#include "mainwindow.h"

StratagemPicker::StratagemPicker(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent),
    ui(new Ui::StratagemPicker),
    m_mainWindow(mainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Select a stratagem");

    // Connect minimize and close buttons
    connect(ui->minimizeBtn, &QPushButton::clicked, this, &StratagemPicker::minimizeWindow);
    connect(ui->closeBtn, &QPushButton::clicked, this, &StratagemPicker::close);

    // Remove the default OS title bar and set fixed size
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // Make window 90% opaque
    setWindowOpacity(0.9);

    // Read stratagems.json
    QFile stratagemsFile(QCoreApplication::applicationDirPath() + "/stratagems.json");
    if (!stratagemsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << stratagemsFile.errorString();
    }
    QByteArray stratagemsData = stratagemsFile.readAll();
    stratagemsFile.close();
    QJsonDocument userDataDoc = QJsonDocument::fromJson(stratagemsData);
    QJsonArray stratagems = userDataDoc.array();

    int row = 0;
    int column = 0;
    const QJsonArray &array = stratagems;
    for (const QJsonValue &val : array) {
        QJsonObject obj = val.toObject();

        QString stratagemName = obj["name"].toString();
        //QJsonArray sequenceArray = obj["sequence"].toArray();

        QPushButton *stratagemBtn = new QPushButton(this);
        QString iconPath = QString(":/thumbs/StratagemIcons/%1.png").arg(stratagemName);
        stratagemBtn->setIcon(QIcon(iconPath));
        stratagemBtn->setIconSize(QSize(50, 50));
        stratagemBtn->setToolTip(stratagemName);

        QGridLayout *layout = qobject_cast<QGridLayout*>(ui->stratagems->layout());
        layout->addWidget(stratagemBtn, row, column);

        column += 1;
        if (column > 4) {
            row += 1;
            column = 0;
        }

        connect(stratagemBtn, &QPushButton::clicked, this, [=]() {
            m_mainWindow->setStratagem(stratagemName);
            this->close();
        });
    }
}

StratagemPicker::~StratagemPicker()
{
    delete ui;
}

void StratagemPicker::minimizeWindow() {
    this->showMinimized();
}

void StratagemPicker::mousePressEvent(QMouseEvent *event)
{
    // Only allow left button to drag
    if (event->button() == Qt::LeftButton) {
        // Record the position where the user clicked
        dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        dragging = true;
        event->accept();
    }
}

void StratagemPicker::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - dragPosition);
        event->accept();
    }
}

void StratagemPicker::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        event->accept();
    }
}
