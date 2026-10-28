#include <windows.h>
#include "gui.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int nShow) {
    (void)hPrev; (void)cmd;
    return gui_run(hInst, nShow);
}