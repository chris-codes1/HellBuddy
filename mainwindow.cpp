#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMouseEvent>
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , stratagemPicker(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("HellBuddy");

    // Connect minimize and close buttons
    connect(ui->minimizeBtn, &QPushButton::clicked, this, &MainWindow::minimizeWindow);
    connect(ui->closeBtn, &QPushButton::clicked, this, &MainWindow::closeAllWindows);

    // Read qt_key_to_win_vk.json and set to array
    QFile qtToWinVkFile(QCoreApplication::applicationDirPath() + "/qt_key_to_win_vk.json");
    if (!qtToWinVkFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << qtToWinVkFile.errorString();
    }
    QByteArray qtToWinVkData = qtToWinVkFile.readAll();
    qtToWinVkFile.close();
    QJsonDocument qtToWinVkDoc = QJsonDocument::fromJson(qtToWinVkData);
    qtToWinVkKeyMap = qtToWinVkDoc.object();

    // Read user_data.json save file and set values
    QFile userDataFile(QCoreApplication::applicationDirPath() + "/user_data.json");
    if (!userDataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << userDataFile.errorString();
    }
    QByteArray userDataData = userDataFile.readAll();
    userDataFile.close();
    QJsonDocument userDataDoc = QJsonDocument::fromJson(userDataData);
    QJsonArray stratagems = userDataDoc.object().value("equipped_stratagems").toArray();
    QJsonArray keybinds = userDataDoc.object().value("keybinds").toArray();

    // Connect stratagem buttons
    for (int i = 0; i <= 7; ++i) {
        // Build the object name (matches your .ui naming)
        QString stratagemBtnName = QString("stratagemBtn%1").arg(i);
        QString keybindBtnName = QString("keybindBtn%1").arg(i);

        // Find the button by name
        QPushButton *stratagemBtn = this->findChild<QPushButton*>(stratagemBtnName);
        QPushButton *keybindBtn = this->findChild<QPushButton*>(keybindBtnName);

        //Set stratagem icon
        QString stratName = stratagems[i].toString();
        QString iconPath = QString(":/thumbs/StratagemIcons/%1.png").arg(stratName);
        stratagemBtn->setIcon(QIcon(iconPath));

        //Set keybind text
        QJsonObject keybindObject = keybinds[i].toObject();
        QString keybindLetter = keybindObject["letter"].toString();
        keybindBtn->setText(keybindLetter);

        //Setup keybind
        int keybindKeyCode = stringHexToInt(keybindObject["key_code"].toString());
        if (!RegisterHotKey( // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey
                reinterpret_cast<HWND>(this->winId()), // window handle
                i,                                              // hotkey ID (must be unique)
                0,                                              // modifiers (e.g. MOD_CONTROL | MOD_ALT)
                keybindKeyCode)) {                              // key code ('H' key)
            qDebug() << "Failed to register hotkey!";
        }

        //Connect clicked for stratagem and keybind buttons
        connect(stratagemBtn, &QPushButton::clicked, this, [=]() {
            onStratagemClicked(i);
        });
        connect(keybindBtn, &QPushButton::clicked, this, [=]() {
            onKeybindClicked(i);
        });
    }

    //this->adjustSize();

    // Make window 90% opaque
    setWindowOpacity(0.9);

    // Remove the default OS title bar and set fixed size
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
}

MainWindow::~MainWindow()
{
    for (int i = 0; i <= 7; ++i) {
        UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), i);
    }
    delete ui;
    delete stratagemPicker;
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            if (msg->wParam >= 0 && msg->wParam <= 7) {
                onHotkeyPressed(msg->wParam);
                return true; // handled
            }
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

int MainWindow::stringHexToInt(const QString& hexStr) {
    bool ok = false;
    int value = hexStr.toInt(&ok, 16);

    if (!ok) {
        qWarning() << "Invalid hex string:" << hexStr;
        return -1;
    }

    return value;
}

QString intToHexString(int value)
{
    return QString("0x%1").arg(value, 0, 16);
}

int getWinVKFromQtKey(int qtKey, const QJsonObject &keyMap)
{
    QString keyStr = QString::number(qtKey); // convert int to string for lookup
    if (!keyMap.contains(keyStr)) {
        qDebug() << "Qt key not found in mapping:" << qtKey;
        return 0; // return 0 if not found
    }

    QJsonObject keyInfo = keyMap.value(keyStr).toObject();
    QString vkCodeStr = keyInfo.value("key_code").toString(); // e.g., "0x41"
    bool ok;
    int vkCode = vkCodeStr.toInt(&ok, 16); // convert hex string to int
    if (!ok) {
        qDebug() << "Failed to convert key_code to int:" << vkCodeStr;
        return 0;
    }

    return vkCode;
}

void MainWindow::minimizeWindow() {
    this->showMinimized();
}

void MainWindow::closeAllWindows() {
    //Find and close select stratagem window if it's exists
    const auto topWidgets = QApplication::topLevelWidgets();
    for (QWidget *widget : topWidgets) {
        stratagemPicker = qobject_cast<StratagemPicker*>(widget);
        if (stratagemPicker)
            break;
    }

    if (stratagemPicker) {
        qDebug() << "window found";
        stratagemPicker->close();
    }
    else if (!stratagemPicker) {
        qDebug() << "window not found";
    }

    //Close main window
    this->close();
}

void MainWindow::onStratagemClicked(int number) {
    //Open window displaying stratagems
    if (!stratagemPicker) {
        stratagemPicker = new StratagemPicker(this); // create it once
    }
    stratagemPicker->show();   // show window
    stratagemPicker->raise();  // bring to front
    stratagemPicker->activateWindow();

    qDebug() << "Stratagem button pressed:" << number;
}

void MainWindow::onKeybindClicked(int number) {
    if (listeningForInput == true) { //If another keybind button is already waiting for input
        return;
    }

    QString btnName = QString("keybindBtn%1").arg(number);
    QPushButton *keybindBtn = this->findChild<QPushButton*>(btnName);
    oldKeybindBtnText = keybindBtn->text();
    keybindBtn->setText("<press key>");

    //Listen for input
    listeningForInput = true;
    selectedKeybindBtn = keybindBtn;
    selectedKeybindNumber = number;
}

void MainWindow::onHotkeyPressed(int hotkeyNumber)
{
    qDebug() << "Hotkey pressed!" << hotkeyNumber;
    // Activate stratagem number 'hotkeyNumber'

}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    // Only allow left button to drag
    if (event->button() == Qt::LeftButton) {
        // Record the position where the user clicked
        dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        dragging = true;
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - dragPosition);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        event->accept();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        listeningForInput = false;
        if (!oldKeybindBtnText.isEmpty()) {
            selectedKeybindBtn->setText(oldKeybindBtnText);
        }
        return;
    } else if (listeningForInput == false) {
        return;
    }

    //Convert Qt key value to Windows VK code
    int qtKeycode;
    QString keyText = event->text().toUpper();  // Typed character
    bool ok;
    if ((event->modifiers() & Qt::KeypadModifier) && keyText.toInt(&ok) && ok) { // Numpad number
        qtKeycode = keyText.toInt() + 10000 ;
        keyText = "NumPad" + keyText;
    } else { // Regular keypress
        qtKeycode = event->key();
    }

    int vkKeybindKeyCode = getWinVKFromQtKey(qtKeycode, qtToWinVkKeyMap);

    //If key isn't mapped
    if (vkKeybindKeyCode == 0) {
        return;
    }

    listeningForInput = false;
    selectedKeybindBtn->setText(keyText);
    oldKeybindBtnText = selectedKeybindBtn->text();

    //Unbind last keybind
    UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), selectedKeybindNumber);

    //Bind new keybind
    if (!RegisterHotKey( // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey
            reinterpret_cast<HWND>(this->winId()), // window handle
            selectedKeybindNumber,                                              // hotkey ID (must be unique)
            0,                                              // modifiers (e.g. MOD_CONTROL | MOD_ALT)
            vkKeybindKeyCode)) {                              // key code ('H' key)
        qDebug() << "Failed to register hotkey!";
    }

    //Update user_data.json with new keybind
    QFile file("user_data.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file for reading.";
        return;
    }

    // Parse existing JSON
    QByteArray data = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qDebug() << "JSON is not an object.";
        return;
    }

    QJsonObject root = doc.object();

    // Get the "keybinds" array
    QJsonArray keybinds = root.value("keybinds").toArray();
    if (selectedKeybindNumber < 0 || selectedKeybindNumber >= keybinds.size()) {
        qDebug() << "Invalid keybind index:" << selectedKeybindNumber;
        return;
    }

    // Create updated object
    QJsonObject newKeybind;
    newKeybind["letter"] = keyText;
    newKeybind["key_code"] = intToHexString(vkKeybindKeyCode);

    // Replace element
    keybinds[selectedKeybindNumber] = newKeybind;

    // Update root object
    root["keybinds"] = keybinds;

    // Write back to file
    file.setFileName("user_data.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "Failed to open file for writing.";
        return;
    }
    QJsonDocument saveDoc(root);
    file.write(saveDoc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Keybind updated successfully.";
}
