@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
if not exist build\bin\Symbols mkdir build\bin\Symbols
set COMMON=/nologo /O2 /MT /std:c++17 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00
if /I "%~1"=="setup" goto setup
rc /nologo /fo build\app.res src\app.rc
if errorlevel 1 exit /b 1
if /I "%~1"=="broker" goto broker
cl %COMMON% /c third_party\minhook\src\buffer.c third_party\minhook\src\hook.c third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde64.c /Fobuild\
if errorlevel 1 exit /b 1
:broker
cl %COMMON% src\agent.cpp build\app.res /Fobuild\agent.obj /Febuild\bin\WindowsFolderSize.exe shell32.lib advapi32.lib user32.lib ole32.lib uuid.lib /link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
if /I "%~1"=="broker" exit /b 0
rc /nologo /fo build\hook.res src\hook.rc
if errorlevel 1 exit /b 1
cl %COMMON% /LD src\hook.cpp build\hook.res /Ithird_party\minhook\include build\buffer.obj build\hook.obj build\trampoline.obj build\hde64.obj /Fobuild\extension.obj /Febuild\bin\WindowsFolderSizeHook.dll shell32.lib shlwapi.lib ole32.lib propsys.lib uuid.lib /link /DEF:src\hook.def /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
cl %COMMON% src\symbols.cpp build\app.res /Fobuild\symbols.obj /Febuild\bin\Symbols\WindowsFolderSizeSymbols.exe dbghelp.lib shell32.lib ole32.lib uuid.lib /link /SUBSYSTEM:CONSOLE /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
copy /y "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\dbghelp.dll" build\bin\Symbols\ >nul
copy /y "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\symsrv.dll" build\bin\Symbols\ >nul
cl %COMMON% src\integration_test.cpp /Fobuild\test.obj /Febuild\bin\WindowsFolderSizeTest.exe shell32.lib ole32.lib oleaut32.lib propsys.lib uuid.lib /link /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
cl %COMMON% src\progress_test.cpp /Fobuild\progress_test.obj /Febuild\bin\WindowsFolderSizeProgressTest.exe /link /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
cl %COMMON% src\display_test.cpp /Fobuild\display_test.obj /Febuild\bin\WindowsFolderSizeDisplayTest.exe shell32.lib shlwapi.lib ole32.lib oleaut32.lib propsys.lib uuid.lib /link /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
cl %COMMON% src\cache_test.cpp /Fobuild\cache_test.obj /Febuild\bin\WindowsFolderSizeCacheTest.exe /link /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
:setup
cl %COMMON% src\msi_finish_patch.cpp /Fobuild\msi_finish_patch.obj /Febuild\msi_finish_patch.exe msi.lib
if errorlevel 1 exit /b 1
cl %COMMON% src\restart_policy_test.cpp /Fobuild\restart_policy_test.obj /Febuild\restart_policy_test.exe
if errorlevel 1 exit /b 1
build\restart_policy_test.exe
if errorlevel 1 exit /b 1
cl %COMMON% src\maintenance.cpp build\app.res /Fobuild\maintenance.obj /Febuild\bin\WindowsFolderSizeMaintenance.exe shell32.lib user32.lib rstrtmgr.lib ole32.lib uuid.lib /link /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
if /I "%~1"=="native" exit /b 0
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-msi.ps1
exit /b %errorlevel%
