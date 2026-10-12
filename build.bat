@echo off
REM ============================================================================
REM  HospitalMS - build script
REM ----------------------------------------------------------------------------
REM  This file is ASCII only.  cmd.exe misparses UTF-8 in .bat files.
REM
REM  Current stage: data structure design.
REM
REM    ds\      data structures        complete (SeqList, LinkedList, Queue,
REM                                    HashTable, Sort)
REM    model\   entity structs         complete (Entity.h)
REM    everything else                 header / skeleton only, not implemented
REM
REM  Because most modules are still skeletons, the program cannot be linked yet.
REM  This script reports the layout and is ready for the compile rules to be
REM  filled in as the modules are written.
REM
REM  Usage:
REM      build             report the current layout
REM      build clean       remove build output
REM ============================================================================

setlocal
set CXX=g++
set CXXFLAGS=-std=c++17 -Wall -Wextra -I. -finput-charset=UTF-8 -fexec-charset=UTF-8

if not exist bin mkdir bin

if "%1"=="clean" goto :clean

echo ==========================================================================
echo   HospitalMS
echo ==========================================================================
echo.
echo   Stage: data structure design
echo.
echo   Complete:
echo     ds\      SeqList.h  LinkedList.h  Queue.h
echo              HashTable.h  HashTable.cpp  Sort.h  Sort.cpp
echo     model\   Entity.h
echo.
echo   Skeleton only (not implemented):
echo     data\    DataStore.h  DataStore.cpp
echo     file\    FileLoader.h  FileLoader.cpp
echo     service\ Triage.h  Triage.cpp  DoctorQuery.h  DoctorQuery.cpp
echo     util\    Console.h  Console.cpp
echo     menu\    Menu.h  Menu.cpp
echo     src\     main.cpp
echo     tests\   test_ds / test_entity / test_loader / test_triage
echo              test_query / test_console
echo.
echo   The program cannot be linked until the modules above are written.
echo.
echo   Compile rules are added here as the implementations are completed.
echo.
exit /b 0

:clean
echo   Cleaning...
if exist bin rmdir /s /q bin
if exist output rmdir /s /q output
echo   Done.
exit /b 0
