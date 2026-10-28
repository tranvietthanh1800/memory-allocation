@echo off
REM ============================================
REM Chay Memory Allocation Simulator (GUI)
REM Khong hien console (start "" de tach process)
REM ============================================
if not exist build\MemoryAllocation.exe (
    echo Chua co build\MemoryAllocation.exe
    echo Hay chay build.bat truoc.
    pause
    exit /b 1
)
start "" "%~dp0build\MemoryAllocation.exe"