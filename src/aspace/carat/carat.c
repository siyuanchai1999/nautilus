/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2019, Brian Suchy
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Brian Suchy
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>

#include <nautilus/aspace.h>

// PATH need to be fixed

#include "patching.h"
// from Brian
#include <nautilus/list.h>
#ifdef NAUT_CONFIG_ASPACE_PAGING_REGION_RB_TREE
#include "../paging/mm_rb_tree.h"

#elif defined NAUT_CONFIG_ASPACE_PAGING_REGION_SPLAY_TREE
#include "../paging/mm_splay_tree.h"

#elif defined NAUT_CONFIG_ASPACE_PAGING_REGION_LINKED_LIST
#include "../paging/mm_linked_list.h"

#else
#include "node_struct.h"

#endif



#ifndef NAUT_CONFIG_DEBUG_ASPACE_CARAT
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("aspace-carat: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("aspace-carat: " fmt, ##args)
#define INFO(fmt, args...)   INFO_PRINT("aspace-carat: " fmt, ##args)

#define ASPACE_LOCK_CONF uint8_t _aspace_lock_flags
// Peter fix a bug with this line, TODO: change in paging.c
#define ASPACE_LOCK(a) _aspace_lock_flags = spin_lock_irq_save(&((a))->lock)
#define ASPACE_TRY_LOCK(a) spin_try_lock_irq_save(&((a)->lock),&_aspace_lock_flags)
#define ASPACE_UNLOCK(a) spin_unlock_irq_restore(&((a)->lock), _aspace_lock_flags)
#define ASPACE_UNIRQ(a) irq_enable_restore(_aspace_lock_flags);


#define ASPACE_NAME(a) ((a)?(a)->aspace->name : "default")
#define THREAD_NAME(t) ((!(t)) ? "(none)" : (t)->is_idle ? "(idle)" : (t)->name[0] ? (t)->name : "(noname)")

#ifndef NAUT_CONFIG_DEBUG_ASPACE_CARAT
#define REGION_FORMAT ""
#define REGION(r)
#else
#define REGION_FORMAT "(VA=0x%p to PA=0x%p, len=%lx, prot=%lx)"
#define REGION(r) (r)->va_start, (r)->pa_start, (r)->len_bytes, (r)->protect.flags
#endif


// ok if r1.permision <= r2.permission
#define PERMISSION_LEQ(r1, r2) \
    (\
        NK_ASPACE_GET_READ(r1->protect.flags)  <= NK_ASPACE_GET_READ(r2->protect.flags) \
    &&  NK_ASPACE_GET_WRITE(r1->protect.flags) <= NK_ASPACE_GET_WRITE(r2->protect.flags) \
    &&  NK_ASPACE_GET_EXEC(r1->protect.flags)  <= NK_ASPACE_GET_EXEC(r2->protect.flags) \
    &&  NK_ASPACE_GET_KERN(r1->protect.flags)  >= NK_ASPACE_GET_KERN(r2->protect.flags) \
    )

// typedef struct nk_aspace_carat {
//     // pointer to the abstract aspace that the
//     // rest of the kernel uses when dealing with this
//     // address space
//     nk_aspace_t *aspace;
    
//     // perhaps you will want to do concurrency control?
//     spinlock_t *  lock;

//     // Here you probably will want your region set data structure 
//     // What should it be...
//     mm_struct_t * reg_tracker;
//     // Your characteristics
//     nk_aspace_characteristics_t chars;

// } nk_aspace_carat_t;

typedef struct nk_aspace_carat_thread {
    struct nk_thread * thread_ptr;
    struct list_head thread_node;
} nk_aspace_carat_thread_t;

typedef struct nk_aspace_carat {
  // pointer to the abstract aspace that the
  // rest of the kernel uses when dealing with this
  // address space
  nk_aspace_t *aspace;

  // perhaps you will want to do concurrency control?
  spinlock_t  lock;

  // Here you probably will want your region set data structure 
  // What should it be...
  mm_struct_t * mm;

  // Your characteristics
  nk_aspace_characteristics_t chars;

  //We may need the list of threads
//   struct list_head threads;
    nk_aspace_carat_thread_t threads;

} nk_aspace_carat_t;





/**
 * CARAT_VALID return 1 if region not valid
 * validness = va_addr == pa_addr
 * */
int CARAT_INVALID(nk_aspace_region_t *region) {
    return region->va_start != region->pa_start;
}

static int destroy(void *state) {
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);


    ASPACE_UNLOCK(carat);
    return 0;
}

static int add_thread(void *state)
{   
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *t = get_cur_thread();
    
    DEBUG("adding thread %d (%s) to address space %s\n", t->tid,THREAD_NAME(t), ASPACE_NAME(carat));
    
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);
    
    nk_aspace_carat_thread_t * new_thread_wrapper = (nk_aspace_carat_thread_t *) malloc(sizeof(nk_aspace_carat_thread_t));
    new_thread_wrapper->thread_ptr = t;

    struct list_head * nelm = &new_thread_wrapper->thread_node;
    // carat->threads
    list_add_tail(nelm, &(carat->threads.thread_node));
    ASPACE_UNLOCK(carat);
    return 0;
}

static int remove_thread(void *state)
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *t = get_cur_thread();
    
    DEBUG("removing thread %d (%s) from address space %s\n", t->tid, THREAD_NAME(t), ASPACE_NAME(carat));
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    struct list_head *cur;
    int failed = 1;
    list_for_each(cur,&(carat->threads.thread_node)) {
	// if (!strcmp(list_entry(cur,struct nk_aspace,aspace_list_node)->name,name)) { 
	//     target = list_entry(cur,struct nk_aspace, aspace_list_node);
	//     break;
	// }
        nk_aspace_carat_thread_t * wrapper_ptr = list_entry(cur, nk_aspace_carat_thread_t, thread_node);
        if (wrapper_ptr->thread_ptr == t) {
            list_del(cur);
            free(wrapper_ptr);
            failed = 0; 
            break;
        }
    }

    ASPACE_UNLOCK(carat);
    return failed;
}

static int add_region(void *state, nk_aspace_region_t *region)
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);
    
    char region_buf[REGION_STR_LEN];
    region2str(region, region_buf);
    DEBUG("adding region %s to address space %s\n", region_buf, ASPACE_NAME(carat));
    
    // check region input validness
    if (CARAT_INVALID(region)) {
        DEBUG("Add region Failed: INVALID input (%s): CARAT expects equal VA and PA\n", region_buf);
        ASPACE_UNLOCK(carat);
        return -1;
    }
    
    // check if region to insert overlap with tracked region
    nk_aspace_region_t * overlap_ptr = mm_check_overlap(carat->mm, region);
    if (overlap_ptr) {
        DEBUG("Add region Failed: region Overlapping:\n"
                "\t(va=%016lx pa=%016lx len=%lx, prot=%lx) \n"
                "\t(va=%016lx pa=%016lx len=%lx, prot=%lx) \n", 
            region->va_start, region->pa_start, region->len_bytes, region->protect.flags,
            overlap_ptr->va_start, overlap_ptr->pa_start, overlap_ptr->len_bytes, overlap_ptr->protect.flags
        );
        ASPACE_UNLOCK(carat);
        return -1;
    }

    mm_insert(carat->mm, region);
    mm_show(carat->mm);
    
    ASPACE_UNLOCK(carat);
    return 0;
}

static int remove_region(void *state, nk_aspace_region_t *region) 
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    char region_buf[REGION_STR_LEN];
    region2str(region, region_buf);
    DEBUG("adding region %s to address space %s\n", region_buf, ASPACE_NAME(carat));
    
    // check region input validness
    if (CARAT_INVALID(region)) {
        DEBUG("Remove region Failed: INVALID input (%s): CARAT expects equal VA and PA\n", region_buf);
        ASPACE_UNLOCK(carat);
        return -1;
    }
    
    if (NK_ASPACE_GET_PIN(region->protect.flags)) {
        DEBUG("Cannot remove pinned region%s\n", region_buf);
        ASPACE_UNLOCK(carat);
        return -1;
    }

    uint8_t check_flag = VA_CHECK | PA_CHECK | LEN_CHECK | PROTECT_CHECK;
    int remove_failed = mm_remove(carat->mm, region, check_flag);

    if (remove_failed) {
        DEBUG("Remove region Failed: %s\n", region_buf);
        ASPACE_UNLOCK(carat);
        return -1;
    } 

    // TODO: remove region with CARAT
    ASPACE_UNLOCK(carat);
    return 0;
}

static int protect_region(void *state, nk_aspace_region_t *region, nk_aspace_protection_t *prot) 
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    // TODO: remove region with data structure
    ASPACE_UNLOCK(carat);
    return 0;
}


/**
 * Question: overalapping region is not legal in CARAT, right?
 * */

static int protection_check(void * state, nk_aspace_region_t * region) {

    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    char region_buf[REGION_STR_LEN];
    region2str(region, region_buf);
    
    DEBUG("Check protection of region %s\n", region_buf);
    // check region input validness
    if (CARAT_INVALID(region)) {
        DEBUG("Protection check Failed: INVALID input (%s): CARAT expects equal VA and PA\n", region_buf);
        ASPACE_UNLOCK(carat);
        return -1;
    }
    
    /**
     * Checking overlap
     * case 1
     * If @region is a subset of an tracked region, 
     *      the overlap_ptr should be deterministic;
     *      overlap_ptr = tracked region that contains @region
     *      return 0
     * 
     * case 2
     * If @region has intersection with several regions (at most two), but no single of them completely contains @region
     *      overlap_ptr may be any of them. 
     *      Yet, it is not a subset of the tracked region. 
     *      return -1
     * 
     * case 3
     * If @region has no inteserction with any tracked regions, 
     *      overlap_ptr = NULL
     *      return -1
     * */
    nk_aspace_region_t * overlap_ptr = mm_check_overlap(carat->mm, region);
    
    region2str(overlap_ptr, region_buf);

    // case 3
    if (overlap_ptr == NULL) {
        DEBUG("Protection check NOT passed! No overalapping region!\n");
        ASPACE_UNLOCK(carat);
        return -1;
    }

    // case 1
    if (overlap_ptr->pa_start <= region->pa_start 
        && overlap_ptr->pa_start + overlap_ptr->len_bytes >= region->pa_start + region->len_bytes 
        && PERMISSION_LEQ(region, overlap_ptr)
    ) { 
        
        DEBUG("Protection check passed! contained by %s\n", region_buf);
        ASPACE_UNLOCK(carat);
        return 0;
    }

    // case 2
    DEBUG("Protection check NOT passed! overlapped region = %s\n", region_buf);
    ASPACE_UNLOCK(carat);
    return -1;
}


static int move_region(void *state, nk_aspace_region_t *cur_region, nk_aspace_region_t *new_region) 
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    // TODO: move region with data structure
    
    // sanity checks
    if (CARAT_INVALID(cur_region)) {
        ASPACE_UNLOCK(carat);
        return -1;
    }

    if (CARAT_INVALID(new_region)) {
        ASPACE_UNLOCK(carat);
        return -1;
    }

    if (cur_region->len_bytes != new_region->len_bytes) {
        ASPACE_UNLOCK(carat);
        return -1;
    }

    // call CARAT runtime
    int res = nk_carat_move_region(cur_region->pa_start, new_region->pa_start, cur_region->len_bytes);
    if (res) {
        ASPACE_UNLOCK(carat);
        return -1;
    }

    // TODO: ask Peter if we need remove and insert new regions here or expect user to make those calls?

    
    ASPACE_UNLOCK(carat);
    return 0;
}

static int switch_from(void *state)
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *thread = get_cur_thread();
    
    DEBUG("switching out address space %s from thread %d (%s)\n",ASPACE_NAME(carat), thread->tid, THREAD_NAME(thread));
    
    return 0;
}

static int switch_to(void *state)
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *thread = get_cur_thread();
    
    DEBUG("switching in address space %s from thread %d (%s)\n", ASPACE_NAME(carat),thread->tid,THREAD_NAME(thread));
    
    return 0;
}

static int exception(void *state, excp_entry_t *exp, excp_vec_t vec) 
{   
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *thread = get_cur_thread();
    
    if (vec==GP_EXCP) {
    ERROR("general protection fault encountered.... uh...\n");
    ERROR("i have seen things that you people would not believe.\n");
    panic("general protection fault delivered to paging subsystem\n");
    return -1; // will never happen
    }

    if (vec!=PF_EXCP) {
    ERROR("Unknown exception %d delivered to paging subsystem\n",vec);
    panic("Unknown exception delivered to paging subsystem\n");
    return -1; // will never happen
    }

    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    // TODO: move region with data structure
    ASPACE_UNLOCK(carat);
    return 0;
}

static int print(void *state, int detailed) 
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *thread = get_cur_thread();
    

    // basic info
    nk_vc_printf("%s: paging address space [granularity 0x%lx alignment 0x%lx]\n",
         ASPACE_NAME(carat), carat->chars.granularity, carat->chars.alignment);

    if (detailed) {
        // print region set data structure here
        mm_show(carat->mm);
        // perhaps print out all the page tables here...
    }

    return 0;
}
static nk_aspace_interface_t carat_interface = {
    .destroy = destroy,
    .add_thread = add_thread,
    .remove_thread = remove_thread,
    .add_region = add_region,
    .remove_region = remove_region,
    .protect_region = protect_region,
    .protection_check = protection_check,
    .move_region = move_region,
    .switch_from = switch_from,
    .switch_to = switch_to,
    .exception = exception,
    .print = print
};

static int get_characteristics(nk_aspace_characteristics_t *c)
{   
    /**
     * CARAT has alignment and granuarity of ??
     * */
    c->alignment = 0;
    c->granularity = 0;
    return 0;
}


static struct nk_aspace *create(char *name, nk_aspace_characteristics_t *c)
{	
    struct naut_info *info = nk_get_nautilus_info();
    nk_aspace_carat_t *carat;
    
    carat = malloc(sizeof(*carat));
    
    if (!carat) {
        ERROR("cannot allocate carat aspace %s\n",name);
        return 0;
    }
    
    memset(carat,0,sizeof(*carat));

    spinlock_init(&(carat->lock));
    // initialize spinlock for carat
    // carat->lock = (spinlock_t *) malloc(sizeof(spinlock_t));
    // if (!carat->lock) {
    //     ERROR("cannot allocate spinlock for carat aspace %s at %p\n", name, carat);
    //     return 0;
    // }
    // spinlock_init(carat->lock);
    INIT_LIST_HEAD(&(carat->threads.thread_node));

    // initialize region dat structure
#ifdef NAUT_CONFIG_ASPACE_PAGING_REGION_RB_TREE
    carat->mm = mm_rb_tree_create();

#elif defined NAUT_CONFIG_ASPACE_PAGING_REGION_SPLAY_TREE
    carat->mm = mm_splay_tree_create();

#elif defined NAUT_CONFIG_ASPACE_PAGING_REGION_LINKED_LIST
    carat->mm = mm_llist_create();

#else
    carat->mm = mm_struct_create();

#endif

    
    // Define characteristic
    carat->chars = *c;

    carat->aspace = nk_aspace_register(name,
                    // we want both page faults and general protection faults (NO, no GPF)
                    //    NK_ASPACE_HOOK_PF | NK_ASPACE_HOOK_GPF,
                        NK_ASPACE_HOOK_GPF,
                    // our interface functions (see above)
                    &carat_interface,
                    // our state, which will be passed back
                    // whenever any of our interface functiosn are used
                    carat);
    if (!carat->aspace) {
        ERROR("Unable to register carat address space %s\n",name);
        return 0;
    }

    DEBUG("carat address space %s configured and initialized at 0x%p (returning 0x%p)\n", name,carat, carat->aspace);
    
    return carat->aspace;
}



static nk_aspace_impl_t carat = {
                .impl_name = "carat",
                .get_characteristics = get_characteristics,
                .create = create,
};

nk_aspace_register_impl(carat);


