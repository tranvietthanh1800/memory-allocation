#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    #include "gui.h"

    int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int nShow) {
        (void)hPrev; (void)cmd;
        return gui_run(hInst, nShow);
    }
#else
    // Trên Linux, hàm main() nằm trong file gui_gtk.c
    // File này để trống để tránh lỗi multiple definition
#endif