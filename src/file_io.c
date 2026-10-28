#include "file_io.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Kiểm tra chuỗi có phải số nguyên không âm */
static int is_positive_int(const char *s) {
    if (!*s) return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

int load_input_file(const char *path,
                    int *out_block_sizes, int *out_n_blocks,
                    int *out_proc_sizes,  int *out_n_procs,
                    char *err, int err_len) {
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(err, err_len, "Khong mo duoc file: %s", path);
        return 0;
    }

    int nb = 0, np = 0;
    if (fscanf(f, "%d", &nb) != 1 || nb <= 0 || nb > MAX_BLOCKS) {
        snprintf(err, err_len, "So luong block khong hop le (1..%d)", MAX_BLOCKS);
        fclose(f); return 0;
    }
    for (int i = 0; i < nb; i++) {
        if (fscanf(f, "%d", &out_block_sizes[i]) != 1 || out_block_sizes[i] <= 0) {
            snprintf(err, err_len, "Kich thuoc block #%d khong hop le", i + 1);
            fclose(f); return 0;
        }
    }
    if (fscanf(f, "%d", &np) != 1 || np <= 0 || np > MAX_PROCESSES) {
        snprintf(err, err_len, "So luong process khong hop le (1..%d)", MAX_PROCESSES);
        fclose(f); return 0;
    }
    for (int i = 0; i < np; i++) {
        if (fscanf(f, "%d", &out_proc_sizes[i]) != 1 || out_proc_sizes[i] <= 0) {
            snprintf(err, err_len, "Kich thuoc process #%d khong hop le", i + 1);
            fclose(f); return 0;
        }
    }
    fclose(f);
    *out_n_blocks = nb;
    *out_n_procs  = np;
    return 1;
}

int save_result_file(const char *path,
                     const char *algo_name,
                     const int *block_sizes, int n_blocks,
                     const int *proc_sizes,  int n_procs,
                     const Result *r,
                     char *err, int err_len) {
    FILE *f = fopen(path, "w");
    if (!f) {
        snprintf(err, err_len, "Khong tao duoc file: %s", path);
        return 0;
    }

    fprintf(f, "===== MEMORY ALLOCATION RESULT =====\n");
    fprintf(f, "Thuat toan: %s\n\n", algo_name);

    fprintf(f, "-- INPUT --\n");
    fprintf(f, "So block: %d\n", n_blocks);
    fprintf(f, "Kich thuoc block: ");
    for (int i = 0; i < n_blocks; i++) fprintf(f, "%d ", block_sizes[i]);
    fprintf(f, "\nSo process: %d\n", n_procs);
    fprintf(f, "Kich thuoc process: ");
    for (int i = 0; i < n_procs; i++) fprintf(f, "%d ", proc_sizes[i]);
    fprintf(f, "\n\n");

    fprintf(f, "-- KET QUA CAP PHAT --\n");
    fprintf(f, "%-10s %-14s %-18s %-16s %-10s\n",
            "Process", "Process Size", "Allocated Block",
            "Remaining Size", "Status");
    for (int i = 0; i < n_procs; i++) {
        const Process *p = &r->procs[i];
        if (p->status) {
            fprintf(f, "P%-9d %-14d Block %-12d %-16d %-10s\n",
                    i + 1, p->size, p->allocated_block,
                    r->blocks[p->allocated_block].remaining, "SUCCESS");
        } else {
            fprintf(f, "P%-9d %-14d %-18s %-16s %-10s\n",
                    i + 1, p->size, "-", "-", "FAIL");
        }
    }
    fprintf(f, "\n-- THONG KE --\n");
    fprintf(f, "Tong process cap phat thanh cong : %d\n", r->total_allocated);
    fprintf(f, "Tong process that bai            : %d\n", r->total_failed);
    fprintf(f, "Tong bo nho con lai              : %d\n", r->total_free_mem);

    fprintf(f, "\n-- TRANG THAI CAC BLOCK --\n");
    for (int i = 0; i < n_blocks; i++) {
        const Block *b = &r->blocks[i];
        fprintf(f, "Block %d: size=%d, con lai=%d",
                i, b->size, b->remaining);
        if (b->process_id >= 0)
            fprintf(f, " (P%d chiem %d)", b->process_id + 1, b->used_size);
        fprintf(f, "\n");
    }

    fclose(f);
    return 1;
}