# HellBuddy

Qt6/C++ macro utility for HELLDIVERS 2. Windows-only. Injects keystrokes via WinAPI (`SendInput`, `RegisterHotKey`).

## Build

```bash
cmake --build build/ --parallel $(nproc)
```

From scratch:
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/usr/share/mingw/toolchain-mingw64.cmake
cmake --build build/ --parallel $(nproc)
cp /usr/x86_64-w64-mingw32/sys-root/mingw/bin/*.dll build/
cp /usr/x86_64-w64-mingw32/sys-root/mingw/lib/qt6/plugins/platforms/qwindows.dll build/platforms/
cp /usr/x86_64-w64-mingw32/sys-root/mingw/lib/qt6/plugins/iconengines/qsvgicon.dll build/iconengines/
cp /usr/x86_64-w64-mingw32/sys-root/mingw/lib/qt6/plugins/imageformats/qsvg.dll build/imageformats/
cp JsonData/*.json version.txt build/
```

## Run (Linux)

```bash
cd build && WINEDEBUG=-all wine HellBuddy.exe
```

## Key files

- `mainwindow.cpp/h` — main window, hotkey registration, macro input logic
- `stratagempicker.cpp/h` — stratagem selection dialog
- `JsonData/stratagems.json` — stratagem names and key sequences
- `JsonData/user_data.json` — user save (equipped stratagems + keybinds)
- `JsonData/helldivers_keybinds.json` — in-game key mapping (hex VK codes)
- `JsonData/qt_key_to_win_vk.json` — Qt keycode → Windows VK code map
- `resources.qrc` — embeds all SVG icons from `StratagemIcons/`

## Adding a new stratagem

1. Add SVG icon to `StratagemIcons/`
2. Run `python3 DevTools/BuildQrcResources.py` to update `resources.qrc`
3. Add entry to `JsonData/stratagems.json`: `{"name": "...", "sequence": ["W","A","S","D"]}`

## Runtime files (must be in build dir alongside .exe)

`stratagems.json`, `helldivers_keybinds.json`, `qt_key_to_win_vk.json`, `user_data.json`, `version.txt`

## DLL версии (важно)

Все `Qt6*.dll` должны быть одной версии. Источник правды — sysroot:
```bash
cp /usr/x86_64-w64-mingw32/sys-root/mingw/bin/Qt6*.dll build/
cp /usr/x86_64-w64-mingw32/sys-root/mingw/lib/qt6/plugins/iconengines/qsvgicon.dll build/iconengines/
```
Смешивание версий (например, DLL от старого релиза + новые) приводит к крашу с `unimplemented function Qt6Core.dll`.
