#include "gui.h"
#include "file_io.h"
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

/* ---------- ID điều khiển ---------- */
#define IDC_EDIT_BLOCK       1001
#define IDC_BTN_ADD_BLOCK    1002
#define IDC_BTN_DEL_BLOCK    1003
#define IDC_LIST_BLOCKS      1004
#define IDC_EDIT_PROC        1005
#define IDC_BTN_ADD_PROC     1006
#define IDC_BTN_DEL_PROC     1007
#define IDC_LIST_PROCS       1008
#define IDC_BTN_FIRST        1010
#define IDC_BTN_NEXT         1011
#define IDC_BTN_BEST         1012
#define IDC_BTN_WORST        1013
#define IDC_BTN_COMPARE      1014
#define IDC_LIST_RESULT      1015
#define IDC_LIST_CMP         1016
#define IDC_STATIC_STATUS    1017
#define IDC_BTN_LOAD         1018
#define IDC_BTN_SAVE         1019
#define IDC_BTN_CLEAR        1020
#define IDC_BTN_HELP         1021
#define IDC_VISUAL           1022
#define IDC_STATIC_STATS     1023

/* ---------- Trạng thái toàn cục ---------- */
static HINSTANCE g_hInst;
static HWND g_hMain;
static HWND g_hEditBlock, g_hEditProc;
static HWND g_hListBlocks, g_hListProcs, g_hListResult, g_hListCmp;
static HWND g_hVisual, g_hStatus, g_hStats;

/* Dữ liệu hiện tại */
static int     g_blockSizes[MAX_BLOCKS];
static int     g_nBlocks = 0;
static int     g_procSizes[MAX_PROCESSES];
static int     g_nProcs  = 0;

/* Kết quả đang hiển thị (để vẽ visual) */
static Result  g_lastResult;
static int     g_hasResult = 0;
static char    g_lastAlgo[64] = "";

/* ---------- Hàm tiện ích ---------- */
static void set_status(const char *msg) {
    SetWindowTextA(g_hStatus, msg);
}

static void refresh_block_list(void) {
    ListView_DeleteAllItems(g_hListBlocks);
    for (int i = 0; i < g_nBlocks; i++) {
        LVITEMA it = {0};
        char buf[32];
        it.mask = LVIF_TEXT;
        it.iItem = i;
        sprintf(buf, "Block %d", i);
        it.pszText = buf;
        ListView_InsertItem(g_hListBlocks, &it);
        sprintf(buf, "%d", g_blockSizes[i]);
        ListView_SetItemText(g_hListBlocks, i, 1, buf);
    }
}

static void refresh_proc_list(void) {
    ListView_DeleteAllItems(g_hListProcs);
    for (int i = 0; i < g_nProcs; i++) {
        LVITEMA it = {0};
        char buf[32];
        it.mask = LVIF_TEXT;
        it.iItem = i;
        sprintf(buf, "P%d", i + 1);
        it.pszText = buf;
        ListView_InsertItem(g_hListProcs, &it);
        sprintf(buf, "%d", g_procSizes[i]);
        ListView_SetItemText(g_hListProcs, i, 1, buf);
    }
}

static void show_result_list(const Result *r, const char *algo) {
    ListView_DeleteAllItems(g_hListResult);
    for (int i = 0; i < r->n_procs; i++) {
        const Process *p = &r->procs[i];
        LVITEMA it = {0};
        char c1[32], c2[32], c3[32], c4[64], c5[32];
        it.mask = LVIF_TEXT;
        it.iItem = i;
        sprintf(c1, "P%d", i + 1);
        it.pszText = c1;
        ListView_InsertItem(g_hListResult, &it);
        sprintf(c2, "%d", p->size);
        ListView_SetItemText(g_hListResult, i, 1, c2);
        if (p->status) {
            sprintf(c3, "Block %d", p->allocated_block);
            sprintf(c4, "%d", r->blocks[p->allocated_block].remaining);
            strcpy(c5, "SUCCESS");
        } else {
            strcpy(c3, "-");
            strcpy(c4, "-");
            strcpy(c5, "FAIL");
        }
        ListView_SetItemText(g_hListResult, i, 2, c3);
        ListView_SetItemText(g_hListResult, i, 3, c4);
        ListView_SetItemText(g_hListResult, i, 4, c5);
    }

    char stats[256];
    sprintf(stats, "%s | Success: %d | Failed: %d | Free memory: %d",
            algo, r->total_allocated, r->total_failed, r->total_free_mem);
    SetWindowTextA(g_hStats, stats);

    char st[128];
    sprintf(st, "Da chay %s", algo);
    set_status(st);
}

static void show_compare_list(const Result *r[4]) {
    const char *names[4] = {"First Fit", "Next Fit", "Best Fit", "Worst Fit"};
    ListView_DeleteAllItems(g_hListCmp);
    for (int i = 0; i < 4; i++) {
        LVITEMA it = {0};
        char c0[32], c1[32], c2[32], c3[32], c4[32];
        it.mask = LVIF_TEXT;
        it.iItem = i;
        it.pszText = (char *)names[i];
        ListView_InsertItem(g_hListCmp, &it);
        sprintf(c1, "%d", r[i]->total_allocated);
        sprintf(c2, "%d", r[i]->total_failed);
        sprintf(c3, "%d", r[i]->total_free_mem);

        /* Fragment lớn nhất còn lại */
        int maxFrag = 0;
        for (int k = 0; k < r[i]->n_blocks; k++)
            if (r[i]->blocks[k].remaining > maxFrag)
                maxFrag = r[i]->blocks[k].remaining;
        sprintf(c4, "%d", maxFrag);

        (void)c0;
        ListView_SetItemText(g_hListCmp, i, 1, c1);
        ListView_SetItemText(g_hListCmp, i, 2, c2);
        ListView_SetItemText(g_hListCmp, i, 3, c3);
        ListView_SetItemText(g_hListCmp, i, 4, c4);
    }
    set_status("Da so sanh 4 thuat toan");
}

static void refresh_visual(void) {
    InvalidateRect(g_hVisual, NULL, TRUE);
    UpdateWindow(g_hVisual);
}

/* Vẽ sơ đồ block: dùng GDI, mỗi block 1 hàng ngang */
static void draw_visual(HDC hdc, RECT *rc) {
    FillRect(hdc, rc, (HBRUSH)(COLOR_WINDOW + 1));

    if (!g_hasResult || g_lastResult.n_blocks == 0) {
        SetTextColor(hdc, RGB(120, 120, 120));
        SetBkMode(hdc, TRANSPARENT);
        DrawTextA(hdc, "Chay mot thuat toan de xem truc quan bo nho...",
                  -1, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    const int padX = 20, padY = 20, labelW = 80;
    int usableW = (rc->right - rc->left) - padX * 2 - labelW;
    int rowH = (rc->bottom - rc->top - padY * 2) / g_lastResult.n_blocks;
    if (rowH > 60) rowH = 60;
    if (rowH < 22) rowH = 22;

    /* Tổng ban đầu để scale theo block lớn nhất */
    int maxSize = 1;
    for (int i = 0; i < g_lastResult.n_blocks; i++)
        if (g_lastResult.blocks[i].size > maxSize)
            maxSize = g_lastResult.blocks[i].size;

    SetBkMode(hdc, TRANSPARENT);
    for (int i = 0; i < g_lastResult.n_blocks; i++) {
        Block *b = &g_lastResult.blocks[i];
        int y = padY + i * rowH;

        /* Nhãn "Block i" */
        char label[32];
        sprintf(label, "Block %d", i);
        SetTextColor(hdc, RGB(0, 0, 0));
        RECT lr = {rc->left + padX, y, rc->left + padX + labelW - 8, y + rowH};
        DrawTextA(hdc, label, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        /* Thanh block: tổng chiều rộng tương ứng size */
        int barX = rc->left + padX + labelW;
        int barW = (int)((double)b->size / maxSize * usableW);
        int barY = y + 4;
        int barH = rowH - 8;

        /* Phần đã cấp phát (bên trái) */
        int usedW = (int)((double)b->used_size / b->size * barW);
        if (usedW > 0) {
            RECT ur = {barX, barY, barX + usedW, barY + barH};
            HBRUSH br = CreateSolidBrush(RGB(70, 160, 240)); /* xanh = đã cấp */
            FillRect(hdc, &ur, br);
            DeleteObject(br);
            /* Tên process */
            if (b->process_id >= 0) {
                char pname[16];
                sprintf(pname, "P%d", b->process_id + 1);
                SetTextColor(hdc, RGB(255, 255, 255));
                DrawTextA(hdc, pname, -1, &ur,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        /* Phần còn trống (bên phải) */
        if (b->remaining > 0) {
            RECT fr = {barX + usedW, barY, barX + barW, barY + barH};
            HBRUSH br = CreateSolidBrush(RGB(210, 230, 210)); /* xanh nhạt = trống */
            FillRect(hdc, &fr, br);
            DeleteObject(br);
            char fname[32];
            sprintf(fname, "free %d", b->remaining);
            SetTextColor(hdc, RGB(30, 90, 30));
            DrawTextA(hdc, fname, -1, &fr,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        /* Viền block */
        RECT br2 = {barX, barY, barX + barW, barY + barH};
        FrameRect(hdc, &br2, (HBRUSH)GetStockObject(BLACK_BRUSH));

        /* Nhãn kích thước tổng */
        char sz[32];
        sprintf(sz, "%d", b->size);
        SetTextColor(hdc, RGB(80, 80, 80));
        RECT sr = {barX + barW + 6, y, rc->right - padX, y + rowH};
        DrawTextA(hdc, sz, -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    /* Chú thích màu */
    int ly = rc->bottom - 22;
    HBRUSH br1 = CreateSolidBrush(RGB(70, 160, 240));
    HBRUSH br2 = CreateSolidBrush(RGB(210, 230, 210));
    RECT r1 = {rc->left + padX, ly, rc->left + padX + 16, ly + 14};
    RECT r2 = {rc->left + padX + 120, ly, rc->left + padX + 136, ly + 14};
    FillRect(hdc, &r1, br1);
    FillRect(hdc, &r2, br2);
    DeleteObject(br1); DeleteObject(br2);
    SetTextColor(hdc, RGB(0, 0, 0));
    RECT t1 = {rc->left + padX + 22, ly - 2, rc->left + padX + 118, ly + 16};
    RECT t2 = {rc->left + padX + 142, ly - 2, rc->left + padX + 260, ly + 16};
    DrawTextA(hdc, "Da cap phat", -1, &t1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextA(hdc, "Con trong", -1, &t2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* ---------- Logic thêm/xóa block & process ---------- */
static void add_block_from_edit(void) {
    char buf[64];
    GetWindowTextA(g_hEditBlock, buf, sizeof buf);
    int v = atoi(buf);
    if (v <= 0) { set_status("Kich thuoc block phai > 0"); return; }
    if (g_nBlocks >= MAX_BLOCKS) { set_status("Da dat gioi han block"); return; }
    g_blockSizes[g_nBlocks++] = v;
    refresh_block_list();
    SetWindowTextA(g_hEditBlock, "");
    set_status("Da them block");
}

static void del_selected_block(void) {
    int sel = ListView_GetNextItem(g_hListBlocks, -1, LVNI_SELECTED);
    if (sel < 0) { set_status("Chua chon block de xoa"); return; }
    for (int i = sel; i < g_nBlocks - 1; i++)
        g_blockSizes[i] = g_blockSizes[i + 1];
    g_nBlocks--;
    refresh_block_list();
    set_status("Da xoa block");
}

static void add_proc_from_edit(void) {
    char buf[64];
    GetWindowTextA(g_hEditProc, buf, sizeof buf);
    int v = atoi(buf);
    if (v <= 0) { set_status("Kich thuoc process phai > 0"); return; }
    if (g_nProcs >= MAX_PROCESSES) { set_status("Da dat gioi han process"); return; }
    g_procSizes[g_nProcs++] = v;
    refresh_proc_list();
    SetWindowTextA(g_hEditProc, "");
    set_status("Da them process");
}

static void del_selected_proc(void) {
    int sel = ListView_GetNextItem(g_hListProcs, -1, LVNI_SELECTED);
    if (sel < 0) { set_status("Chua chon process de xoa"); return; }
    for (int i = sel; i < g_nProcs - 1; i++)
        g_procSizes[i] = g_procSizes[i + 1];
    g_nProcs--;
    refresh_proc_list();
    set_status("Da xoa process");
}

/* ---------- Chuẩn bị input cho thuật toán ---------- */
static int build_input(Block *blocks, Process *procs) {
    if (g_nBlocks == 0) { set_status("Chua co block nao"); return 0; }
    if (g_nProcs  == 0) { set_status("Chua co process nao"); return 0; }
    for (int i = 0; i < g_nBlocks; i++) {
        blocks[i].id = i;
        blocks[i].size = g_blockSizes[i];
        blocks[i].remaining = g_blockSizes[i];
        blocks[i].process_id = -1;
        blocks[i].used_size = 0;
    }
    for (int i = 0; i < g_nProcs; i++) {
        procs[i].id = i;
        procs[i].size = g_procSizes[i];
        procs[i].allocated_block = -1;
        procs[i].status = 0;
    }
    return 1;
}

static void run_one_algo(int algo /* 0..3 */) {
    Block initB[MAX_BLOCKS];
    Process initP[MAX_PROCESSES];
    if (!build_input(initB, initP)) return;

    switch (algo) {
        case 0: run_first_fit(initB, g_nBlocks, initP, g_nProcs, &g_lastResult);
                strcpy(g_lastAlgo, "First Fit"); break;
        case 1: run_next_fit(initB, g_nBlocks, initP, g_nProcs, &g_lastResult);
                strcpy(g_lastAlgo, "Next Fit"); break;
        case 2: run_best_fit(initB, g_nBlocks, initP, g_nProcs, &g_lastResult);
                strcpy(g_lastAlgo, "Best Fit"); break;
        case 3: run_worst_fit(initB, g_nBlocks, initP, g_nProcs, &g_lastResult);
                strcpy(g_lastAlgo, "Worst Fit"); break;
    }
    g_hasResult = 1;
    show_result_list(&g_lastResult, g_lastAlgo);
    refresh_visual();
}

static void run_compare(void) {
    Block initB[MAX_BLOCKS];
    Process initP[MAX_PROCESSES];
    if (!build_input(initB, initP)) return;

    static Result r0, r1, r2, r3;
    run_first_fit (initB, g_nBlocks, initP, g_nProcs, &r0);
    run_next_fit  (initB, g_nBlocks, initP, g_nProcs, &r1);
    run_best_fit  (initB, g_nBlocks, initP, g_nProcs, &r2);
    run_worst_fit (initB, g_nBlocks, initP, g_nProcs, &r3);

    const Result *rs[4] = {&r0, &r1, &r2, &r3};
    show_compare_list(rs);

    /* Visual hóa kết quả của First Fit */
    g_lastResult = r0;
    g_hasResult = 1;
    strcpy(g_lastAlgo, "First Fit (compare)");
    refresh_visual();

    set_status("Da so sanh 4 thuat toan - xem bang ben phai");
}

/* ---------- Load / Save file ---------- */
static void do_load_file(void) {
    OPENFILENAMEA ofn = {0};
    char path[MAX_PATH] = "data\\input.txt";
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameA(&ofn)) return;

    int bs[MAX_BLOCKS], ps[MAX_PROCESSES], nb = 0, np = 0;
    char err[256];
    if (!load_input_file(path, bs, &nb, ps, &np, err, sizeof err)) {
        MessageBoxA(g_hMain, err, "Loi doc file", MB_ICONERROR);
        return;
    }
    g_nBlocks = nb; memcpy(g_blockSizes, bs, sizeof(int) * nb);
    g_nProcs  = np; memcpy(g_procSizes,  ps, sizeof(int) * np);
    refresh_block_list();
    refresh_proc_list();
    g_hasResult = 0;
    refresh_visual();
    set_status("Da load input.txt thanh cong");
}

static void do_save_file(void) {
    if (!g_hasResult) {
        MessageBoxA(g_hMain, "Chua chay thuat toan nao de luu.",
                    "Thong bao", MB_ICONINFORMATION);
        return;
    }
    OPENFILENAMEA ofn = {0};
    char path[MAX_PATH] = "result.txt";
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "txt";

    if (!GetSaveFileNameA(&ofn)) return;

    char err[256];
    if (!save_result_file(path, g_lastAlgo,
                          g_blockSizes, g_nBlocks,
                          g_procSizes,  g_nProcs,
                          &g_lastResult, err, sizeof err)) {
        MessageBoxA(g_hMain, err, "Loi ghi file", MB_ICONERROR);
        return;
    }
    char msg[512];
    sprintf(msg, "Da luu ket qua vao:\n%s", path);
    MessageBoxA(g_hMain, msg, "Thanh cong", MB_ICONINFORMATION);
    set_status("Da luu ket qua");
}

static void do_clear_all(void) {
    g_nBlocks = 0;
    g_nProcs  = 0;
    g_hasResult = 0;
    refresh_block_list();
    refresh_proc_list();
    ListView_DeleteAllItems(g_hListResult);
    ListView_DeleteAllItems(g_hListCmp);
    SetWindowTextA(g_hStats, "");
    refresh_visual();
    set_status("Da lam moi du lieu");
}

static void do_help(void) {
    MessageBoxA(g_hMain,
        "HUONG DAN SU DUNG\n"
        "------------------\n"
        "1. Nhap kich thuoc Block -> Add. Co the xoa bang Delete.\n"
        "2. Nhap kich thuoc Process -> Add. Co the xoa bang Delete.\n"
        "3. Bam First Fit / Next Fit / Best Fit / Worst Fit de chay.\n"
        "4. Bam Compare All Algorithms de so sanh ca 4.\n"
        "5. Bam Load input.txt de doc file du lieu.\n"
        "6. Bam Save result.txt de luu ket qua.\n"
        "7. Vung 'Truc quan bo nho' hien thi block da cap / con trong.\n"
        "8. Bang so sanh hien thi so process thanh cong/that bai va bo nho con lai.",
        "Huong dan", MB_ICONINFORMATION);
}

/* ---------- Tạo control con ---------- */
static HWND make_static(HWND parent, const char *txt, int x, int y, int w, int h,
                        HFONT font) {
    HWND h = CreateWindowExA(0, "STATIC", txt,
                             WS_CHILD | WS_VISIBLE | SS_LEFT,
                             x, y, w, h, parent, NULL, g_hInst, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)font, TRUE);
    return h;
}

static HWND make_edit(HWND parent, int id, int x, int y, int w, int h, HFONT font) {
    HWND e = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                             WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                             x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    SendMessageA(e, WM_SETFONT, (WPARAM)font, TRUE);
    return e;
}

static HWND make_button(HWND parent, int id, const char *txt,
                        int x, int y, int w, int h, HFONT font) {
    HWND b = CreateWindowExA(0, "BUTTON", txt,
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    SendMessageA(b, WM_SETFONT, (WPARAM)font, TRUE);
    return b;
}

static HWND make_listview(HWND parent, int id, int x, int y, int w, int h,
                          const char **cols, int ncols, int *widths, HFONT font) {
    HWND lv = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                              WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                              x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    ListView_SetExtendedListViewStyle(lv,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    SendMessageA(lv, WM_SETFONT, (WPARAM)font, TRUE);
    for (int i = 0; i < ncols; i++) {
        LVCOLUMNA c = {0};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        c.pszText = (char *)cols[i];
        c.cx = widths[i];
        c.iSubItem = i;
        ListView_InsertColumn(lv, i, &c);
    }
    return lv;
}

/* ---------- Tạo giao diện ---------- */
static void create_ui(HWND hwnd) {
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT hFontBold = CreateFontA(-16, 0, 0, 0, FW_BOLD, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT hFontTitle = CreateFontA(-22, 0, 0, 0, FW_BOLD, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    /* --- Header --- */
    HWND t1 = make_static(hwnd, "Memory Allocation Simulator", 20, 12, 500, 32, hFontTitle);
    SetTextColor(t1, RGB(20, 60, 160));
    (void)make_static(hwnd, "First Fit  |  Next Fit  |  Best Fit  |  Worst Fit",
                      20, 46, 500, 20, hFont);
    make_button(hwnd, IDC_BTN_HELP, "Huong dan", 1080, 16, 110, 28, hFont);

    /* --- Block section (trái trên) --- */
    int y0 = 80;
    make_static(hwnd, "MEMORY BLOCKS", 20, y0, 260, 22, hFontBold);
    g_hEditBlock = make_edit(hwnd, IDC_EDIT_BLOCK, 20, y0 + 26, 90, 26, hFont);
    make_button(hwnd, IDC_BTN_ADD_BLOCK, "Add Block", 116, y0 + 26, 90, 26, hFont);
    make_button(hwnd, IDC_BTN_DEL_BLOCK, "Delete", 212, y0 + 26, 80, 26, hFont);

    const char *colsB[] = {"Block", "Size"};
    int wB[] = {90, 60};
    g_hListBlocks = make_listview(hwnd, IDC_LIST_BLOCKS,
                                  20, y0 + 60, 260, 160, colsB, 2, wB, hFont);

    /* --- Process section (giữa trên) --- */
    int x1 = 300;
    make_static(hwnd, "PROCESSES", x1, y0, 260, 22, hFontBold);
    g_hEditProc = make_edit(hwnd, IDC_EDIT_PROC, x1, y0 + 26, 90, 26, hFont);
    make_button(hwnd, IDC_BTN_ADD_PROC, "Add Process", x1 + 96, y0 + 26, 90, 26, hFont);
    make_button(hwnd, IDC_BTN_DEL_PROC, "Delete", x1 + 192, y0 + 26, 80, 26, hFont);

    const char *colsP[] = {"Process", "Size"};
    int wP[] = {90, 60};
    g_hListProcs = make_listview(hwnd, IDC_LIST_PROCS,
                                 x1, y0 + 60, 260, 160, colsP, 2, wP, hFont);

    /* --- Algorithm buttons (phải trên) --- */
    int x2 = 580;
    make_static(hwnd, "ALGORITHMS", x2, y0, 260, 22, hFontBold);
    make_button(hwnd, IDC_BTN_FIRST, "Run First Fit", x2, y0 + 26, 140, 30, hFont);
    make_button(hwnd, IDC_BTN_NEXT,  "Run Next Fit",  x2 + 146, y0 + 26, 140, 30, hFont);
    make_button(hwnd, IDC_BTN_BEST,  "Run Best Fit",  x2, y0 + 60, 140, 30, hFont);
    make_button(hwnd, IDC_BTN_WORST, "Run Worst Fit", x2 + 146, y0 + 60, 140, 30, hFont);
    make_button(hwnd, IDC_BTN_COMPARE, "Compare All Algorithms",
                x2, y0 + 94, 286, 30, hFont);

    make_button(hwnd, IDC_BTN_LOAD, "Load input.txt",
                x2, y0 + 128, 140, 30, hFont);
    make_button(hwnd, IDC_BTN_SAVE, "Save result.txt",
                x2 + 146, y0 + 128, 140, 30, hFont);
    make_button(hwnd, IDC_BTN_CLEAR, "Clear All",
                x2, y0 + 162, 286, 30, hFont);

    /* --- Result table --- */
    int y1 = 300;
    make_static(hwnd, "KET QUA CAP PHAT", 20, y1, 400, 22, hFontBold);

    const char *colsR[] = {"Process", "Process Size", "Allocated Block", "Remaining", "Status"};
    int wR[] = {70, 100, 110, 90, 80};
    g_hListResult = make_listview(hwnd, IDC_LIST_RESULT,
                                  20, y1 + 26, 460, 220, colsR, 5, wR, hFont);

    g_hStats = make_static(hwnd, "", 20, y1 + 252, 460, 22, hFont);

    /* --- Comparison table --- */
    make_static(hwnd, "SO SANH THUAT TOAN", 500, y1, 400, 22, hFontBold);
    const char *colsC[] = {"Algorithm", "Success", "Failed", "Free Mem", "Max Free Frag"};
    int wC[] = {110, 80, 80, 90, 100};
    g_hListCmp = make_listview(hwnd, IDC_LIST_CMP,
                               500, y1 + 26, 460, 220, colsC, 5, wC, hFont);

    /* --- Visual memory --- */
    int y2 = 560;
    make_static(hwnd, "TRUC QUAN BO NHO", 20, y2, 400, 22, hFontBold);
    g_hVisual = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", "",
                                WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                20, y2 + 26, 940, 260,
                                hwnd, (HMENU)(INT_PTR)IDC_VISUAL, g_hInst, NULL);

    /* --- Status bar --- */
    g_hStatus = CreateWindowExA(0, "STATIC", "San sang",
                                WS_CHILD | WS_VISIBLE | SS_SUNKEN,
                                0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_STATIC_STATUS,
                                g_hInst, NULL);
    SendMessageA(g_hStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

    /* Lưu ý: Visual dùng STATIC + SS_OWNERDRAW nhưng STATIC không tự vẽ ownerdraw.
       → Chuyển sang xử lý WM_PAINT trên control con thông qua wndproc riêng. */
    (void)hFontBold; (void)t1;
}

/* WndProc riêng cho control visual để vẽ GDI */
static LRESULT CALLBACK VisualProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            /* Vẽ vào bộ đệm để tránh nháy */
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
            draw_visual(mem, &rc);
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ---------- WndProc chính ---------- */
static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            create_ui(hwnd);
            return 0;

        case WM_COMMAND: {
            int id = LOWORD(wp);
            switch (id) {
                case IDC_BTN_ADD_BLOCK: add_block_from_edit(); break;
                case IDC_BTN_DEL_BLOCK: del_selected_block(); break;
                case IDC_BTN_ADD_PROC:  add_proc_from_edit(); break;
                case IDC_BTN_DEL_PROC:  del_selected_proc(); break;
                case IDC_BTN_FIRST:     run_one_algo(0); break;
                case IDC_BTN_NEXT:      run_one_algo(1); break;
                case IDC_BTN_BEST:      run_one_algo(2); break;
                case IDC_BTN_WORST:     run_one_algo(3); break;
                case IDC_BTN_COMPARE:   run_compare(); break;
                case IDC_BTN_LOAD:      do_load_file(); break;
                case IDC_BTN_SAVE:      do_save_file(); break;
                case IDC_BTN_CLEAR:     do_clear_all(); break;
                case IDC_BTN_HELP:      do_help(); break;
            }
            return 0;
        }

        case WM_SIZE: {
            int w = LOWORD(lp), h = HIWORD(lp);
            /* Đặt status bar ở đáy */
            if (g_hStatus)
                MoveWindow(g_hStatus, 0, h - 24, w, 24, TRUE);
            /* Visual co dãn theo chiều ngang */
            if (g_hVisual)
                MoveWindow(g_hVisual, 20, 586, w - 40, 240, TRUE);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ---------- Entry point GUI ---------- */
int gui_run(HINSTANCE hInst, int nCmdShow) {
    g_hInst = hInst;

    InitCommonControls();

    /* Đăng ký class cho control visual */
    WNDCLASSA wcv = {0};
    wcv.lpfnWndProc = VisualProc;
    wcv.hInstance = hInst;
    wcv.lpszClassName = "VisualClass";
    wcv.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcv.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wcv);

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = MainProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "MemoryAllocClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "MemoryAllocClass",
        "Memory Allocation Simulator - First Fit | Next Fit | Best Fit | Worst Fit",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1020, 700,
        NULL, NULL, hInst, NULL);
    if (!hwnd) return 0;
    g_hMain = hwnd;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageA(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    return (int)m.wParam;
}