@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build mkdir build
cl /nologo /O2 /MT /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 src\async_cache_test.cpp /Fobuild\async_cache_test.obj /Febuild\async_cache_test.exe shell32.lib ole32.lib uuid.lib user32.lib
if errorlevel 1 exit /b 1
build\async_cache_test.exe
exit /b %errorlevel%
