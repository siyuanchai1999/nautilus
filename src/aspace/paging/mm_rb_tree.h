#include "node_struct.h"



#define NUM2COLOR(n) (((n) == BLACK) ? 'B' : 'R')
#define NODE_STR_LEN 25
#define NODE_STR_DETAIL_LEN (NODE_STR_LEN * 4)

enum rb_tree_node_color {
    BLACK,
    RED
};

typedef struct mm_rb_node {
    enum rb_tree_node_color color;
    nk_aspace_region_t region;
    struct mm_rb_node * parent;
    struct mm_rb_node * left;
    struct mm_rb_node * right;
} mm_rb_node_t;

typedef struct rb_tree
{
    mm_struct_t super;
    mm_rb_node_t * NIL;
    mm_rb_node_t * root;
    
    int (*compf)(mm_rb_node_t * n1, mm_rb_node_t * n2);
} mm_rb_tree_t;

mm_struct_t * mm_rb_tree_create();