#include "memory.h"
#include <string.h>

/* Next Fit: giống First Fit nhưng con trỏ tìm kiếm tiếp tục từ vị trí vừa cấp phát,
   quay vòng khi hết mảng. */
void run_next_fit(const Block *init_blocks, int nb,
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

    int cursor = 0;  /* con trỏ Next Fit */
    for (int i = 0; i < np; i++) {
        int found = -1;
        /* Duyệt vòng quanh nb block bắt đầu từ cursor */
        for (int k = 0; k < nb; k++) {
            int j = (cursor + k) % nb;
            if (blocks[j].remaining >= procs[i].size) {
                found = j;
                break;
            }
        }
        if (found >= 0) {
            blocks[found].remaining -= procs[i].size;
            blocks[found].process_id = i;
            blocks[found].used_size += procs[i].size;
            procs[i].allocated_block = found;
            procs[i].status = 1;
            cursor = (found + 1) % nb;   /* tiếp tục từ block kế tiếp */
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