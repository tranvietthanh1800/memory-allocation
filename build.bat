@echo off
REM ============================================
REM Build Memory Allocation Simulator - Win32
REM Yeu cau: MinGW-w64 (gcc) trong PATH
REM ============================================
setlocal

where gcc >nul 2>nul
if errorlevel 1 (
    echo [LOI] Khong tim thay gcc. Cai MinGW-w64 va them vao PATH.
    echo Vi du: https://www.msys2.org/
    pause
    exit /b 1
)

if not exist build mkdir build

echo [1/2] Bien dich cac file .c ...
gcc -O2 -Wall -Wextra ^
    src\main.c src\gui.c src\memory.c ^
    src\first_fit.c src\next_fit.c src\best_fit.c src\worst_fit.c ^
    src\file_io.c ^
    -o build\MemoryAllocation.exe ^
    -mwindows -lgdi32 -lcomctl32 -lcomdlg32 -luser32 -lkernel32
if errorlevel 1 (
    echo [LOI] Bien dich that bai.
    pause
    exit /b 1
)

if not exist results mkdir results

echo [2/2] Xong. File: build\MemoryAllocation.exe
pause