#include "memory.h"
#include <string.h>

/* Worst Fit: chọn block có remaining lớn nhất trong các block đủ chỗ */
void run_worst_fit(const Block *init_blocks, int nb,
                   const Process *init_procs, int np, Result *out) {
    Block   blocks[MAX_BLOCKS];
    Process procs[MAX_PROCESSES];

    memcpy(blocks, init_blocks, sizeof(Block) * nb);
    memcpy(procs,  init_procs,  sizeof(Process) * np);
    for (int i = 0; i < nb; i++) {
        blocks[i].remaining  = blocks[i].size;
        blocks[i].process_id = -1;
        blocks[i].used_size  = 0;
    }
    for (int i = 0; i < np; i++) {
        procs[i].allocated_block = -1;
        procs[i].status          = 0;
    }

    for (int i = 0; i < np; i++) {
        int worst = -1;
        int worst_rem = 0;
        for (int j = 0; j < nb; j++) {
            if (blocks[j].remaining >= procs[i].size) {
                if (worst < 0 || blocks[j].remaining > worst_rem) {
                    worst = j;
                    worst_rem = blocks[j].remaining;
                }
            }
        }
        if (worst >= 0) {
            blocks[worst].remaining -= procs[i].size;
            blocks[worst].process_id = i;
            blocks[worst].used_size += procs[i].size;
            procs[i].allocated_block = worst;
            procs[i].status = 1;
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