@echo off
REM build.bat for Aur (CMake presets)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
setlocal EnableExtensions EnableDelayedExpansion
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "CONFIG=Release"
if /i "%~1"=="Debug" set "CONFIG=Debug"
if /i "%~1"=="clean" (
    if exist "%SCRIPT_DIR%\build" rmdir /s /q "%SCRIPT_DIR%\build"
    exit /b 0
)
if /i "%~1"=="rebuild" (
    if exist "%SCRIPT_DIR%\build" rmdir /s /q "%SCRIPT_DIR%\build"
)

cmake --preset %CONFIG% -S "%SCRIPT_DIR%" -B "%SCRIPT_DIR%\build\%CONFIG%" 2>&1
if errorlevel 1 exit /b 1
cmake --build "%SCRIPT_DIR%\build\%CONFIG%" -j 8
if errorlevel 1 exit /b 1
if exist "%SCRIPT_DIR%\build\%CONFIG%\Aur Studio.exe" "%QT_DIR%\bin\windeployqt.exe" "%SCRIPT_DIR%\build\%CONFIG%\Aur Studio.exe" >nul 2>&1
exit /b 0
