@echo off
REM ============================================================================
REM  HospitalMS compile script
REM  Data Structures Course Design - Problem 1
REM ----------------------------------------------------------------------------
REM  Usage (run in project root):
REM      build             compile main program
REM      build test        compile and run all self-tests
REM      build run         compile and run main program
REM      build gendata     compile and run the test data generator
REM      build clean       remove build outputs
REM
REM  This file is ASCII-only: cmd.exe misparses UTF-8 Chinese comments.
REM ============================================================================

setlocal

REM ---- compiler and flags ----
set CXX=g++
set CXXFLAGS=-std=c++17 -Wall -Wextra -I.
REM   -finput-charset / -fexec-charset: sources contain Chinese comments and
REM   string literals; MinGW must be told they are UTF-8 or it garbles them.
set CXXFLAGS=%CXXFLAGS% -finput-charset=UTF-8 -fexec-charset=UTF-8

if not exist bin mkdir bin

REM ============================================================================
REM  Module source lists
REM  Only .cpp files are listed, never .h: headers are pulled in by #include
REM  and cannot be compiled on their own. Template classes (ds/SeqList.h,
REM  ds/LinkedList.h, ds/Queue.h) keep their code in the header, so they have
REM  no .cpp and do not appear here.
REM
REM  When you add a new module, add its .cpp to the matching line below and
REM  to MAIN_SRC.
REM ============================================================================

set DS_SRC=ds\HashTable.cpp ds\Sort.cpp
set DATA_SRC=data\DataStore.cpp
set FILE_SRC=file\FileLoader.cpp
set UTIL_SRC=util\Console.cpp
set MENU_SRC=menu\Menu.cpp
set SERVICE_SRC=service\Triage.cpp service\DoctorQuery.cpp

REM ---- main program ----
REM   src\main.cpp is not written yet, so the build step below skips politely.
set MAIN_SRC=%DS_SRC% %DATA_SRC% %FILE_SRC% %SERVICE_SRC% %UTIL_SRC% %MENU_SRC%

REM ---- self-tests, one executable each ----
set TEST_DS_SRC=%DS_SRC% tests\test_ds.cpp
set TEST_CONSOLE_SRC=util\Console.cpp tests\test_console.cpp
set TEST_ENTITY_SRC=tests\test_entity.cpp
set TEST_LOADER_SRC=%DS_SRC% %DATA_SRC% %FILE_SRC% tests\test_loader.cpp
set TEST_TRIAGE_SRC=%DS_SRC% %DATA_SRC% %FILE_SRC% %UTIL_SRC% service\Triage.cpp tests\test_triage.cpp
set TEST_QUERY_SRC=%DS_SRC% %DATA_SRC% %FILE_SRC% %UTIL_SRC% service\DoctorQuery.cpp tests\test_query.cpp

REM ============================================================================
if "%1"=="clean"   goto :clean
if "%1"=="test"    goto :test
if "%1"=="run"     goto :run
if "%1"=="gendata" goto :gendata
goto :build_main

REM ---------------------------------------------------------------------------
:build_main
echo ==========================================================================
echo   Compiling HospitalMS
echo ==========================================================================

REM   The main program needs a main() function. Until src\main.cpp exists,
REM   linking would fail with "undefined reference to WinMain", so check first
REM   and skip politely instead of erroring out.
if not exist src\main.cpp (
    echo   [SKIP] src\main.cpp does not exist yet.
    echo          Nothing to build - the main program needs a main^(^) function.
    echo          Run "build test" to compile and run the self-tests instead.
    exit /b 0
)

%CXX% %CXXFLAGS% %MAIN_SRC% src\main.cpp -o bin\hospital.exe
if errorlevel 1 (
    echo.
    echo   [FAILED] compilation error
    exit /b 1
)
echo.
echo   [OK] bin\hospital.exe
exit /b 0

REM ---------------------------------------------------------------------------
:run
if not exist bin\hospital.exe (
    echo   main program not built yet - building in a separate process...
    cmd /c ""%~f0""
    if errorlevel 1 exit /b 1
)
if not exist bin\hospital.exe (
    echo   [SKIP] bin\hospital.exe still absent. Write src\main.cpp first.
    exit /b 1
)
echo.
echo   Running...
echo.
bin\hospital.exe
exit /b 0

REM ---------------------------------------------------------------------------
:gendata
REM   The data generator is a separate tool, not part of the main program,
REM   so it gets its own executable and its own build rule.
echo ==========================================================================
echo   Compiling test data generator
echo ==========================================================================
%CXX% %CXXFLAGS% tools\gen_data.cpp -o bin\gen_data.exe
if errorlevel 1 (
    echo.
    echo   [FAILED] compilation error
    exit /b 1
)
echo.
echo   Running generator, writing to files\
echo.
bin\gen_data.exe files
if errorlevel 1 (
    echo.
    echo   [FAILED] data generation reported errors
    exit /b 1
)
echo.
echo   [OK] files\ regenerated
exit /b 0

REM ---------------------------------------------------------------------------
:test
echo ==========================================================================
echo   Self-test 1/6 : data structures
echo ==========================================================================
%CXX% %CXXFLAGS% %TEST_DS_SRC% -o bin\test_ds.exe
if errorlevel 1 exit /b 1
bin\test_ds.exe
if errorlevel 1 (
    echo   [FAILED] test_ds
    exit /b 1
)

echo.
echo ==========================================================================
echo   Self-test 2/6 : data entities
echo ==========================================================================
%CXX% %CXXFLAGS% %TEST_ENTITY_SRC% -o bin\test_entity.exe
if errorlevel 1 exit /b 1
bin\test_entity.exe
if errorlevel 1 (
    echo   [FAILED] test_entity
    exit /b 1
)

echo.
echo ==========================================================================
echo   Self-test 3/6 : data loading
echo ==========================================================================
%CXX% %CXXFLAGS% %TEST_LOADER_SRC% -o bin\test_loader.exe
if errorlevel 1 exit /b 1
bin\test_loader.exe
if errorlevel 1 (
    echo   [FAILED] test_loader
    exit /b 1
)

echo.
echo ==========================================================================
echo   Self-test 4/6 : intelligent triage
echo ==========================================================================
%CXX% %CXXFLAGS% %TEST_TRIAGE_SRC% -o bin\test_triage.exe
if errorlevel 1 exit /b 1
bin\test_triage.exe
if errorlevel 1 (
    echo   [FAILED] test_triage
    exit /b 1
)

echo.
echo ==========================================================================
echo   Self-test 5/6 : doctor query
echo ==========================================================================
%CXX% %CXXFLAGS% %TEST_QUERY_SRC% -o bin\test_query.exe
if errorlevel 1 exit /b 1
bin\test_query.exe
if errorlevel 1 (
    echo   [FAILED] test_query
    exit /b 1
)

echo.
echo ==========================================================================
echo   Self-test 6/6 : console utilities
echo ==========================================================================
%CXX% %CXXFLAGS% %TEST_CONSOLE_SRC% -o bin\test_console.exe
if errorlevel 1 exit /b 1
bin\test_console.exe
if errorlevel 1 (
    echo   [FAILED] test_console
    exit /b 1
)

echo.
echo   All self-tests passed.
exit /b 0

REM ---------------------------------------------------------------------------
:clean
echo   Cleaning...
if exist bin rmdir /s /q bin
if exist output rmdir /s /q output
echo   Done.
exit /b 0