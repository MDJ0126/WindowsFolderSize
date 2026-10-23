@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
if not exist dist\Symbols mkdir dist\Symbols
set COMMON=/nologo /O2 /MT /std:c++17 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00
if /I "%~1"=="setup" goto setup
rc /nologo /fo build\app.res src\app.rc
if errorlevel 1 exit /b 1
cl %COMMON% /c third_party\minhook\src\buffer.c third_party\minhook\src\hook.c third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde64.c /Fobuild\
if errorlevel 1 exit /b 1
cl %COMMON% src\agent.cpp build\app.res /Fobuild\agent.obj /Fedist\WindowsFolderSize.exe shell32.lib advapi32.lib user32.lib ole32.lib uuid.lib /link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
rc /nologo /fo build\hook.res src\hook.rc
if errorlevel 1 exit /b 1
cl %COMMON% /LD src\hook.cpp build\hook.res /Ithird_party\minhook\include build\buffer.obj build\hook.obj build\trampoline.obj build\hde64.obj /Fobuild\extension.obj /Fedist\WindowsFolderSizeHook.dll shell32.lib shlwapi.lib ole32.lib propsys.lib uuid.lib /link /DEF:src\hook.def /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
cl %COMMON% src\symbols.cpp build\app.res /Fobuild\symbols.obj /Fedist\Symbols\WindowsFolderSizeSymbols.exe dbghelp.lib shell32.lib ole32.lib uuid.lib /link /SUBSYSTEM:CONSOLE /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
copy /y "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\dbghelp.dll" dist\Symbols\ >nul
copy /y "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\symsrv.dll" dist\Symbols\ >nul
cl %COMMON% src\integration_test.cpp /Fobuild\test.obj /Fedist\WindowsFolderSizeTest.exe shell32.lib ole32.lib oleaut32.lib propsys.lib uuid.lib /link /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
cl %COMMON% src\progress_test.cpp /Fobuild\progress_test.obj /Fedist\WindowsFolderSizeProgressTest.exe /link /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
cl %COMMON% src\display_test.cpp /Fobuild\display_test.obj /Fedist\WindowsFolderSizeDisplayTest.exe shell32.lib shlwapi.lib ole32.lib oleaut32.lib propsys.lib uuid.lib /link /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
cl %COMMON% src\cache_test.cpp /Fobuild\cache_test.obj /Fedist\WindowsFolderSizeCacheTest.exe /link /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
:setup
cl %COMMON% src\maintenance.cpp build\app.res /Fobuild\maintenance.obj /Fedist\WindowsFolderSizeMaintenance.exe shell32.lib user32.lib rstrtmgr.lib /link /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
if /I "%~1"=="native" exit /b 0
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-msi.ps1
exit /b %errorlevel%
