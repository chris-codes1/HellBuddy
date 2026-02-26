@echo off
set PROJECT_NAME=HellBuddy

REM Build and data directories (relative to project root)
set PROJECT_ROOT=%~dp0..
set BUILD_DIR=%PROJECT_ROOT%\build\Desktop_Qt_6_9_1_MinGW_64_bit-Release
set JSON_DATA_DIR=%PROJECT_ROOT%\JsonData

REM Get project info
set /p VERSION=<"%PROJECT_ROOT%\version.txt"
set RELEASE_NAME=%PROJECT_NAME%%VERSION%

echo Building project in release mode...

REM Ensure build directory exists
if not exist "%BUILD_DIR%" (
    echo Build directory not found. Build the project in Release first.
    pause
    exit /b 1
)

cd /d %BUILD_DIR%
cmake --build . --config Release

echo Copying JsonData files...
xcopy /E /I /Y "%JSON_DATA_DIR%" "%BUILD_DIR%"

echo Copying version file...
copy "%PROJECT_ROOT%\version.txt" "%BUILD_DIR%"

cd /d %BUILD_DIR%

echo Running windeployqt...
windeployqt %PROJECT_NAME%.exe

cd ..

echo Duplicating project...
xcopy /E /I /Y "%BUILD_DIR%" "%PROJECT_ROOT%\build\%RELEASE_NAME%"

@REM echo Renaming release folder...
@REM rename Desktop_Qt_6_9_1_MinGW_64_bit-Release %RELEASE_NAME%

echo Zipping release...
powershell Compress-Archive -Path "%PROJECT_ROOT%\build\%RELEASE_NAME%" -DestinationPath "%PROJECT_ROOT%\build\%RELEASE_NAME%.zip" -Force

echo Done!
pause