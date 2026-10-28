#ifndef FILE_IO_H
#define FILE_IO_H

#include "memory.h"

/* Đọc input.txt theo đúng định dạng:
 *   dòng 1: số block
 *   dòng 2: các kích thước block
 *   dòng 3: số process
 *   dòng 4: các kích thước process
 * Trả về 1 nếu thành công, 0 nếu lỗi (kèm thông điệp trong err).
 */
int load_input_file(const char *path,
                    int *out_block_sizes, int *out_n_blocks,
                    int *out_proc_sizes,  int *out_n_procs,
                    char *err, int err_len);

/* Ghi kết quả ra file text. */
int save_result_file(const char *path,
                     const char *algo_name,
                     const int *block_sizes, int n_blocks,
                     const int *proc_sizes,  int n_procs,
                     const Result *r,
                     char *err, int err_len);

#endif