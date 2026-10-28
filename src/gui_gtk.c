#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "memory.h"
#include "file_io.h"

// Các widget toàn cục
static GtkWidget *window;
static GtkWidget *entry_block, *entry_proc;
static GtkWidget *treeview_blocks, *treeview_procs;
static GtkWidget *treeview_result, *treeview_cmp;
static GtkWidget *lbl_stats, *lbl_status;
static GtkWidget *drawing_area;

// Dữ liệu hiện tại
static int g_blockSizes[MAX_BLOCKS];
static int g_nBlocks = 0;
static int g_procSizes[MAX_PROCESSES];
static int g_nProcs = 0;

static Result g_lastResult;
static int g_hasResult = 0;
static char g_lastAlgo[64] = "";

// Các cột cho TreeView
enum { COL_BLOCK_ID, COL_BLOCK_SIZE, N_BLOCK_COLS };
enum { COL_PROC_ID, COL_PROC_SIZE, N_PROC_COLS };
enum { COL_RES_P, COL_RES_SIZE, COL_RES_BLK, COL_RES_REM, COL_RES_STATUS, N_RES_COLS };
enum { COL_CMP_ALGO, COL_CMP_SUCC, COL_CMP_FAIL, COL_CMP_FREE, COL_CMP_FRAG, N_CMP_COLS };

// ---------- Tiện ích ----------
static void set_status(const char *msg) {
    gtk_label_set_text(GTK_LABEL(lbl_status), msg);
}

static GtkWidget* create_treeview(const char *cols[], int ncols, int col_ids[]) {
    GtkListStore *store = gtk_list_store_new(ncols, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    for (int i = 0; i < ncols; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(cols[i], renderer, "text", col_ids[i], NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tv), column);
    }
    return tv;
}

// ---------- Cập nhật giao diện ----------
static void refresh_block_list() {
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_blocks)));
    gtk_list_store_clear(store);
    for (int i = 0; i < g_nBlocks; i++) {
        GtkTreeIter iter;
        char id[16], size[16];
        sprintf(id, "Block %d", i);
        sprintf(size, "%d", g_blockSizes[i]);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, id, 1, size, -1);
    }
}

static void refresh_proc_list() {
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_procs)));
    gtk_list_store_clear(store);
    for (int i = 0; i < g_nProcs; i++) {
        GtkTreeIter iter;
        char id[16], size[16];
        sprintf(id, "P%d", i + 1);
        sprintf(size, "%d", g_procSizes[i]);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, id, 1, size, -1);
    }
}

static void show_result_list(const Result *r, const char *algo) {
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_result)));
    gtk_list_store_clear(store);
    for (int i = 0; i < r->n_procs; i++) {
        const Process *p = &r->procs[i];
        GtkTreeIter iter;
        char p_name[16], p_size[16], b_name[32] = "-", rem[32] = "-";
        sprintf(p_name, "P%d", i + 1);
        sprintf(p_size, "%d", p->size);
        if (p->status) {
            sprintf(b_name, "Block %d", p->allocated_block);
            sprintf(rem, "%d", r->blocks[p->allocated_block].remaining);
        }
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, p_name, 1, p_size, 2, b_name, 3, rem, 4, p->status ? "SUCCESS" : "FAIL", -1);
    }
    char stats[256];
    sprintf(stats, "%s | Success: %d | Failed: %d | Free memory: %d", algo, r->total_allocated, r->total_failed, r->total_free_mem);
    gtk_label_set_text(GTK_LABEL(lbl_stats), stats);
    set_status("Da chay thuat toan");
}

static void show_compare_list(const Result *r[4]) {
    const char *names[4] = {"First Fit", "Next Fit", "Best Fit", "Worst Fit"};
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_cmp)));
    gtk_list_store_clear(store);
    for (int i = 0; i < 4; i++) {
        GtkTreeIter iter;
        char succ[16], fail[16], free_mem[16], frag[16];
        sprintf(succ, "%d", r[i]->total_allocated);
        sprintf(fail, "%d", r[i]->total_failed);
        sprintf(free_mem, "%d", r[i]->total_free_mem);
        int maxFrag = 0;
        for (int k = 0; k < r[i]->n_blocks; k++)
            if (r[i]->blocks[k].remaining > maxFrag) maxFrag = r[i]->blocks[k].remaining;
        sprintf(frag, "%d", maxFrag);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, names[i], 1, succ, 2, fail, 3, free_mem, 4, frag, -1);
    }
}

// Vẽ trực quan bằng Cairo (tương đương GDI bên Windows)
static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    
    // Xóa nền
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    if (!g_hasResult || g_lastResult.n_blocks == 0) {
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_move_to(cr, 20, 40);
        cairo_show_text(cr, "Chay mot thuat toan de xem truc quan bo nho...");
        return FALSE;
    }

    int padX = 20, padY = 20, labelW = 80;
    int usableW = alloc.width - padX * 2 - labelW;
    int rowH = (alloc.height - padY * 2) / g_lastResult.n_blocks;
    if (rowH > 60) rowH = 60;
    if (rowH < 22) rowH = 22;

    int maxSize = 1;
    for (int i = 0; i < g_lastResult.n_blocks; i++)
        if (g_lastResult.blocks[i].size > maxSize) maxSize = g_lastResult.blocks[i].size;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);

    for (int i = 0; i < g_lastResult.n_blocks; i++) {
        Block *b = &g_lastResult.blocks[i];
        int y = padY + i * rowH;

        // Nhãn Block i
        cairo_set_source_rgb(cr, 0, 0, 0);
        char label[32];
        sprintf(label, "Block %d", i);
        cairo_move_to(cr, padX, y + rowH/2 + 4);
        cairo_show_text(cr, label);

        int barX = padX + labelW;
        int barW = (int)((double)b->size / maxSize * usableW);
        int barY = y + 4;
        int barH = rowH - 8;

        // Phần đã cấp phát (màu xanh)
        int usedW = (int)((double)b->used_size / b->size * barW);
        if (usedW > 0) {
            cairo_set_source_rgb(cr, 0.27, 0.62, 0.94); 
            cairo_rectangle(cr, barX, barY, usedW, barH);
            cairo_fill(cr);
            if (b->process_id >= 0) {
                cairo_set_source_rgb(cr, 1, 1, 1);
                char pname[16]; sprintf(pname, "P%d", b->process_id + 1);
                cairo_move_to(cr, barX + 5, barY + barH/2 + 4);
                cairo_show_text(cr, pname);
            }
        }

        // Phần còn trống (màu xanh nhạt)
        if (b->remaining > 0) {
            cairo_set_source_rgb(cr, 0.82, 0.90, 0.82);
            cairo_rectangle(cr, barX + usedW, barY, barW - usedW, barH);
            cairo_fill(cr);
            char fname[32]; sprintf(fname, "free %d", b->remaining);
            cairo_set_source_rgb(cr, 0.1, 0.3, 0.1);
            cairo_move_to(cr, barX + usedW + 5, barY + barH/2 + 4);
            cairo_show_text(cr, fname);
        }

        // Viền
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, 1);
        cairo_rectangle(cr, barX, barY, barW, barH);
        cairo_stroke(cr);

        // Tổng size
        char sz[16]; sprintf(sz, "%d", b->size);
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
        cairo_move_to(cr, barX + barW + 6, y + rowH/2 + 4);
        cairo_show_text(cr, sz);
    }
    return FALSE;
}

// ---------- Xử lý sự kiện ----------
static void on_add_block(GtkWidget *w, gpointer d) {
    const char *txt = gtk_entry_get_text(GTK_ENTRY(entry_block));
    int v = atoi(txt);
    if (v <= 0) { set_status("Kich thuoc block phai > 0"); return; }
    if (g_nBlocks >= MAX_BLOCKS) { set_status("Da dat gioi han block"); return; }
    g_blockSizes[g_nBlocks++] = v;
    refresh_block_list();
    gtk_entry_set_text(GTK_ENTRY(entry_block), "");
}

static void on_add_proc(GtkWidget *w, gpointer d) {
    const char *txt = gtk_entry_get_text(GTK_ENTRY(entry_proc));
    int v = atoi(txt);
    if (v <= 0) { set_status("Kich thuoc process phai > 0"); return; }
    if (g_nProcs >= MAX_PROCESSES) { set_status("Da dat gioi han process"); return; }
    g_procSizes[g_nProcs++] = v;
    refresh_proc_list();
    gtk_entry_set_text(GTK_ENTRY(entry_proc), "");
}

static void on_clear_all(GtkWidget *w, gpointer d) {
    g_nBlocks = 0; g_nProcs = 0; g_hasResult = 0;
    refresh_block_list(); refresh_proc_list();
    GtkListStore *s1 = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_result)));
    GtkListStore *s2 = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_cmp)));
    gtk_list_store_clear(s1); gtk_list_store_clear(s2);
    gtk_label_set_text(GTK_LABEL(lbl_stats), "");
    gtk_widget_queue_draw(drawing_area);
    set_status("Da lam moi du lieu");
}

static int build_input(Block *blocks, Process *procs) {
    if (g_nBlocks == 0) { set_status("Chua co block nao"); return 0; }
    if (g_nProcs == 0) { set_status("Chua co process nao"); return 0; }
    for (int i = 0; i < g_nBlocks; i++) {
        blocks[i].id = i; blocks[i].size = g_blockSizes[i];
        blocks[i].remaining = g_blockSizes[i]; blocks[i].process_id = -1; blocks[i].used_size = 0;
    }
    for (int i = 0; i < g_nProcs; i++) {
        procs[i].id = i; procs[i].size = g_procSizes[i];
        procs[i].allocated_block = -1; procs[i].status = 0;
    }
    return 1;
}

static void run_one_algo(int algo) {
    Block initB[MAX_BLOCKS]; Process initP[MAX_PROCESSES];
    if (!build_input(initB, initP)) return;

    switch (algo) {
        case 0: run_first_fit(initB, g_nBlocks, initP, g_nProcs, &g_lastResult); strcpy(g_lastAlgo, "First Fit"); break;
        case 1: run_next_fit(initB, g_nBlocks, initP, g_nProcs, &g_lastResult); strcpy(g_lastAlgo, "Next Fit"); break;
        case 2: run_best_fit(initB, g_nBlocks, initP, g_nProcs, &g_lastResult); strcpy(g_lastAlgo, "Best Fit"); break;
        case 3: run_worst_fit(initB, g_nBlocks, initP, g_nProcs, &g_lastResult); strcpy(g_lastAlgo, "Worst Fit"); break;
    }
    g_hasResult = 1;
    show_result_list(&g_lastResult, g_lastAlgo);
    gtk_widget_queue_draw(drawing_area);
}

static void run_compare() {
    Block initB[MAX_BLOCKS]; Process initP[MAX_PROCESSES];
    if (!build_input(initB, initP)) return;

    static Result r0, r1, r2, r3;
    run_first_fit(initB, g_nBlocks, initP, g_nProcs, &r0);
    run_next_fit(initB, g_nBlocks, initP, g_nProcs, &r1);
    run_best_fit(initB, g_nBlocks, initP, g_nProcs, &r2);
    run_worst_fit(initB, g_nBlocks, initP, g_nProcs, &r3);

    const Result *rs[4] = {&r0, &r1, &r2, &r3};
    show_compare_list(rs);
    g_lastResult = r0; g_hasResult = 1; strcpy(g_lastAlgo, "First Fit (compare)");
    gtk_widget_queue_draw(drawing_area);
    set_status("Da so sanh 4 thuat toan");
}

// Callback cho 4 nút thuật toán
static void on_run_first(GtkWidget *w, gpointer d) { run_one_algo(0); }
static void on_run_next(GtkWidget *w, gpointer d) { run_one_algo(1); }
static void on_run_best(GtkWidget *w, gpointer d) { run_one_algo(2); }
static void on_run_worst(GtkWidget *w, gpointer d) { run_one_algo(3); }

// Xử lý Load File
static void on_load_file(GtkWidget *w, gpointer d) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Load input.txt", GTK_WINDOW(window),
        GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        int bs[MAX_BLOCKS], ps[MAX_PROCESSES], nb = 0, np = 0;
        char err[256];
        if (!load_input_file(filename, bs, &nb, ps, &np, err, sizeof err)) {
            GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", err);
            gtk_dialog_run(GTK_DIALOG(msg)); gtk_widget_destroy(msg);
        } else {
            g_nBlocks = nb; memcpy(g_blockSizes, bs, sizeof(int) * nb);
            g_nProcs = np; memcpy(g_procSizes, ps, sizeof(int) * np);
            refresh_block_list(); refresh_proc_list();
            g_hasResult = 0; gtk_widget_queue_draw(drawing_area);
            set_status("Da load input.txt thanh cong");
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

// Xử lý Save File
static void on_save_file(GtkWidget *w, gpointer d) {
    if (!g_hasResult) {
        GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Chua chay thuat toan nao de luu.");
        gtk_dialog_run(GTK_DIALOG(msg)); gtk_widget_destroy(msg);
        return;
    }
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save result.txt", GTK_WINDOW(window),
        GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "result.txt");

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char err[256];
        if (!save_result_file(filename, g_lastAlgo, g_blockSizes, g_nBlocks, g_procSizes, g_nProcs, &g_lastResult, err, sizeof err)) {
            GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", err);
            gtk_dialog_run(GTK_DIALOG(msg)); gtk_widget_destroy(msg);
        } else {
            char buf[512]; sprintf(buf, "Da luu ket qua vao:\n%s", filename);
            GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", buf);
            gtk_dialog_run(GTK_DIALOG(msg)); gtk_widget_destroy(msg);
            set_status("Da luu ket qua");
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void on_help(GtkWidget *w, gpointer d) {
    GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "HUONG DAN SU DUNG\n------------------\n"
        "1. Nhap kich thuoc Block -> Add Block.\n"
        "2. Nhap kich thuoc Process -> Add Process.\n"
        "3. Bam Run [Ten Thuat Toan] de chay.\n"
        "4. Bam Compare de so sanh ca 4.\n"
        "5. Bam Load / Save de doc/ghi file.");
    gtk_dialog_run(GTK_DIALOG(msg)); gtk_widget_destroy(msg);
}

// ---------- Hàm main GTK ----------
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Memory Allocation Simulator - GTK3");
    gtk_window_set_default_size(GTK_WINDOW(window), 1020, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_main), 10);
    gtk_container_add(GTK_CONTAINER(window), vbox_main);

    // Header
    GtkWidget *lbl_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_title), "<span size='x-large' weight='bold'>Memory Allocation Simulator</span>");
    gtk_widget_set_halign(lbl_title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_main), lbl_title, FALSE, FALSE, 0);

    // Phần 3 cột trên
    GtkWidget *hbox_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_box_pack_start(GTK_BOX(vbox_main), hbox_top, FALSE, FALSE, 0);

    // Cột Block
    GtkWidget *vbox_blk = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(hbox_top), vbox_blk, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_blk), gtk_label_new("MEMORY BLOCKS"), FALSE, FALSE, 0);
    GtkWidget *hbox_be = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    entry_block = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox_be), entry_block, TRUE, TRUE, 0);
    GtkWidget *btn_add_blk = gtk_button_new_with_label("Add Block");
    g_signal_connect(btn_add_blk, "clicked", G_CALLBACK(on_add_block), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_be), btn_add_blk, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_blk), hbox_be, FALSE, FALSE, 0);
    
    GtkWidget *scroll_blk = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scroll_blk, -1, 150);
    const char *colsB[] = {"Block", "Size"}; int idsB[] = {0, 1};
    treeview_blocks = create_treeview(colsB, 2, idsB);
    gtk_container_add(GTK_CONTAINER(scroll_blk), treeview_blocks);
    gtk_box_pack_start(GTK_BOX(vbox_blk), scroll_blk, TRUE, TRUE, 0);

    // Cột Process
    GtkWidget *vbox_prc = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(hbox_top), vbox_prc, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_prc), gtk_label_new("PROCESSES"), FALSE, FALSE, 0);
    GtkWidget *hbox_pe = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    entry_proc = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox_pe), entry_proc, TRUE, TRUE, 0);
    GtkWidget *btn_add_prc = gtk_button_new_with_label("Add Process");
    g_signal_connect(btn_add_prc, "clicked", G_CALLBACK(on_add_proc), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_pe), btn_add_prc, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_prc), hbox_pe, FALSE, FALSE, 0);

    GtkWidget *scroll_prc = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scroll_prc, -1, 150);
    const char *colsP[] = {"Process", "Size"}; int idsP[] = {0, 1};
    treeview_procs = create_treeview(colsP, 2, idsP);
    gtk_container_add(GTK_CONTAINER(scroll_prc), treeview_procs);
    gtk_box_pack_start(GTK_BOX(vbox_prc), scroll_prc, TRUE, TRUE, 0);

    // Cột Algorithm
    GtkWidget *vbox_alg = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(hbox_top), vbox_alg, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_alg), gtk_label_new("ALGORITHMS"), FALSE, FALSE, 0);
    
    GtkWidget *grid_alg = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_alg), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid_alg), 5);
    GtkWidget *b1 = gtk_button_new_with_label("Run First Fit"); g_signal_connect(b1, "clicked", G_CALLBACK(on_run_first), NULL); gtk_grid_attach(GTK_GRID(grid_alg), b1, 0, 0, 1, 1);
    GtkWidget *b2 = gtk_button_new_with_label("Run Next Fit");  g_signal_connect(b2, "clicked", G_CALLBACK(on_run_next), NULL);  gtk_grid_attach(GTK_GRID(grid_alg), b2, 1, 0, 1, 1);
    GtkWidget *b3 = gtk_button_new_with_label("Run Best Fit");  g_signal_connect(b3, "clicked", G_CALLBACK(on_run_best), NULL);  gtk_grid_attach(GTK_GRID(grid_alg), b3, 0, 1, 1, 1);
    GtkWidget *b4 = gtk_button_new_with_label("Run Worst Fit"); g_signal_connect(b4, "clicked", G_CALLBACK(on_run_worst), NULL); gtk_grid_attach(GTK_GRID(grid_alg), b4, 1, 1, 1, 1);
    GtkWidget *b5 = gtk_button_new_with_label("Compare All Algorithms"); g_signal_connect(b5, "clicked", G_CALLBACK(run_compare), NULL); gtk_grid_attach(GTK_GRID(grid_alg), b5, 0, 2, 2, 1);
    
    GtkWidget *b6 = gtk_button_new_with_label("Load input.txt"); g_signal_connect(b6, "clicked", G_CALLBACK(on_load_file), NULL); gtk_grid_attach(GTK_GRID(grid_alg), b6, 0, 3, 1, 1);
    GtkWidget *b7 = gtk_button_new_with_label("Save result.txt"); g_signal_connect(b7, "clicked", G_CALLBACK(on_save_file), NULL); gtk_grid_attach(GTK_GRID(grid_alg), b7, 1, 3, 1, 1);
    GtkWidget *b8 = gtk_button_new_with_label("Clear All"); g_signal_connect(b8, "clicked", G_CALLBACK(on_clear_all), NULL); gtk_grid_attach(GTK_GRID(grid_alg), b8, 0, 4, 2, 1);
    
    GtkWidget *b9 = gtk_button_new_with_label("Huong dan"); g_signal_connect(b9, "clicked", G_CALLBACK(on_help), NULL); gtk_grid_attach(GTK_GRID(grid_alg), b9, 0, 5, 2, 1);
    gtk_box_pack_start(GTK_BOX(vbox_alg), grid_alg, FALSE, FALSE, 0);

    // Phần 2 bảng giữa
    GtkWidget *hbox_mid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_box_pack_start(GTK_BOX(vbox_main), hbox_mid, TRUE, TRUE, 0);

    GtkWidget *vbox_res = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(hbox_mid), vbox_res, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_res), gtk_label_new("KET QUA CAP PHAT"), FALSE, FALSE, 0);
    
    GtkWidget *scroll_res = gtk_scrolled_window_new(NULL, NULL);
    const char *colsR[] = {"Process", "Process Size", "Allocated Block", "Remaining", "Status"}; int idsR[] = {0,1,2,3,4};
    treeview_result = create_treeview(colsR, 5, idsR);
    gtk_container_add(GTK_CONTAINER(scroll_res), treeview_result);
    gtk_box_pack_start(GTK_BOX(vbox_res), scroll_res, TRUE, TRUE, 0);
    lbl_stats = gtk_label_new("");
    gtk_widget_set_halign(lbl_stats, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_res), lbl_stats, FALSE, FALSE, 0);

    GtkWidget *vbox_cmp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(hbox_mid), vbox_cmp, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_cmp), gtk_label_new("SO SANH THUAT TOAN"), FALSE, FALSE, 0);
    
    GtkWidget *scroll_cmp = gtk_scrolled_window_new(NULL, NULL);
    const char *colsC[] = {"Algorithm", "Success", "Failed", "Free Mem", "Max Free Frag"}; int idsC[] = {0,1,2,3,4};
    treeview_cmp = create_treeview(colsC, 5, idsC);
    gtk_container_add(GTK_CONTAINER(scroll_cmp), treeview_cmp);
    gtk_box_pack_start(GTK_BOX(vbox_cmp), scroll_cmp, TRUE, TRUE, 0);

    // Phần trực quan
    GtkWidget *lbl_vis = gtk_label_new("TRUC QUAN BO NHO");
    gtk_widget_set_halign(lbl_vis, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_main), lbl_vis, FALSE, FALSE, 0);
    
    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, -1, 180);
    g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(on_draw), NULL);
    GtkWidget *frame_vis = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(frame_vis), drawing_area);
    gtk_box_pack_start(GTK_BOX(vbox_main), frame_vis, FALSE, FALSE, 0);

    // Status bar
    lbl_status = gtk_label_new("San sang");
    gtk_widget_set_halign(lbl_status, GTK_ALIGN_START);
    gtk_box_pack_end(GTK_BOX(vbox_main), lbl_status, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}