@echo off
rem rekta-core with MSVC: build\rekta.lib, build\demo.exe and build\test.exe. Run from a Developer Command Prompt.
rem   build.bat         the library and the demo
rem   build.bat test    also the headless tests (the image comparison needs node)
setlocal
cd /d "%~dp0"
if not exist build mkdir build
set CFLAGS=/nologo /std:c11 /O2 /W4 /fp:precise /wd4100 /wd4996 /wd4244 /wd4267 /wd4305 /wd4456 /wd4457 /wd4459 /D_CRT_SECURE_NO_WARNINGS
cl %CFLAGS% /c src\rekta.c /Fobuild\rekta.obj || exit /b 1
lib /nologo build\rekta.obj /OUT:build\rekta.lib || exit /b 1
cl %CFLAGS% demo\demo.c build\rekta.lib user32.lib gdi32.lib advapi32.lib /Fobuild\demo.obj /Febuild\demo.exe /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup || exit /b 1
if /i "%1"=="test" (
  cl %CFLAGS% /DRK_HEADLESS_ONLY test\test.c /Fobuild\test.obj /Febuild\test.exe || exit /b 1
  build\test.exe build\test-out || exit /b 1
  node test\compare.mjs test\ref build\test-out || exit /b 1
)
echo built build\rekta.lib and build\demo.exe
