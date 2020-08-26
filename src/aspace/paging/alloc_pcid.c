#include "alloc_pcid.h"

#define ERROR_PCID(fmt, args...) ERROR_PRINT("aspace-paging-pcid: " fmt, ##args)
#define DEBUG_PCID(fmt, args...) DEBUG_PRINT("aspace-paging-pcid: " fmt, ##args)
#define INFO_PCID(fmt, args...)   INFO_PRINT("aspace-paging-pcid: " fmt, ##args)

pcid_alloc_t allocator = {
    .head = 0,
    .tail = 0,
    .size = 0,
    .data = NULL
};


int pcid_allocator_init() {
    
    if (!allocator.data) {
        DEBUG_PCID("init allocator!\n");
        allocator.data = (uint16_t *) malloc(sizeof(uint16_t) * MAX_PCID);
        if (!allocator.data) {
            ERROR_PCID("Cannot allocate size = %d bytes for pcid allocator\n", sizeof(uint16_t) * MAX_PCID);
            return -1;
        }

        for (int i = 0; i < MAX_PCID; i ++) {
            allocator.data[i] = i;
        }
        allocator.size = MAX_PCID;
        allocator.tail = MAX_PCID;
    }
    return 0;
}

// allocate a new pcid, written to target_ptr
int alloc_pcid (ph_cr3_pcide_t * cr3) {
    int init_res = pcid_allocator_init();
    if (init_res) {
        panic("Cannot allocate size = %d bytes for pcid allocator\n", sizeof(uint16_t) * MAX_PCID);
        return -1;
    }
    
    // pop one from the queue
    if (allocator.size > 0) {
        uint16_t pcid = allocator.data[allocator.head % MAX_PCID];
        allocator.head++;
        allocator.size--;
        cr3->pcid = pcid;

        return 0;

    } else {
        ERROR_PCID("no available elements in pcid allocator!\n");
        return -1;
    }
}

// 
int free_pcid (ph_cr3_pcide_t * cr3) {
    int init_res = pcid_allocator_init();
    if (init_res) {
        panic("Cannot allocate size = %d bytes for pcid allocator\n", sizeof(uint16_t) * MAX_PCID);
        return -1;
    }

    if (allocator.size < MAX_PCID) {
        allocator.data[allocator.tail % MAX_PCID] = cr3->pcid;
        allocator.tail++;
        allocator.size++;

        return 0;
    } else {
        ERROR_PCID("no available space in pcid allocator!\n");
        return -1;
    }
}