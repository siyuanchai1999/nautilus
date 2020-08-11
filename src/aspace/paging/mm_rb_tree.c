#include "mm_rb_tree.h"
#include <nautilus/nautilus.h>

#define ERROR(fmt, args...) ERROR_PRINT("aspace-paging-rbtree: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("aspace-paging-rbtree: " fmt, ##args)
#define INFO(fmt, args...)   INFO_PRINT("aspace-paging-rbtree: " fmt, ##args)
/*
         |                    |   
         x                    y
       /  \                 /  \
      a    y      ===>     x    c
         /  \            /  \  
        b    c          a    b
*/
void left_rotate(mm_rb_tree_t * tree, mm_rb_node_t *x) {
    mm_rb_node_t * y = x->right;
    
    // transfer b from y to x
    x->right = y->left;
    if (y->left != tree->NIL) {
        y->left->parent = x;
    }

    // link y to x's parent
    y->parent = x->parent;
    if (x->parent == tree->NIL) {
        tree->root = y;
    } else {
        if(x == x->parent->left) {
            x->parent->left = y;
        } else {
            x->parent->right = y;
        }
    }

    // link x and y as left child and parent 
    y->left = x;
    x->parent = y;

}

/*
         |                    |
         x                    y
       /  \                 /  \
      y    a      ===>     b    x
    /  \                      /  \  
   b    c                    c    a
*/
void right_rotate(mm_rb_tree_t * tree, mm_rb_node_t *x) {
    mm_rb_node_t * y = x->left;
    
    // transfer c from y to x
    x->left = y->right;
    if (y->right != tree->NIL) {
        y->right->parent = x;
    }

    // link y to x's parent
    y->parent = x->parent;
    if (x->parent == tree->NIL) {
        tree->root = y;
    } else {
        if(x == x->parent->left) {
            x->parent->left = y;
        } else {
            x->parent->right = y;
        }
    }

    // link x and y as left child and parent 
    y->right = x;
    x->parent = y;
}

void rb_tree_insert_fixup(mm_rb_tree_t * tree, mm_rb_node_t * z){
    mm_rb_node_t * y;
    char buf[NODE_STR_DETAIL_LEN];

    while (z->parent->color == RED) {
        // node2str_detail(tree, z, buf);
        // printf("%s\n", buf);
        if (z->parent == z->parent->parent->left) {
            y = z->parent->parent->right;
            if (y->color == RED) {
                //case 1 pop red node up
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                y->color = BLACK;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    // case 2, converts to case 3
                    z = z->parent;
                    left_rotate(tree, z);
                }
                // case 3 solve the red conflict
                
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                right_rotate(tree, z->parent->parent);
            
            }
        } else {
            y = z->parent->parent->left;
            if (y->color == RED) {
                //case 1 pop red node up
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                y->color = BLACK;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    // case 2, converts to case 3
                    z = z->parent;
                    right_rotate(tree, z);
                }
                // case 3 solve the red conflict
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                left_rotate(tree, z->parent->parent);
            }
        }
    }
    tree->root->color = BLACK;
}

int rb_tree_insert(mm_struct_t * self, nk_aspace_region_t * region) {
    mm_rb_tree_t * tree = (mm_rb_tree_t * ) self;

    mm_rb_node_t * curr = tree->root;
    mm_rb_node_t * parent = tree->NIL;

    mm_rb_node_t * wrapper = (mm_rb_node_t *) malloc(sizeof(mm_rb_node_t));
    if (!wrapper){
        ERROR("cannot allocate a node for linked list data structure to track region mapping\n");
        return -1;
    }
    wrapper->region = *region;
    
    while (curr != tree->NIL) {
        int comp = (*tree->compf)(wrapper, curr);
        parent = curr;
        if (comp < 0) {
            curr = curr->left;
        } else {
            curr = curr->right;
        }
        
    }
    wrapper->parent = parent;
    
    if (parent == tree->NIL) {
        tree->root = wrapper;
    } else {
        int comp = (*tree->compf)(wrapper, parent);
        if (comp < 0) {
            parent->left = wrapper;
        } else {
            parent->right = wrapper;
        }
    }

    wrapper->left = tree->NIL;
    wrapper->right = tree->NIL;
    wrapper->color = RED;
    rb_tree_insert_fixup(tree, wrapper);

    tree->super.size = tree->super.size + 1;

    return 0;
}

mm_rb_node_t * rb_tree_search(mm_rb_tree_t * tree, mm_rb_node_t * node) {
    // mm_rb_tree_t * tree = (mm_rb_tree_t * ) self;

    mm_rb_node_t * curr = tree->root;
    
    while (curr != tree->NIL) {
        int comp = (*tree->compf)(node, curr);
        if (comp < 0) {
            curr = curr->left;
        } else if (comp > 0) {
            curr = curr->right;
        } else {
            return curr;
        }        
    }

    return tree->NIL;
}

/*
    return tree->NIL if tree is empty
*/
mm_rb_node_t * rb_tree_minimum(mm_rb_tree_t * tree, mm_rb_node_t * node) {
    mm_rb_node_t * curr = node;
    mm_rb_node_t * parent = curr;
    while (curr != tree->NIL) {
        parent = curr;
        curr = curr->left;
    }
    
    return parent;
}

/*
    return tree->NIL if tree is empty
*/
mm_rb_node_t * rb_tree_maximum(mm_rb_tree_t * tree, mm_rb_node_t * node) {
    mm_rb_node_t * curr = tree->root;
    mm_rb_node_t * parent = tree->NIL;
    while (curr != tree->NIL) {
        parent = curr;
        curr = curr->right;
    }
    
    return parent;
}

void mm_rb_tree_transplant(mm_rb_tree_t * tree, mm_rb_node_t * u, mm_rb_node_t * v) {
    if (u->parent == tree->NIL) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }

    v->parent = u->parent;
}

void rb_tree_delete_fixup(mm_rb_tree_t * tree, mm_rb_node_t * x){
    mm_rb_node_t * w;
    while (x != tree->root && x->color == BLACK) {
        // char buf[NODE_STR_DETAIL_LEN];
        // node2str_detail(tree, x, buf);
        // printf("extra blackness at %s\n", buf);
        if (x == x->parent->left){
            w = x->parent->right;

            // node2str_detail(tree, w, buf);
            // printf("w = %s\n", buf);

            if (w->color == RED) {
                /*
                    case 1: brother red
                    converts to cases 2, 3, 4
                      (p, B)                    (w, B)
                       /  \                      /  \
                  (x, B)  (w, R)    ==>     (p, R)  (b, B)
                         /    \             /   \
                       (a, B) (b, B)    (x, B) (a, B) 
                */
                w->color = BLACK;
                x->parent->color = RED;
                left_rotate(tree, x->parent);
                w = x->parent->right;
            }
            // now we have brother black. 
        
            if (w->left->color == BLACK && w->right->color == BLACK) {
                /*
                    case 2: w black and both children of w black
                    move the extra blackness up.
                    Note that if we enter case 2 from case 1, x->parent->color == RED, so loop terminates there 
                        (p, U)                    (p, U)  new x
                        /  \                      /  \
                    (x, B)  (w, B)    ==>     (x, B)  (w, R)
                            /    \                    /   \
                        (a, B) (b, B)             (a, B) (b, B) 
                */
                w->color = RED;
                x = x->parent;
            } else {
                if (w->left->color == RED && w->right->color == BLACK) {
                    /*
                        case 3: w black and left child RED but right child BLACK
                        we convet to case 4
                            (p, U)                    (p, U)  
                            /  \                      /  \
                        (x, B)  (w, B)    ==>     (x, B)  (a, B) new w
                                /    \                       \
                            (a, R) (b, B)                  (w, R)
                                                               \
                                                             (b, B) 
                    */
                    w->left->color = BLACK;
                    w->color = RED;
                    right_rotate(tree, w);
                    w = x->parent->right;
                }
                /*
                    case 4: w black and right child RED
                    left rotate the tree and resolve the conflict
                        (p, U)                          (w, U)
                        /  \                            /    \
                    (x, B)  (a, B) new w    ==>     (p, B)  (d, B)
                                \                     /        
                                (d, R)            (x, B)       
                */        
                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->right->color = BLACK;
                left_rotate(tree, x->parent);
                x = tree->root;
            }

        } else {
            /*
                symmetric to the cases elaborated above,
                change left to right, right to left, 
                and left_rotate to righ_rotate and right_rotate to left_rotate
            */

            w = x->parent->left;

            // node2str_detail(tree, w, buf);
            // printf("w = %s\n", buf);

            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                right_rotate(tree, x->parent);
                w = x->parent->left;
            }

            if (w->left->color == BLACK && w->right->color == BLACK) {
                w->color = RED;
                x = x->parent;
            } else {
                if (w->left->color == BLACK) {
                    w->right->color = BLACK;
                    w->color = RED;
                    left_rotate(tree, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->left->color = BLACK;
                right_rotate(tree, x->parent);
                x = tree->root;
            }
        }

        // rb_tree_level_order(tree);
    }
    x->color = BLACK;
}

void rb_tree_delete_node(mm_rb_tree_t * tree, mm_rb_node_t * z) {
    if (z == tree->NIL || z == NULL) {
        return;
    }

    mm_rb_node_t * y, * x;
    enum rb_tree_node_color original_color;
    if (z->left == tree->NIL || z->right == tree->NIL) {
        original_color = z->color;
        if (z->left == tree->NIL) {
            x = z->right;
            mm_rb_tree_transplant(tree, z, z->right);
        }else {
            x = z->left;
            mm_rb_tree_transplant(tree, z, z->left);
        }
        
    } else {
        /*
            In this case, z, the node we try to move, has two non trivial children.
            y would be the node to replace z, and x would be the node to replace y. 
        */
        y = rb_tree_minimum(tree, z->right);
        original_color = y->color;
        
        /*
            Because we are using the minimum within the subtree under z->right as y, 
            y-left must be tree->NIL.
        */

        x = y->right;
        if (y->parent == z) {
            x->parent = y;
        } else {
            mm_rb_tree_transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }

        mm_rb_tree_transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    // If y is originally a black one, we need to call the fixup function to fix the extra blackness. 
    // printf("try to fixup\n");
    if (original_color == BLACK) rb_tree_delete_fixup(tree, x);
    free(z);

    tree->super.size = tree->super.size - 1;
}

void rb_tree_inorder_helper(mm_rb_tree_t * tree, mm_rb_node_t * curr) {
    if (curr != tree->NIL) {
        rb_tree_inorder_helper(tree, curr->left);
        DEBUG("(VA = 0x%016lx to PA = 0x%016lx, len = %lx, prot=%lx)\n", 
            curr->region.va_start,
            curr->region.pa_start,
            curr->region.len_bytes,
            curr->region.protect.flags
        );
        rb_tree_inorder_helper(tree, curr->right);
    }
}


void rb_tree_inorder(mm_struct_t * self){
    mm_rb_tree_t * tree = (mm_rb_tree_t * ) self;
    DEBUG("Displaying the tree at %p with inorder (left, root, right)"
            "expect ascending order\n",
            tree
        );
    rb_tree_inorder_helper(tree, tree->root);
    DEBUG("\n");
}

/*
    dir = 1 select min
    dir = -1 select max
*/
mm_rb_node_t * new_bound(
    mm_rb_tree_t * tree, 
    mm_rb_node_t * prev_upper, 
    mm_rb_node_t * curr_upper,
    int dir
) {
    if (prev_upper == tree->NIL) {
        return curr_upper;
    }

    int comp = (*tree->compf) (prev_upper, curr_upper);
    if (comp * dir <= 0) {
        return prev_upper;
    } else {
        return curr_upper;
    }
}


/*
    find lowest upper bound for key among the elements in tree
    return tree->NIL if key is larger than all elements in the tree
    AKA, no upper bound
*/
mm_rb_node_t * rb_tree_LUB(mm_rb_tree_t * tree, mm_rb_node_t * node) {
    mm_rb_node_t * curr = tree->root;
    mm_rb_node_t * upper_bound = tree->NIL;


    while (curr != tree->NIL) {
        int comp = (*tree->compf) (node, curr);
        if (comp == 0){
            upper_bound = curr;
            break;
        } else if (comp < 0) {
            upper_bound = new_bound(tree, upper_bound, curr, 1);
            curr = curr->left;
        } else {
            curr = curr->right;
        }
    }

    return upper_bound;
}

/*
    find greatest lower bound for key among the elements in tree
    return tree->NIL if key is larger than all elements in the tree
    AKA, no upper bound
*/
mm_rb_node_t * rb_tree_GLB(mm_rb_tree_t * tree, mm_rb_node_t * node) {
    mm_rb_node_t * curr = tree->root;
    mm_rb_node_t * lower_bound = tree->NIL;

    while (curr != tree->NIL) {
        int comp = (*tree->compf) (node, curr);
        if (comp == 0){
            lower_bound = curr;
            break;
        } else if (comp > 0) {
            lower_bound = new_bound(tree, lower_bound, curr, -1);
            curr = curr->right;
        } else {

            curr = curr->left;
        }
    }

    return lower_bound;
}

nk_aspace_region_t * mm_rb_tree_check_overlap(mm_struct_t * self, nk_aspace_region_t * region) {
    mm_rb_tree_t * tree = (mm_rb_tree_t *) self;

    mm_rb_node_t wrapper;
    wrapper.region = * region;

    mm_rb_node_t * GLB = rb_tree_GLB(tree, &wrapper);
    mm_rb_node_t * LUB = rb_tree_LUB(tree, &wrapper);

    if (LUB == NULL ) {
        panic("get lowest upperbound fails!\n");
        return NULL;
    }

    if (LUB == NULL ) {
        panic("get greatest lower bound fails!\n");
        return NULL;
    }

    if (GLB != tree->NIL) {
        nk_aspace_region_t * curr_region_ptr = &GLB->region;
        if (overlap_helper(curr_region_ptr, region)) {
            return curr_region_ptr;
        }
    } 

    if (LUB != tree->NIL) {
        nk_aspace_region_t * curr_region_ptr = &LUB->region;
        if (overlap_helper(curr_region_ptr, region)) {
            return curr_region_ptr;
        }
    }

    return NULL;
}

nk_aspace_region_t * rb_tree_find_reg_at_addr(mm_struct_t * self, addr_t address) {
    mm_rb_tree_t * tree = (mm_rb_tree_t *) self;

    mm_rb_node_t node;
    node.region.va_start = (void *) address;
    node.region.len_bytes = 0;

    mm_rb_node_t * GLB = rb_tree_GLB(tree, &node);

    if (GLB == tree->NIL) return NULL;
    // DEBUG("(VA = 0x%016lx to PA = 0x%016lx, len = %lx, prot=%lx)\n", 
    //         GLB->region.va_start,
    //         GLB->region.pa_start,
    //         GLB->region.len_bytes,
    //         GLB->region.protect.flags
    //     );
    nk_aspace_region_t * curr_region_ptr = &GLB->region;
    if (overlap_helper(curr_region_ptr, &node.region)) {
        return curr_region_ptr;
    }

    return NULL;
    
}

nk_aspace_region_t * rb_tree_update_region (
    mm_struct_t * self, 
    nk_aspace_region_t * cur_region, 
    nk_aspace_region_t * new_region, 
    uint8_t eq_flag
) {
    mm_rb_tree_t * tree = (mm_rb_tree_t *) self;

    if (!(eq_flag & VA_CHECK)) {
        ERROR("rb tree expect to update regions with VA as the same!\n");
        return NULL;
    }

    if (!(region_equal(cur_region, new_region, eq_flag))){
        ERROR("rb tree expect to have the input region equal in terms of eq_flag!\n");
        return NULL;
    }

    mm_rb_node_t node;
    node.region = *cur_region;

    mm_rb_node_t * target_node = rb_tree_search(tree, &node);
    
    // region not found
    if (target_node == tree->NIL) return NULL;
    
    // criterion not met
    nk_aspace_region_t * curr_region_ptr = &target_node->region;
    int eq = region_equal(curr_region_ptr, cur_region, eq_flag);
    if (!eq) return NULL;

    region_update(curr_region_ptr, new_region, eq_flag);

    return curr_region_ptr;
}

int rb_tree_remove(mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags) {
    mm_rb_tree_t * tree = (mm_rb_tree_t *) self;

    if (!(check_flags & VA_CHECK)) {
        ERROR("rb tree expect to remove regions with VA_check flag set!\n");
        return 0;
    }
    mm_rb_node_t node;
    node.region = *region;
    
    mm_rb_node_t * target = rb_tree_search(tree, &node);
    
    // region not found

    if (target == tree->NIL) return 0;
    // criterion not met
    if (!region_equal(&target->region, region, check_flags)) return 0;

    rb_tree_delete_node(tree,target);

    return 1;
}

nk_aspace_region_t* rb_tree_contains(
    mm_struct_t * self, 
    nk_aspace_region_t * region, 
    uint8_t check_flags
) {
    mm_rb_tree_t * tree = (mm_rb_tree_t *) self;

    if (!(check_flags & VA_CHECK)) {
        ERROR("rb tree expect to search regions with VA_check flag set!\n");
        return NULL;
    }

    mm_rb_node_t node;
    node.region = *region;

    mm_rb_node_t * target = rb_tree_search(tree, &node);
    
    // region not found
    if (target == tree->NIL) return NULL;
    // criterion not met
    if (!region_equal(&target->region, region, check_flags)) return NULL;

    return &target->region;
}

int rb_comp_region(mm_rb_node_t * n1, mm_rb_node_t * n2) {
    if (n1->region.va_start < n2->region.va_start) {
        return -1;
    } else if (n1->region.va_start > n2->region.va_start) {
        return 1;
    } else {
        return 0;
    }
}

mm_rb_node_t * create_rb_NIL() {
    mm_rb_node_t * nil = (mm_rb_node_t *) malloc(sizeof(mm_rb_node_t));
    if (!nil) {
        panic("Fail to create rb NIL node!\n");
        return NULL;
    }

    nil->color = BLACK;
    nil->parent = nil->left = nil->right = NULL;
    nk_aspace_region_t reg_default;
    nil->region = reg_default;
    return nil;
}

mm_struct_t * mm_rb_tree_create() {
    mm_rb_tree_t * rbtree = (mm_rb_tree_t *) malloc(sizeof(mm_rb_tree_t));
    if (!rbtree) {
        ERROR("Fail to create rb tree!\n");
        return NULL;
    }

    mm_struct_init(&rbtree->super);
    
    rbtree->super.vptr->insert = &rb_tree_insert;
    rbtree->super.vptr->show = &rb_tree_inorder;
    rbtree->super.vptr->check_overlap = &mm_rb_tree_check_overlap;
    rbtree->super.vptr->find_reg_at_addr = &rb_tree_find_reg_at_addr;
    rbtree->super.vptr->update_region = &rb_tree_update_region;
    rbtree->super.vptr->remove = &rb_tree_remove;
    rbtree->super.vptr->contains = &rb_tree_contains;
    
    rbtree->NIL = create_rb_NIL();
    rbtree->root = rbtree->NIL;
    rbtree->compf = &rb_comp_region;

    return &rbtree->super;
}