#include "memory.h"
#include <string.h>

void reset_blocks(Block *blocks, int n_blocks, const int *orig_sizes) {
    for (int i = 0; i < n_blocks; i++) {
        blocks[i].id          = i;
        blocks[i].size        = orig_sizes[i];
        blocks[i].remaining   = orig_sizes[i];
        blocks[i].process_id  = -1;
        blocks[i].used_size   = 0;
    }
}

void copy_input(const Block *src_b, int nb, const Process *src_p, int np,
                Block *dst_b, Process *dst_p) {
    memcpy(dst_b, src_b, sizeof(Block) * nb);
    memcpy(dst_p, src_p, sizeof(Process) * np);
}

int compute_total_free(const Block *blocks, int n_blocks) {
    int s = 0;
    for (int i = 0; i < n_blocks; i++) s += blocks[i].remaining;
    return s;
}