#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>

namespace {
constexpr int kSlotCount         = 8;      // macro slots shown in the UI
constexpr int kStratagemSlots    = 10;     // stratagem entries stored per preset
constexpr int kHotkeyMacroToggle = 999999;
constexpr int kHotkeyPrevPreset  = 900;
constexpr int kHotkeyNextPreset  = 901;

const char *kSmallBtnStyle = R"(
QPushButton {
    background-color: rgb(15, 15, 15);
    color: rgb(255, 255, 255);
    border: none;
}
QPushButton:hover  { background-color: #202020; }
QPushButton:pressed { background-color: #404040; }
)";

const char *kComboStyle = R"(
QComboBox {
    background-color: rgb(15, 15, 15);
    color: rgb(255, 255, 255);
    border: none;
    padding: 2px 4px;
}
QComboBox:hover { background-color: #202020; }
QComboBox::drop-down { border: none; width: 16px; }
QComboBox QAbstractItemView {
    background-color: rgb(15, 15, 15);
    color: rgb(255, 255, 255);
    selection-background-color: #404040;
    border: 1px solid #404040;
}
)";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , stratagemPicker(nullptr)
    , listeningForInput(false)
    , selectedKeybindNumber(-1)
    , selectedStratagemNumber(0)
    , macroDisabled(false)
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

    // Read user_data.json (migrates pre-preset save files automatically)
    loadUserData();

    equippedStratagems.resize(kStratagemSlots);

    // Connect stratagem / keybind buttons. Icons, labels and hotkeys are set
    // by applyPreset() below so that switching presets reuses the same path.
    for (int i = 0; i < kSlotCount; ++i) {
        QString stratagemBtnName = QString("stratagemBtn%1").arg(i);
        QString keybindBtnName = QString("keybindBtn%1").arg(i);

        QPushButton *stratagemBtn = this->findChild<QPushButton*>(stratagemBtnName);
        QPushButton *keybindBtn = this->findChild<QPushButton*>(keybindBtnName);

        if (stratagemBtn) {
            connect(stratagemBtn, &QPushButton::clicked, this, [=]() {
                onStratagemClicked(i);
            });
        }
        if (keybindBtn) {
            connect(keybindBtn, &QPushButton::clicked, this, [=]() {
                onKeybindClicked(i);
            });
        }
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
        for (int j = 0; j < seqArray.size(); ++j) {
            const QJsonValue &seqVal = seqArray.at(j);
            sequence.append(seqVal.toString());
        }

        stratagems.insert(name, sequence);
    }

    // Build the preset bar and load the last used preset
    setupPresetBar();
    applyPreset(activePresetIndex);

    //Register macro disabled key code
    if (!RegisterHotKey( // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey
            reinterpret_cast<HWND>(this->winId()), // window handle
            kHotkeyMacroToggle,                    // hotkey ID (must be unique)
            0,                                     // modifiers (e.g. MOD_CONTROL | MOD_ALT)
            0xBE)) {                               // key code ('.' key)
        qDebug() << "Failed to register macro disabled hotkey!";
    }

    //Register preset cycling hotkeys (defaults: Alt + [ and Alt + ])
    QJsonObject presetHotkeys = userData.value("preset_hotkeys").toObject();
    int prevVk = stringHexToInt(presetHotkeys.value("prev").toString("0xDB"));
    int nextVk = stringHexToInt(presetHotkeys.value("next").toString("0xDD"));
    UINT presetMod = MOD_ALT;
    QString modStr = presetHotkeys.value("modifiers").toString("alt").toLower();
    if (modStr == "ctrl" || modStr == "control")      presetMod = MOD_CONTROL;
    else if (modStr == "shift")                       presetMod = MOD_SHIFT;
    else if (modStr == "none")                        presetMod = 0;

    if (prevVk > 0 && !RegisterHotKey(reinterpret_cast<HWND>(this->winId()),
                                      kHotkeyPrevPreset, presetMod, prevVk)) {
        qDebug() << "Failed to register previous preset hotkey!";
    }
    if (nextVk > 0 && !RegisterHotKey(reinterpret_cast<HWND>(this->winId()),
                                      kHotkeyNextPreset, presetMod, nextVk)) {
        qDebug() << "Failed to register next preset hotkey!";
    }

    //this->adjustSize();

    // Make window 90% opaque
    setWindowOpacity(0.9);

    // Remove the default OS title bar and set fixed size
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
}

MainWindow::~MainWindow()
{
    for (int i = 0; i < kSlotCount; ++i) {
        unregisterSlotHotkeys(i);
    }
    UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), kHotkeyMacroToggle);
    UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), kHotkeyPrevPreset);
    UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), kHotkeyNextPreset);
    delete ui;
    //delete stratagemPicker;
}

// =====================================================================
// Presets
// =====================================================================

QString MainWindow::userDataPath() const
{
    return QCoreApplication::applicationDirPath() + "/user_data.json";
}

QJsonObject MainWindow::makeDefaultPreset(const QString &name) const
{
    static const char *defaultLetters[kSlotCount] = {"T","Y","H","N","U","J","M","K"};
    static const char *defaultCodes[kSlotCount]   = {"0x54","0x59","0x48","0x4E","0x55","0x4A","0x4D","0x4B"};

    QJsonArray strat;
    for (int i = 0; i < kStratagemSlots; ++i) {
        strat.append("Resupply");
    }

    QJsonArray binds;
    for (int i = 0; i < kSlotCount; ++i) {
        QJsonObject kb;
        kb["letter"]   = defaultLetters[i];
        kb["key_code"] = defaultCodes[i];
        binds.append(kb);
    }

    QJsonObject preset;
    preset["name"]                = name;
    preset["equipped_stratagems"] = strat;
    preset["keybinds"]            = binds;
    return preset;
}

void MainWindow::loadUserData()
{
    QJsonObject root;

    QFile file(userDataPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open user_data.json:" << file.errorString();
    } else {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    // Migrate the old single-loadout format into a one-entry preset list.
    if (!root.contains("presets")) {
        QJsonObject preset = makeDefaultPreset("Default");
        if (root.contains("equipped_stratagems")) {
            preset["equipped_stratagems"] = root.value("equipped_stratagems").toArray();
        }
        if (root.contains("keybinds")) {
            preset["keybinds"] = root.value("keybinds").toArray();
        }

        QJsonArray migrated;
        migrated.append(preset);

        QJsonObject newRoot;
        newRoot["presets"]       = migrated;
        newRoot["active_preset"] = 0;
        if (root.contains("preset_hotkeys")) {
            newRoot["preset_hotkeys"] = root.value("preset_hotkeys");
        }
        root = newRoot;
    }

    userData = root;
    presets  = userData.value("presets").toArray();

    if (presets.isEmpty()) {
        presets.append(makeDefaultPreset("Default"));
    }

    activePresetIndex = userData.value("active_preset").toInt(0);
    if (activePresetIndex < 0 || activePresetIndex >= presets.size()) {
        activePresetIndex = 0;
    }
}

void MainWindow::saveUserData()
{
    userData["presets"]       = presets;
    userData["active_preset"] = activePresetIndex;

    QFile file(userDataPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "Failed to open user_data.json for writing:" << file.errorString();
        return;
    }
    file.write(QJsonDocument(userData).toJson(QJsonDocument::Indented));
    file.close();
}

void MainWindow::syncActivePresetFromState()
{
    if (activePresetIndex < 0 || activePresetIndex >= presets.size()) {
        return;
    }

    QJsonArray strat;
    for (int i = 0; i < equippedStratagems.size(); ++i) {
        strat.append(equippedStratagems[i]);
    }

    QJsonObject preset = presets.at(activePresetIndex).toObject();
    preset["equipped_stratagems"] = strat;
    preset["keybinds"]            = currentKeybinds;
    presets[activePresetIndex]    = preset;
}

void MainWindow::setupPresetBar()
{
    QHBoxLayout *presetLayout = new QHBoxLayout();
    presetLayout->setSpacing(1);
    presetLayout->setContentsMargins(0, 0, 0, 0);

    presetBox = new QComboBox(this);
    presetBox->setStyleSheet(kComboStyle);
    presetBox->setToolTip("Active preset (Alt + [ / Alt + ] to cycle)");
    presetBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    presetBox->setMinimumHeight(24);
    refreshPresetBox();

    QPushButton *addBtn = new QPushButton("+", this);
    addBtn->setToolTip("New preset");
    QPushButton *renameBtn = new QPushButton("R", this);
    renameBtn->setToolTip("Rename preset");
    QPushButton *deleteBtn = new QPushButton("-", this);
    deleteBtn->setToolTip("Delete preset");

    const QList<QPushButton*> smallButtons = {addBtn, renameBtn, deleteBtn};
    for (QPushButton *btn : smallButtons) {
        btn->setStyleSheet(kSmallBtnStyle);
        btn->setFixedSize(24, 24);
        btn->setFocusPolicy(Qt::NoFocus);
    }

    presetLayout->addWidget(presetBox);
    presetLayout->addWidget(addBtn);
    presetLayout->addWidget(renameBtn);
    presetLayout->addWidget(deleteBtn);

    // Insert between the title bar (index 0) and the stratagem grid (index 1)
    ui->verticalLayout->insertLayout(1, presetLayout);

    // The .ui file sets stretch factors per index; re-apply them after the insert
    ui->verticalLayout->setStretch(0, 0);           // title bar
    ui->verticalLayout->setStretch(1, 0);           // preset bar
    ui->verticalLayout->setStretch(2, 999999999);   // stratagem grid
    ui->verticalLayout->setStretch(3, 0);           // macro disabled button

    // activated() only fires on user interaction, so programmatic index
    // changes (e.g. hotkey cycling) do not re-trigger it.
    connect(presetBox, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::onPresetSelected);
    connect(addBtn,    &QPushButton::clicked, this, &MainWindow::onAddPreset);
    connect(renameBtn, &QPushButton::clicked, this, &MainWindow::onRenamePreset);
    connect(deleteBtn, &QPushButton::clicked, this, &MainWindow::onDeletePreset);
}

void MainWindow::refreshPresetBox()
{
    if (!presetBox) {
        return;
    }

    QSignalBlocker blocker(presetBox);
    presetBox->clear();
    for (int i = 0; i < presets.size(); ++i) {
        QString name = presets.at(i).toObject().value("name").toString();
        if (name.isEmpty()) {
            name = QString("Preset %1").arg(i + 1);
        }
        presetBox->addItem(name);
    }
    presetBox->setCurrentIndex(activePresetIndex);
}

void MainWindow::registerSlotHotkeys(int slot, int vkCode)
{
    if (vkCode <= 0) {
        return;
    }

    if (!RegisterHotKey(reinterpret_cast<HWND>(this->winId()), slot, 0, vkCode)) {
        qDebug() << "Failed to register hotkey for slot" << slot;
    }
    if (!RegisterHotKey(reinterpret_cast<HWND>(this->winId()), slot + 100, MOD_SHIFT, vkCode)) {
        qDebug() << "Failed to register shift hotkey for slot" << slot;
    }
}

void MainWindow::unregisterSlotHotkeys(int slot)
{
    UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), slot);
    UnregisterHotKey(reinterpret_cast<HWND>(this->winId()), slot + 100);
}

void MainWindow::applyPreset(int index)
{
    if (index < 0 || index >= presets.size()) {
        return;
    }

    // Abort a pending "press a key" rebind so it can't land in the new preset
    if (listeningForInput && selectedKeybindBtn && !oldKeybindBtnText.isEmpty()) {
        selectedKeybindBtn->setText(oldKeybindBtnText);
    }
    listeningForInput = false;

    activePresetIndex = index;

    QJsonObject preset  = presets.at(index).toObject();
    QJsonArray stratArr = preset.value("equipped_stratagems").toArray();
    currentKeybinds     = preset.value("keybinds").toArray();

    equippedStratagems.resize(kStratagemSlots);
    for (int i = 0; i < kStratagemSlots; ++i) {
        equippedStratagems[i] = stratArr.at(i).toString();
    }

    for (int i = 0; i < kSlotCount; ++i) {
        QPushButton *stratagemBtn =
            this->findChild<QPushButton*>(QString("stratagemBtn%1").arg(i));
        if (stratagemBtn) {
            QString iconPath = QString(":/thumbs/StratagemIcons/%1.svg").arg(equippedStratagems[i]);
            stratagemBtn->setIcon(QIcon(iconPath));
            stratagemBtn->setToolTip(equippedStratagems[i]);
        }

        QJsonObject keybindObject = currentKeybinds.at(i).toObject();

        QPushButton *keybindBtn =
            this->findChild<QPushButton*>(QString("keybindBtn%1").arg(i));
        if (keybindBtn) {
            keybindBtn->setText(keybindObject.value("letter").toString());
        }

        // Rebind the Windows hotkeys to this preset's keys
        unregisterSlotHotkeys(i);
        registerSlotHotkeys(i, stringHexToInt(keybindObject.value("key_code").toString()));
    }

    if (presetBox && presetBox->currentIndex() != index) {
        QSignalBlocker blocker(presetBox);
        presetBox->setCurrentIndex(index);
    }
}

void MainWindow::cyclePreset(int delta)
{
    if (presets.size() < 2) {
        return;
    }

    int next = (activePresetIndex + delta) % presets.size();
    if (next < 0) {
        next += presets.size();
    }

    applyPreset(next);
    saveUserData();
}

void MainWindow::onPresetSelected(int index)
{
    if (index == activePresetIndex) {
        return;
    }
    applyPreset(index);
    saveUserData();
}

void MainWindow::onAddPreset()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, "New preset", "Preset name:",
                                         QLineEdit::Normal,
                                         QString("Preset %1").arg(presets.size() + 1), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    // Start the new preset as a copy of the current loadout
    syncActivePresetFromState();
    QJsonObject preset = presets.at(activePresetIndex).toObject();
    preset["name"] = name.trimmed();
    presets.append(preset);

    activePresetIndex = presets.size() - 1;
    refreshPresetBox();
    applyPreset(activePresetIndex);
    saveUserData();
}

void MainWindow::onRenamePreset()
{
    if (activePresetIndex < 0 || activePresetIndex >= presets.size()) {
        return;
    }

    QJsonObject preset = presets.at(activePresetIndex).toObject();

    bool ok = false;
    QString name = QInputDialog::getText(this, "Rename preset", "Preset name:",
                                         QLineEdit::Normal,
                                         preset.value("name").toString(), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    preset["name"] = name.trimmed();
    presets[activePresetIndex] = preset;

    refreshPresetBox();
    saveUserData();
}

void MainWindow::onDeletePreset()
{
    if (presets.size() <= 1) {
        QMessageBox::information(this, "Delete preset",
                                 "You need at least one preset.");
        return;
    }

    QString name = presets.at(activePresetIndex).toObject().value("name").toString();
    if (QMessageBox::question(this, "Delete preset",
                              QString("Delete preset \"%1\"?").arg(name))
        != QMessageBox::Yes) {
        return;
    }

    presets.removeAt(activePresetIndex);
    if (activePresetIndex >= presets.size()) {
        activePresetIndex = presets.size() - 1;
    }

    refreshPresetBox();
    applyPreset(activePresetIndex);
    saveUserData();
}

// =====================================================================

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
            if (hotkeyId == kHotkeyMacroToggle) { // Macro disabled
                toggleDisableMacro();
                return true;
            } else if (hotkeyId == kHotkeyPrevPreset) { // Previous preset
                cyclePreset(-1);
                return true;
            } else if (hotkeyId == kHotkeyNextPreset) { // Next preset
                cyclePreset(1);
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
    // } else if (activeWindowTitle != "HELLDIVERS 2") { // Check if window selected is 'HELLDIVERS 2'
    //     return;
    // }

    qDebug() << "Sanity checks completed, macro activated";

    if (hotkeyNumber >= 100) { // If the hotkey has a modifier
        hotkeyNumber -= 100;
    }

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
}

void MainWindow::setStratagem(const QString &stratagemName)
{
    //Set icon
    QString iconPath = QString(":/thumbs/StratagemIcons/%1.svg").arg(stratagemName);
    QString buttonName = QString("stratagemBtn%1").arg(selectedStratagemNumber);
    QPushButton *button = findChild<QPushButton *>(buttonName);
    if (button) {
        button->setIcon(QIcon(iconPath));
        button->setToolTip(stratagemName);
    }

    if (selectedStratagemNumber < 0 || selectedStratagemNumber >= equippedStratagems.size()) {
        qDebug() << "Invalid stratagem index:" << selectedStratagemNumber;
        return;
    }

    //Set stratagem name into the live loadout
    equippedStratagems[selectedStratagemNumber] = stratagemName;

    //Store it in the active preset and save
    syncActivePresetFromState();
    saveUserData();
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
        if (!oldKeybindBtnText.isEmpty() && selectedKeybindBtn) {
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

    //Rebind the hotkey
    unregisterSlotHotkeys(selectedKeybindNumber);
    registerSlotHotkeys(selectedKeybindNumber, vkKeybindKeyCode);

    if (selectedKeybindNumber < 0 || selectedKeybindNumber >= currentKeybinds.size()) {
        qDebug() << "Invalid keybind index:" << selectedKeybindNumber;
        return;
    }

    // Create updated object
    QJsonObject newKeybind;
    newKeybind["letter"] = keyText;
    newKeybind["key_code"] = intToHexString(vkKeybindKeyCode);

    // Replace element in the live loadout, store it in the active preset and save
    currentKeybinds[selectedKeybindNumber] = newKeybind;
    syncActivePresetFromState();
    saveUserData();
}
