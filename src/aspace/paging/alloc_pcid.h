#include <nautilus/nautilus.h>
#include "paging_helpers.h"
// 12 bits for PCID in cr3
#define MAX_PCID 4096

typedef struct alloc
{
    uint64_t head;  // head of the queue
    uint64_t tail;  // tail of the queue
    uint16_t size;  // size of the queue < MAX_PCID, so uint16_t enough
    uint16_t *data; // data ptr 
} pcid_alloc_t;

int alloc_pcid (ph_cr3_pcide_t * cr3);

int free_pcid (ph_cr3_pcide_t * cr3);
