@echo off
setlocal
cd /d "%~dp0.."
set FOLDERSIZE_TEST=1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\prepare-tests.ps1
if errorlevel 1 exit /b 1
dist\WindowsFolderSizeProgressTest.exe "%CD%\build\scan-fixture"
if errorlevel 1 exit /b 1
start /b "" dist\WindowsFolderSize.exe
dist\WindowsFolderSizeTest.exe "%CD%\build" scan-fixture
set RESULT=%errorlevel%
if not "%RESULT%"=="0" goto stop
dist\WindowsFolderSizeCacheTest.exe
set RESULT=%errorlevel%
:stop
start /b /wait "" dist\WindowsFolderSize.exe --stop
if not "%RESULT%"=="0" exit /b %RESULT%
dist\WindowsFolderSizeDisplayTest.exe "%CD%\build" scan-fixture
exit /b %errorlevel%
