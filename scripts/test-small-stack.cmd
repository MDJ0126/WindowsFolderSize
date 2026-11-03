@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /MT src\small_stack_test.cpp /Febuild\small_stack_test.exe /Fobuild\small_stack_test.obj
if errorlevel 1 exit /b 1
build\small_stack_test.exe "%CD%\build\bin\WindowsFolderSizeHook.dll"
exit /b %errorlevel%
