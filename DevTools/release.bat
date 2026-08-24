@echo off
set PROJECT_NAME=HellBuddy

REM Build the final .exe

REM Build and data directories (relative to project root)
set PROJECT_ROOT=%~dp0..
set BUILD_DIR=%PROJECT_ROOT%\build\Desktop_Qt_6_9_1_MinGW_64_bit-Release
set JSON_DATA_DIR=%PROJECT_ROOT%\JsonData

REM Get project info
set /p VERSION=<"%PROJECT_ROOT%\version.txt"
set RELEASE_NAME=%PROJECT_NAME%%VERSION%
set RELEASE_DIR=%PROJECT_ROOT%\build\%RELEASE_NAME%

echo Building project in release mode...

REM Ensure build directory exists
if not exist "%BUILD_DIR%" (
    echo Build directory not found. Build the project in Release first.
    pause
    exit /b 1
)

cd /d %BUILD_DIR%
cmake --build . --config Release
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

REM Stage a clean release folder holding only what HellBuddy needs at runtime.
REM Copying the whole build directory would drag in CMake/ninja intermediates.
echo Staging release folder...
if exist "%RELEASE_DIR%" rmdir /S /Q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

echo Copying executable...
copy /Y "%BUILD_DIR%\%PROJECT_NAME%.exe" "%RELEASE_DIR%"

echo Copying JsonData files...
copy /Y "%JSON_DATA_DIR%\*.json" "%RELEASE_DIR%"

echo Copying version file...
copy /Y "%PROJECT_ROOT%\version.txt" "%RELEASE_DIR%"

REM Deploy Qt into the staged folder. HellBuddy links Widgets only and paints through the
REM raster engine, so the software OpenGL fallback, the D3D shader compiler and the
REM translation catalogues are all dead weight.
echo Running windeployqt...
windeployqt --release --no-translations --no-opengl-sw --no-system-d3d-compiler "%RELEASE_DIR%\%PROJECT_NAME%.exe"
if errorlevel 1 (
    echo windeployqt failed.
    pause
    exit /b 1
)

REM Nothing in the source uses QNetwork; windeployqt pulls it in transitively.
if exist "%RELEASE_DIR%\Qt6Network.dll" del /Q "%RELEASE_DIR%\Qt6Network.dll"
if exist "%RELEASE_DIR%\networkinformation" rmdir /S /Q "%RELEASE_DIR%\networkinformation"
if exist "%RELEASE_DIR%\tls" rmdir /S /Q "%RELEASE_DIR%\tls"

REM Icons are SVG and the app icon is ICO, so the other image format plugins go unused.
if exist "%RELEASE_DIR%\imageformats\qjpeg.dll" del /Q "%RELEASE_DIR%\imageformats\qjpeg.dll"
if exist "%RELEASE_DIR%\imageformats\qgif.dll" del /Q "%RELEASE_DIR%\imageformats\qgif.dll"

echo Zipping release...
powershell Compress-Archive -Path "%RELEASE_DIR%" -DestinationPath "%PROJECT_ROOT%\build\%RELEASE_NAME%.zip" -Force

echo Done!
pause
