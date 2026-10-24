@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /MT /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE src\partial_test.cpp /Fobuild\partial_test.obj /Febuild\partial_test.exe shell32.lib ole32.lib uuid.lib advapi32.lib propsys.lib
if errorlevel 1 exit /b 1
build\partial_test.exe
exit /b %errorlevel%
