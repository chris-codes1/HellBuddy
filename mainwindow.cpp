#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , stratagemPicker(nullptr)
    , listeningForInput(false)     // ← add
    , selectedKeybindNumber(-1)    // ← add
    , selectedStratagemNumber(0)   // ← add
    , macroDisabled(false)          // ← add
{
    ui->setupUi(this);
    setWindowTitle("HellBuddy");

    // Read version
    QFile versionFile(QCoreApplication::applicationDirPath() + "/version.txt");

    if (!versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open version.txt:" << versionFile.errorString();
    } else {
        QString version = QString::fromUtf8(versionFile.readAll()).trimmed();
        versionFile.close();

        ui->version->setText("v" + version);
    }

    // Setup Helldivers 2 keybinds
    QFile helldiversKeybindsFile(QCoreApplication::applicationDirPath() + "/helldivers_keybinds.json");
    if (!helldiversKeybindsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << helldiversKeybindsFile.errorString();
    }

    QByteArray helldiversKeybindsData = helldiversKeybindsFile.readAll();
    helldiversKeybindsFile.close();

    QJsonDocument helldiversKeybindsDoc = QJsonDocument::fromJson(helldiversKeybindsData);
    if (!helldiversKeybindsDoc.isObject()) {
        qWarning() << "Invalid JSON structure";
    }

    QJsonObject hdkbObj = helldiversKeybindsDoc.object();

    //Key map
    keyMap = {
        {"W", stringHexToInt(hdkbObj.value("up").toString())},
        {"A", stringHexToInt(hdkbObj.value("left").toString())},
        {"S", stringHexToInt(hdkbObj.value("down").toString())},
        {"D", stringHexToInt(hdkbObj.value("right").toString())},
        {"stratagem_menu", stringHexToInt(hdkbObj.value("stratagem_menu").toString())}
    };

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
    QJsonArray equippedStratagemsArray = userDataDoc.object().value("equipped_stratagems").toArray();
    QJsonArray keybinds = userDataDoc.object().value("keybinds").toArray();

    equippedStratagems.resize(10);

    // Connect stratagem buttons
    for (int i = 0; i <= 7; ++i) {
        // Build the object name (matches your .ui naming)
        QString stratagemBtnName = QString("stratagemBtn%1").arg(i);
        QString keybindBtnName = QString("keybindBtn%1").arg(i);

        // Find the button by name
        QPushButton *stratagemBtn = this->findChild<QPushButton*>(stratagemBtnName);
        QPushButton *keybindBtn = this->findChild<QPushButton*>(keybindBtnName);

        //Set stratagem icon
        QString stratName = equippedStratagemsArray[i].toString();
        QString iconPath = QString(":/thumbs/StratagemIcons/%1.svg").arg(stratName);
        stratagemBtn->setIcon(QIcon(iconPath));

        //Set into equipped stratagems array
        equippedStratagems[i] = stratName;

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
                keybindKeyCode)) {                              // key code
            qDebug() << "Failed to register hotkey!";
        }
        if (!RegisterHotKey( // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey
                reinterpret_cast<HWND>(this->winId()), // window handle
                i + 100,                                              // hotkey ID (must be unique)
                MOD_SHIFT,                                              // shift modifier
                keybindKeyCode)) {                              // key code
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

    //Build stratagems hash table
    QFile stratagemsFile(QCoreApplication::applicationDirPath() + "/stratagems.json");
    if (!stratagemsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << stratagemsFile.errorString();
    }
    QByteArray stratagemsData = stratagemsFile.readAll();
    stratagemsFile.close();
    QJsonDocument stratagemsDoc = QJsonDocument::fromJson(stratagemsData);
    QJsonArray array = stratagemsDoc.array();
    for (int i = 0; i < array.size(); ++i) {
        const QJsonValue &value = array.at(i);
        QJsonObject obj = value.toObject();

        QString name = obj["name"].toString();
        QJsonArray seqArray = obj["sequence"].toArray();

        QVector<QString> sequence;
        for (int i = 0; i < seqArray.size(); ++i) {
            const QJsonValue &seqVal = seqArray.at(i);
            sequence.append(seqVal.toString());
        }

        stratagems.insert(name, sequence);
    }

    //Register macro disabled key code
    if (!RegisterHotKey( // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey
            reinterpret_cast<HWND>(this->winId()), // window handle
            999999,                                              // hotkey ID (must be unique)
            0,                                              // modifiers (e.g. MOD_CONTROL | MOD_ALT)
            0xBE)) {                              // key code ('A' key)
        qDebug() << "Failed to register macro disabled hotkey!";
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
        UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), i); // Stratagem hotkeys
        UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), i + 100); // Stratagem hotkeys
    }
    UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), 999999); // Macro disabled hotkey
    delete ui;
    //delete stratagemPicker;
}

void MainWindow::toggleDisableMacro() {
    // macroDisabled = !macroDisabled;

    // if (macroDisabled == true) {
    //     ui->MacroDisabledBtn->setStyleSheet(
    //         "QPushButton { background-color: rgb(15, 15, 15); color: rgb(255,0,0); }"
    //         "QPushButton:hover { background-color: #202020; /* slightly lighter */ }"
    //         "QPushButton:pressed { background-color: #404040; /* lighter when pressed */ }"
    //     );
    //     ui->MacroDisabledBtn->setText("Disabled");
    // } else if (macroDisabled == false) {
    //     ui->MacroDisabledBtn->setStyleSheet(
    //         "QPushButton { background-color: rgb(15, 15, 15); color: rgb(0,255,0); }"
    //         "QPushButton:hover { background-color: #202020; /* slightly lighter */ }"
    //         "QPushButton:pressed { background-color: #404040; /* lighter when pressed */ }"
    //     );
    //     ui->MacroDisabledBtn->setText("Enabled");
    // }
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            int hotkeyId = msg->wParam;
            if (hotkeyId == 999999) { // Macro disabled
                toggleDisableMacro();
                return true;
            } else if (hotkeyId >= 0 && hotkeyId <= 107) { // Stratagem
                int keyCode = hotkeyId;
                onHotkeyPressed(hotkeyId, keyCode);
                return true;
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
        if (stratagemPicker) {
            stratagemPicker->close();
            break;
        }
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

    selectedStratagemNumber = number;
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

QString getActiveWindowTitle() {
    HWND hwnd = GetForegroundWindow(); // get handle to active window
    if (!hwnd)
        return "No active window";

    wchar_t title[256];
    GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t));

    return QString::fromWCharArray(title);
}

void pressKey(WORD key) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key; // virtual key code, e.g., VK_A
    input.ki.dwFlags = 0; // 0 = key press
    SendInput(1, &input, sizeof(INPUT));
}

void releaseKey(WORD key) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    input.ki.dwFlags = KEYEVENTF_KEYUP; // key release
    SendInput(1, &input, sizeof(INPUT));
}

void MainWindow::onHotkeyPressed(int hotkeyNumber, int keyCode)
{
    qDebug() << "Hotkey pressed: " << hotkeyNumber;

    //Sanity checks
    QString activeWindowTitle = getActiveWindowTitle();
    // if (macroDisabled == true) { // Check if macro is enabled
    //     return;
    // } else if (activeWindowTitle != "HELLDIVERS™ 2") { // Check if window selected is 'HELLDIVERS 2'
    //     return;
    // }

    qDebug() << "Sanity checks completed, macro activated";

    if (hotkeyNumber >= 100) { // If the hotkey has a modifier
        hotkeyNumber -= 100;
    }

    //Change button color to green
    // QString stratagemBtnName = QString("stratagemBtn%1").arg(hotkeyNumber);
    // QPushButton *stratagemBtn = this->findChild<QPushButton*>(stratagemBtnName);
    // stratagemBtn->setStyleSheet(
    //     "QPushButton:hover { background-color: rgb(32, 32, 32); }"
    //     "QPushButton:pressed { background-color: rgb(64, 64, 64); }"
    // );

    // Activate stratagem number 'hotkeyNumber'
    QString stratagemToActivate = equippedStratagems[hotkeyNumber];
    QVector<QString> sequence = stratagems[stratagemToActivate];

    pressKey(keyMap.value("stratagem_menu"));
    QThread::msleep(50);
    for (const QString &keyStr : sequence) {
        QThread::msleep(50);
        pressKey(keyMap.value(keyStr));
        QThread::msleep(50);
        releaseKey(keyMap.value(keyStr));
    }
    QThread::msleep(50);
    releaseKey(keyMap.value("stratagem_menu"));

    // Change color button back
    // stratagemBtn->setStyleSheet(
    //     "QPushButton { background-color: rgb(15, 15, 15); }"
    //     "QPushButton:hover { background-color: rgb(32, 32, 32); }"
    //     "QPushButton:pressed { background-color: rgb(64, 64, 64); }"
    // );
}

void MainWindow::setStratagem(const QString &stratagemName)
{
    //Set icon
    QString iconPath = QString(":/thumbs/StratagemIcons/%1.svg").arg(stratagemName);
    QString buttonName = QString("stratagemBtn%1").arg(selectedStratagemNumber);
    QPushButton *button = findChild<QPushButton *>(buttonName);
    button->setIcon(QIcon(iconPath));

    //Set stratagem name to 'keybinds' QVector - [i] = stratagemName
    equippedStratagems[selectedStratagemNumber] = stratagemName;

    // Save selected stratagem to user_data.json -> equipped_stratagems -  [i] = stratagemName
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

    // Get the "equipped_stratagems" array
    QJsonArray equippedStratagemsArray = root.value("equipped_stratagems").toArray();
    if (selectedStratagemNumber < 0 || selectedStratagemNumber >= equippedStratagemsArray.size()) {
        qDebug() << "Invalid stratagem index:" << selectedStratagemNumber;
        return;
    }

    // Replace element
    equippedStratagemsArray[selectedStratagemNumber] = stratagemName;

    // Update root object
    root["equipped_stratagems"] = equippedStratagemsArray;

    // // Write back to file
    file.setFileName("user_data.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "Failed to open file for writing.";
        return;
    }
    QJsonDocument saveDoc(root);
    file.write(saveDoc.toJson(QJsonDocument::Indented));
    file.close();
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
    UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), selectedKeybindNumber+100);

    //Bind new keybind
    if (!RegisterHotKey( // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey
            reinterpret_cast<HWND>(this->winId()), // window handle
            selectedKeybindNumber,                                              // hotkey ID (must be unique)
            0,                                              // modifiers (e.g. MOD_CONTROL | MOD_ALT)
            vkKeybindKeyCode)) {                              // key code
        qDebug() << "Failed to register hotkey!";
    }
    if (!RegisterHotKey( // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey
            reinterpret_cast<HWND>(this->winId()), // window handle
            selectedKeybindNumber+100,                                              // hotkey ID (must be unique)
            4,                                              // modifiers (e.g. MOD_CONTROL | MOD_ALT)
            vkKeybindKeyCode)) {                              // key code
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
}
