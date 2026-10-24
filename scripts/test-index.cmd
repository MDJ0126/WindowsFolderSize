@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /MT /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE src\file_index_test.cpp /Fobuild\file_index_test.obj /Febuild\file_index_test.exe shell32.lib ole32.lib uuid.lib user32.lib
if errorlevel 1 exit /b 1
build\file_index_test.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE src\persistence_test.cpp /Fobuild\persistence_test.obj /Febuild\persistence_test.exe shell32.lib ole32.lib uuid.lib user32.lib
if errorlevel 1 exit /b 1
build\persistence_test.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE src\burst_test.cpp /Fobuild\burst_test.obj /Febuild\burst_test.exe shell32.lib ole32.lib uuid.lib user32.lib
if errorlevel 1 exit /b 1
build\burst_test.exe
exit /b %errorlevel%
