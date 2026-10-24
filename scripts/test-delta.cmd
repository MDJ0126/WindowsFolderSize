@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /MT /std:c++17 /EHsc /DUNICODE /D_UNICODE src\delta_test.cpp /Fobuild\delta_test.obj /Febuild\delta_test.exe shell32.lib ole32.lib uuid.lib user32.lib
if errorlevel 1 exit /b 1
build\delta_test.exe
exit /b %errorlevel%
