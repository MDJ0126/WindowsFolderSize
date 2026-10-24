@echo off
setlocal
cd /d "%~dp0.."
set FOLDERSIZE_TEST=1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\prepare-tests.ps1
if errorlevel 1 exit /b 1
build\bin\WindowsFolderSizeProgressTest.exe "%CD%\build\scan-fixture"
if errorlevel 1 exit /b 1
start /b "" build\bin\WindowsFolderSize.exe
build\bin\WindowsFolderSizeTest.exe "%CD%\build" scan-fixture
set RESULT=%errorlevel%
if not "%RESULT%"=="0" goto stop
build\bin\WindowsFolderSizeCacheTest.exe
set RESULT=%errorlevel%
:stop
start /b /wait "" build\bin\WindowsFolderSize.exe --stop
if not "%RESULT%"=="0" exit /b %RESULT%
build\bin\WindowsFolderSizeDisplayTest.exe "%CD%\build" scan-fixture
exit /b %errorlevel%
