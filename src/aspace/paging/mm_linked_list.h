#include "node_struct.h"

typedef struct mm_llist_node {
    nk_aspace_region_t region;
    struct mm_llist_node * next_llist_node;
} mm_llist_node_t;

typedef struct mm_llist {
    mm_struct_t super;
    mm_llist_node_t * region_head;
} mm_llist_t;

int mm_llist_init(mm_llist_t * llist);
