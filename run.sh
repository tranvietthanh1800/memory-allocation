#!/bin/bash
# ============================================
# Chay Memory Allocation Simulator (GTK3)
# ============================================
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -f "$DIR/build/MemoryAllocation" ]; then
    echo "Chua co build/MemoryAllocation"
    echo "Hay chay: ./build.sh"
    exit 1
fi

# 1. Xóa các biến môi trường bị nhiễm độc bởi VS Code (Snap)
unset GTK_PATH
unset GIO_MODULE_DIR

# 2. Ép chương trình dùng X11 (tránh lỗi antialiasing trên Ubuntu 24.04)
export GDK_BACKEND=x11

# 3. Chạy chương trình
"$DIR/build/MemoryAllocation"