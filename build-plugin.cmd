@echo off
setlocal

if "%~1"=="" (
    echo Usage: build-plugin.cmd ^<path-to-PluginSDK-SDK-0.4.0^>
    exit /b 2
)

set "ROOT=%~dp0"
set "SRC=%ROOT%src"
set "SDK_INCLUDE=%~1\include"
set "OUT=%TEMP%\d2rl-melee-splash-build-0.3.0"

if not exist "%SDK_INCLUDE%\D2RLPlugin\api.h" (
    echo Could not find D2RLoader SDK headers under "%SDK_INCLUDE%".
    exit /b 2
)

if not exist "%OUT%" mkdir "%OUT%"
if errorlevel 1 exit /b %errorlevel%

cl.exe /nologo /std:c++20 /W4 /permissive- /EHsc /utf-8 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /I"%SDK_INCLUDE%" /I"%SRC%" /c "%SRC%\plugin.cpp" /Fo"%OUT%\plugin.obj"
if errorlevel 1 exit /b %errorlevel%

cl.exe /nologo /std:c++20 /W4 /permissive- /EHsc /utf-8 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /I"%SDK_INCLUDE%" /I"%SRC%" /c "%SRC%\installer.cpp" /Fo"%OUT%\installer.obj"
if errorlevel 1 exit /b %errorlevel%

ml64.exe /nologo /c /Fo"%OUT%\pre_conversion.obj" "%SRC%\pre_conversion.asm"
if errorlevel 1 exit /b %errorlevel%

pushd "%SRC%"
rc.exe /nologo /I"%SDK_INCLUDE%" /fo"%OUT%\plugin.res" plugin.rc
if errorlevel 1 (
    popd
    exit /b 1
)
popd

link.exe /nologo /DLL /INCREMENTAL:NO /Brepro /MACHINE:X64 /OUT:"%OUT%\d2rl-melee-splash.dll" "%OUT%\plugin.obj" "%OUT%\installer.obj" "%OUT%\pre_conversion.obj" "%OUT%\plugin.res"
if errorlevel 1 exit /b %errorlevel%

echo Built: %OUT%\d2rl-melee-splash.dll
endlocal
