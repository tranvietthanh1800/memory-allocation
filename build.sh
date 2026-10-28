#!/bin/bash
# ============================================
# Build Memory Allocation Simulator - Ubuntu (GTK3)
# Yeu cau: build-essential, libgtk-3-dev
# ============================================

set -e

if ! pkg-config --exists gtk+-3.0; then
    echo "[LOI] Khong tim thay GTK3. Hay chay:"
    echo "  sudo apt install build-essential libgtk-3-dev"
    exit 1
fi

mkdir -p build

echo "[1/2] Bien dich cac file .c ..."
gcc -O2 -Wall -Wextra \
    src/main.c src/gui_gtk.c src/memory.c \
    src/first_fit.c src/next_fit.c src/best_fit.c src/worst_fit.c \
    src/file_io.c \
    -o build/MemoryAllocation \
    $(pkg-config --cflags --libs gtk+-3.0)

echo "[2/2] Xong. File: build/MemoryAllocation"