@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /MT /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE src\resource_test.cpp /Febuild\resource_test.exe /Fobuild\resource_test.obj winsqlite3.lib psapi.lib user32.lib shell32.lib ole32.lib uuid.lib
if errorlevel 1 exit /b 1
build\resource_test.exe legacy
if errorlevel 1 exit /b 1
build\resource_test.exe
exit /b %errorlevel%

