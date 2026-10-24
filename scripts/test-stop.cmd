@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /MT /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE src\stop_broker.cpp /Fobuild\stop_broker.obj /Febuild\bin\WindowsFolderSizeStop.exe shell32.lib user32.lib rstrtmgr.lib ole32.lib uuid.lib /link /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE src\stop_broker_test.cpp /Fobuild\stop_broker_test.obj /Febuild\stop_broker_test.exe shell32.lib user32.lib ole32.lib uuid.lib
if errorlevel 1 exit /b 1
build\stop_broker_test.exe
exit /b %errorlevel%
