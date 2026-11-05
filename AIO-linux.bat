@echo off
rem ============================================================================
rem  AIO-linux -- the Linux part of AIO.bat for Aur. Called by AIO.bat.
rem
rem  A Windows host cannot produce a Linux Qt binary, so this file says what to
rem  run on a Linux machine and returns 3. It is a named instruction, not a
rem  silent success -- an unimplemented verb must say so.
rem  On Linux the same presets apply: Release/Debug -> build/Release, build/Debug.
rem  Exit 0 pass, 1 fail, 3 not implemented on this host.
rem ============================================================================
set "AIO_APP=Aur"
if /i "%CONFIG%"=="Debug" (set "CFG=Debug") else (set "CFG=Release")

if /i "%VERB%"=="clean" (
    if exist "%AIO_HOME%\build" (
        echo [linux] on the Linux host: rm -rf build
        exit /b 3
    )
    exit /b 0
)

echo [linux] Aur is Windows/Linux; this host is Windows.
echo [linux] on a Linux machine with Qt 6.12 installed, from the project root:
if /i "%VERB%"=="build"   echo [linux]   cmake --preset %CFG%  ^&^& cmake --build build/%CFG% -j 8
if /i "%VERB%"=="compile" echo [linux]   cmake --build build/%CFG% -j 8
if /i "%VERB%"=="debug"   echo [linux]   cmake --preset Debug  ^&^& cmake --build build/Debug -j 8
if /i "%VERB%"=="run"     echo [linux]   ./build/%CFG%/Aur Studio
if /i "%VERB%"=="test"    echo [linux]   ctest --test-dir build/%CFG% --output-on-failure --timeout 300
if /i "%VERB%"=="deploy"  echo [linux]   Qt libraries are system packages; nothing to package
if /i "%VERB%"=="--list"  echo [linux]   presets Release/Debug -> build\Release, build\Debug
exit /b 3
