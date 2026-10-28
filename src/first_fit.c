#include "memory.h"
#include <string.h>

void run_first_fit(const Block *init_blocks, int nb,
                   const Process *init_procs, int np, Result *out) {
    Block   blocks[MAX_BLOCKS];
    Process procs[MAX_PROCESSES];

    /* Deep-copy để không làm hỏng dữ liệu gốc của người dùng */
    memcpy(blocks, init_blocks, sizeof(Block) * nb);
    memcpy(procs,  init_procs,  sizeof(Process) * np);

    /* Reset trạng thái cấp phát (dữ liệu đầu vào là block "sạch") */
    for (int i = 0; i < nb; i++) {
        blocks[i].remaining  = blocks[i].size;
        blocks[i].process_id = -1;
        blocks[i].used_size  = 0;
    }
    for (int i = 0; i < np; i++) {
        procs[i].allocated_block = -1;
        procs[i].status          = 0;
    }

    /* First Fit: duyệt từ block 0 → nb-1 cho từng process */
    for (int i = 0; i < np; i++) {
        for (int j = 0; j < nb; j++) {
            if (blocks[j].remaining >= procs[i].size) {
                blocks[j].remaining -= procs[i].size;
                blocks[j].process_id = i;
                blocks[j].used_size += procs[i].size;
                procs[i].allocated_block = j;
                procs[i].status = 1;
                break;
            }
        }
    }

    memcpy(out->blocks, blocks, sizeof(Block) * nb);
    memcpy(out->procs,  procs,  sizeof(Process) * np);
    out->n_blocks = nb;
    out->n_procs  = np;

    out->total_allocated = 0;
    out->total_failed    = 0;
    for (int i = 0; i < np; i++) {
        if (procs[i].status) out->total_allocated++;
        else                 out->total_failed++;
    }
    out->total_free_mem = compute_total_free(blocks, nb);
}