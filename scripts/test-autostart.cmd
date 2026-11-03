@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build\autostart mkdir build\autostart
cl /nologo /O2 /MT /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE src\autostart_test.cpp /Fobuild\autostart_test.obj /Febuild\autostart\explorer.exe shell32.lib ole32.lib uuid.lib user32.lib
if errorlevel 1 exit /b 1
build\autostart\explorer.exe "%CD%\build\bin\WindowsFolderSizeHook.dll"
exit /b %errorlevel%
