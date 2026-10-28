#ifndef MEMORY_H
#define MEMORY_H

#define MAX_BLOCKS    100
#define MAX_PROCESSES 100
#define NAME_LEN      32

/* Mô hình: PHÂN VÙNG ĐỘNG - block bị chia nhỏ sau mỗi lần cấp phát */
typedef struct {
    int  id;              /* chỉ số block (0-based) */
    int  size;            /* kích thước ban đầu */
    int  remaining;       /* dung lượng còn lại */
    int  process_id;      /* -1 nếu trống, ngược lại là chỉ số process đang chiếm phần đầu block */
    int  used_size;       /* kích thước đã bị process chiếm (tại đầu block) */
} Block;

typedef struct {
    int id;
    int size;
    int allocated_block;  /* -1 nếu chưa được cấp */
    int status;           /* 1 = success, 0 = fail */
} Process;

typedef struct {
    Process procs[MAX_PROCESSES];
    int     n_procs;
    Block   blocks[MAX_BLOCKS];
    int     n_blocks;

    int     total_allocated;  /* số process được cấp */
    int     total_failed;     /* số process thất bại */
    int     total_free_mem;   /* tổng dung lượng còn lại của tất cả block */
} Result;

/* Reset trạng thái block về ban đầu (remaining = size, process_id = -1) */
void reset_blocks(Block *blocks, int n_blocks, const int *orig_sizes);

/* Deep-copy dữ liệu đầu vào để mỗi thuật toán chạy trên bản sao riêng */
void copy_input(const Block *src_b, int nb, const Process *src_p, int np,
                Block *dst_b, Process *dst_p);

/* Tổng dung lượng còn lại (dùng để thống kê) */
int compute_total_free(const Block *blocks, int n_blocks);

/* Bốn thuật toán - tất cả dùng chung interface */
void run_first_fit (const Block *init_blocks, int nb,
                    const Process *init_procs, int np, Result *out);

void run_next_fit  (const Block *init_blocks, int nb,
                    const Process *init_procs, int np, Result *out);

void run_best_fit  (const Block *init_blocks, int nb,
                    const Process *init_procs, int np, Result *out);

void run_worst_fit (const Block *init_blocks, int nb,
                    const Process *init_procs, int np, Result *out);

#endif