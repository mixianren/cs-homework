@echo off
REM ============================================================================
REM  HospitalMS - start the program
REM ============================================================================
REM  Just double-click this file.
REM
REM  ---------------------------------------------------------------------------
REM  Why a .bat file is needed at all (the program itself handles directories):
REM
REM  bin\hospital.exe locates files\ by searching upward from the EXE's own
REM  location, so it already works no matter which directory you start it from.
REM  What it cannot do is keep the console window open after it exits -- if you
REM  double-click the .exe, Windows opens a window, the program runs, and the
REM  window closes the instant it finishes.  This script adds that final
REM  "press any key" so you can read the last screen.
REM
REM  ---------------------------------------------------------------------------
REM  ASCII only, on purpose.  cmd.exe misparses UTF-8 in .bat files, and that
REM  includes the FILE NAME: a .bat called with Chinese characters could not be
REM  resolved by cmd from this shell and hung.  Keep this name ASCII.
REM ============================================================================

REM ---- work from this script's folder, wherever it was invoked from ----
cd /d "%~dp0"

REM ---- build first if the exe is missing, so a fresh copy works in one click ----
if not exist "bin\hospital.exe" (
    echo   bin\hospital.exe not found - building first...
    echo.
    call build.bat
    if errorlevel 1 (
        echo.
        echo   Build failed.  Run "build test" to see details.
        echo.
        pause
        exit /b 1
    )
    echo.
)

echo   Starting HospitalMS...
echo.

REM ---- run ----
bin\hospital.exe

REM ---- keep the window open so the last screen is readable ----
REM   "nopause" as the first argument skips this.  Only automated checks use it,
REM   because they cannot press a key.  A double-click passes nothing.
echo.
if /i not "%1"=="nopause" (
    echo   Program exited.
    pause
)
