
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
 * Copyright (c) 2019, Hongyi Chen
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Hongyi Chen
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


//
// This is a template for the CS343 paging lab at
// Northwestern University
//
// Please also look at the paging_helpers files!
//
//
//
//

#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>
#include <nautilus/cpu.h>

#include <nautilus/aspace.h>

#include "paging_helpers.h"
#include "mm_linked_list.h"
#include "mm_splay_tree.h"

// #include "node_struct.h"
// #include "test.h"
//
// Add debugging and other optional output to this subsytem
//
#ifndef NAUT_CONFIG_DEBUG_ASPACE_PAGING
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("aspace-paging: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("aspace-paging: " fmt, ##args)
#define INFO(fmt, args...)   INFO_PRINT("aspace-paging: " fmt, ##args)

#define PAGING_PAGE_SIZE PAGE_SIZE_4KB
// Some macros to hide the details of doing locking for
// a paging address space
#define ASPACE_LOCK_CONF uint8_t _aspace_lock_flags
#define ASPACE_LOCK(a) _aspace_lock_flags = spin_lock_irq_save(&(a)->lock)
#define ASPACE_TRY_LOCK(a) spin_try_lock_irq_save(&(a)->lock,&_aspace_lock_flags)
#define ASPACE_UNLOCK(a) spin_unlock_irq_restore(&(a)->lock, _aspace_lock_flags);
#define ASPACE_UNIRQ(a) irq_enable_restore(_aspace_lock_flags);

// graceful printouts of names
#define ASPACE_NAME(a) ((a)?(a)->aspace->name : "default")
#define THREAD_NAME(t) ((!(t)) ? "(none)" : (t)->is_idle ? "(idle)" : (t)->name[0] ? (t)->name : "(noname)")
#define THRESH PAGE_SIZE_2MB

// You probably want some sort of data structure that will let you
// keep track of the set of regions you are asked to add/remove/change

// You will want some data structure to represent the state
// of a paging address space
typedef struct nk_aspace_paging {
    // pointer to the abstract aspace that the
    // rest of the kernel uses when dealing with this
    // address space
    nk_aspace_t *aspace;
    
    // perhaps you will want to do concurrency control?
    spinlock_t   lock;

    // Here you probably will want your region set data structure 
    // What should it be...
    mm_struct_t * paging_mm_struct;
    // Your characteristics
    nk_aspace_characteristics_t chars;

    // The cr3 register contents that reflect
    // the root of your page table hierarchy
    ph_cr3e_t     cr3;

    // The cr4 register contents used by the HW to interpret
    // your page table hierarchy.   We only care about a few bits
#define CR4_MASK 0xb0ULL // bits 4,5,7
    uint64_t      cr4;

} nk_aspace_paging_t;



// The function the aspace abstraction will call when it
// wants to destroy your address space
static  int destroy(void *state)
{
    // the pointer it hands you is for the state you supplied
    // when you registered the address space
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;

    DEBUG("destroying address space %s\n", ASPACE_NAME(p));

    ASPACE_LOCK_CONF;

    // lets do that with a lock, perhaps? 
    ASPACE_LOCK(p);
    //
    // WRITEME!!    actually do the work
    // 
    free(p->paging_mm_struct);
    free(p);
    ASPACE_UNLOCK(p);

    return 0;
}

// The function the aspace abstraction will call when it
// is adding a thread to your address space
// do you care? 
static int add_thread(void *state)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;
    struct nk_thread *t = get_cur_thread();
    
    DEBUG("adding thread %d (%s) to address space %s\n", t->tid,THREAD_NAME(t), ASPACE_NAME(p));
    
    return 0;
}
    
    
// The function the aspace abstraction will call when it
// is removing from your address space
// do you care? 
static int remove_thread(void *state)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;
    struct nk_thread *t = get_cur_thread();
    
    DEBUG("removing thread %d (%s) from address space %s\n", t->tid, THREAD_NAME(t), ASPACE_NAME(p));
    
    return 0;
}



// The function the aspace abstraction will call when it
// is adding a region to your address space

ph_pf_access_t access_from_region (nk_aspace_region_t *region) {
    ph_pf_access_t access;
    access.val = 0;
    
    access.write = NK_ASPACE_GET_WRITE(region->protect.flags);
    access.user = !NK_ASPACE_GET_KERN(region->protect.flags);
    access.ifetch = NK_ASPACE_GET_EXEC(region->protect.flags);

    return access;
}


int clear_cache (nk_aspace_paging_t *p, nk_aspace_region_t *region, uint64_t threshold) {
    
    // if we are editing the current address space of this cpu, then we
    // might need to flush the TLB here.   We can do that with a cr3 write
    // like: write_cr3(p->cr3.val);

    // if this aspace is active on a different cpu, we might need to do
    // a TLB shootdown here (out of scope of class)
    // a TLB shootdown is an interrupt to a remote CPU whose handler
    // flushes the TLB

    if (p->aspace == get_cpu()->cur_aspace) {
        if (region->len_bytes > threshold) {
            write_cr3(p->cr3.val);
            DEBUG("flush TLB DONE!\n");
        } else {
            uint64_t offset = 0;
            while (offset < region->len_bytes) {
                invlpg((addr_t)region->va_start + (addr_t) offset);
                offset = offset + p->chars.granularity;
            }
            DEBUG("virtual address cache from %016lx to %016lx are invalidated\n", region->va_start, region->pa_start);
        }
    } else {
        // TLB shootdown???
    }
    return 0;
}

static int add_region(void *state, nk_aspace_region_t *region)
{   
    
    // add the new node into region_list
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;

    DEBUG("adding region (va=%016lx pa=%016lx len=%lx) to address space %s\n", region->va_start, region->pa_start, region->len_bytes,ASPACE_NAME(p));

    ASPACE_LOCK_CONF;

    ASPACE_LOCK(p);

    // WRITE ME!!
    // DEBUG("cr3 = %016lx\tcr4 = %016lx\n", p->cr3, p->cr4);
    // DEBUG("alignment = %lx\talignment = %lx\n", p->chars.alignment, p->chars.granularity);
    // first you should sanity check the region and then place it into
    // your region data structure

    // sanity check to be sure it doesn't overlap an existing region...
    nk_aspace_region_t * overlap_ptr = mm_check_overlap(p->paging_mm_struct, region);
    if (overlap_ptr) {
        DEBUG("region Overlapping:\n"
                "\t(va=%016lx pa=%016lx len=%lx, prot=%lx) \n"
                "\t(va=%016lx pa=%016lx len=%lx, prot=%lx) \n", 
            region->va_start, region->pa_start, region->len_bytes, region->protect.flags,
            overlap_ptr->va_start, overlap_ptr->pa_start, overlap_ptr->len_bytes, overlap_ptr->protect.flags
        );
        ASPACE_UNLOCK(p);
        return -1;
    }
    DEBUG("no region overlapped\n");

    if (NK_ASPACE_GET_EAGER(region->protect.flags)) {
	
        // an eager region means that we need to build all the corresponding
        // page table entries right now, before we return

        // DRILL THE PAGE TABLES HERE
        ph_pf_access_t access_type = access_from_region(region);
        uint64_t offset = 0;
        int ret;
        while (offset < region->len_bytes){
            ret = paging_helper_drill(
                p->cr3, 
                (addr_t) region->va_start + (addr_t) offset, 
                (addr_t) region->pa_start + (addr_t) offset, 
                access_type
            );

            // DEBUG("%d: helper_drill return = %d\n", offset, ret);
            if (ret < 0) {
                DEBUG("Failed to drill at virtual address=%p"
                        " physical adress %p"
                        " and ret code of %d",
                        (addr_t) region->va_start + (addr_t) offset,
                        (addr_t) region->pa_start + (addr_t) offset,
                        ret
                );
                ASPACE_UNLOCK(p);
                return ret;
            }
            offset = offset + p->chars.granularity;
        }
        DEBUG("Eager paging table drill done!\n");
    }
    else {
        // lazy drilling 
        // nothing to do
    }

    // DEBUG("before mm_insert\n");

    mm_insert(p->paging_mm_struct, region);
    // DEBUG("after mm_insert\n");
    mm_show(p->paging_mm_struct);

    

    clear_cache(p, region, THRESH);

    ASPACE_UNLOCK(p);
    
    return 0;
}

// The function the aspace abstraction will call when it
// is removing a region from your address space
static int remove_region(void *state, nk_aspace_region_t *region)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;

    DEBUG("removing region (va=%016lx pa=%016lx len=%lx) "
            "from address space %s\n", 
            region->va_start, region->pa_start, region->len_bytes,
            ASPACE_NAME(p)
    );

    ASPACE_LOCK_CONF;

    ASPACE_LOCK(p);

    // WRITE ME!
    
    // first, find the region in your data structure
    // it had better exist and be identical.
    uint8_t check_flag = VA_CHECK | PA_CHECK | LEN_CHECK | PROTECT_CHECK;
    int remove_success = mm_remove(p->paging_mm_struct, region, check_flag);

    if (!remove_success) {
        DEBUG("region to remove \
            (va=%016lx pa=%016lx len=%lx, prot=%lx) not FOUND", 
            region->va_start, 
            region->pa_start, 
            region->len_bytes,
            region->protect.flags
        );
        ASPACE_UNLOCK(p);
        return -1;
    }    

    // next, remove all corresponding page table entries that exist
    ph_pf_access_t access_type = access_from_region(region);
    uint64_t offset = 0;
    
    while (offset < region->len_bytes){
        uint64_t *entry;
        addr_t virtaddr = (addr_t) region->va_start + (addr_t) offset;
        int ret = paging_helper_walk(p->cr3, virtaddr, access_type, &entry);

        ((ph_pte_t *) entry)->present = 0;  

        offset = offset + p->chars.granularity;
    }
    

    clear_cache(p, region, THRESH);

    
    ASPACE_UNLOCK(p);

    return 0;

}
   
// The function the aspace abstraction will call when it
// is changing the protections of an existing region
static int protect_region(void *state, nk_aspace_region_t *region, nk_aspace_protection_t *prot)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;

    DEBUG("protecting region" 
            "(va=%016lx pa=%016lx len=%lx, prot=%lx)" 
            "from address space %s"
            "to new protection = %lx\n", 
            region->va_start, region->pa_start, region->len_bytes, region->protect.flags,
            ASPACE_NAME(p),
            prot->flags
    );

    DEBUG("Old protection details:" 
        "(read=%d write=%d exec=%d pin=%d kern=%d swap=%d eager=%d)\n",
        NK_ASPACE_GET_READ(region->protect.flags),
        NK_ASPACE_GET_WRITE(region->protect.flags),
        NK_ASPACE_GET_EXEC(region->protect.flags),
        NK_ASPACE_GET_PIN(region->protect.flags), 
        NK_ASPACE_GET_KERN(region->protect.flags), 
        NK_ASPACE_GET_SWAP(region->protect.flags), 
        NK_ASPACE_GET_EAGER(region->protect.flags)
    );

    DEBUG("new protection details:" 
        "(read=%d write=%d exec=%d pin=%d kern=%d swap=%d eager=%d)\n",
        NK_ASPACE_GET_READ(prot->flags),
        NK_ASPACE_GET_WRITE(prot->flags),
        NK_ASPACE_GET_EXEC(prot->flags),
        NK_ASPACE_GET_PIN(prot->flags), 
        NK_ASPACE_GET_KERN(prot->flags), 
        NK_ASPACE_GET_SWAP(prot->flags), 
        NK_ASPACE_GET_EAGER(prot->flags)
    );

    ASPACE_LOCK_CONF;

    ASPACE_LOCK(p);

    // WRITE ME!
    
    nk_aspace_region_t new_prot_wrapper = *region;
    new_prot_wrapper.protect = *prot;
    // first, find the region in your data structure
    // it had better exist and be identical except for protections
    uint8_t check_flag = VA_CHECK | LEN_CHECK | PA_CHECK;
    nk_aspace_region_t* reg_ptr = mm_update_region(p->paging_mm_struct, region, &new_prot_wrapper, check_flag);
    
    if (reg_ptr == NULL) {
        DEBUG("region to update protect \
             (va=%016lx pa=%016lx len=%lx, prot=%lx) not FOUND", 
            region->va_start, 
            region->pa_start, 
            region->len_bytes,
            region->protect.flags
        );
        ASPACE_UNLOCK(p);
        return -1;
    }

    // next, update the region protections from your data structure
    ph_pf_access_t access_type = access_from_region(region);
    ph_pf_access_t new_access = access_from_region(reg_ptr);

    if (!NK_ASPACE_GET_EAGER(region->protect.flags) && 
        NK_ASPACE_GET_EAGER(reg_ptr->protect.flags)
    ) {
        uint64_t offset = 0;
        int ret;
        while (offset < reg_ptr->len_bytes){
            ret = paging_helper_drill(
                p->cr3, 
                (addr_t) reg_ptr->va_start + (addr_t) offset, 
                (addr_t) reg_ptr->pa_start + (addr_t) offset, 
                new_access
            );

            // DEBUG("%d: helper_drill return = %d\n", offset, ret);
            
            if (ret < 0) {
                DEBUG("Failed to drill at virtual address=%p"
                        " physical adress %p"
                        " and ret code of %d",
                        (addr_t) reg_ptr->va_start + (addr_t) offset,
                        (addr_t) reg_ptr->pa_start + (addr_t) offset,
                        ret
                );
                ASPACE_UNLOCK(p);
                return ret;
            }
            offset = offset + p->chars.granularity;
        }
    } else if (access_type.val != new_access.val) {
            // next, update all corresponding page table entries that exist
        uint64_t offset = 0;

        while (offset < reg_ptr->len_bytes){
            uint64_t *entry;
            addr_t virtaddr = (addr_t) region->va_start + (addr_t) offset;
            int ret = paging_helper_walk(p->cr3, virtaddr, access_type, &entry);
            
            if(! ret){
                perm_set(entry, new_access);
            }
            
            offset = offset + p->chars.granularity;
        }
    }
    // next, if we are editing the current address space of this cpu,
    // we need to either invalidate individual pages using invlpg()
    // or do a full TLB flush with a write to cr3.
    clear_cache(p, reg_ptr, THRESH);
    // next, if this address space is active on a different cpu, we
    // would need to do a TLB shootdown for that cpu
    ASPACE_UNLOCK(p);

    return 0;
}

static int move_region(void *state, nk_aspace_region_t *cur_region, nk_aspace_region_t *new_region)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;

    DEBUG("moving region (va=%016lx pa=%016lx len=%lx, prot=%lx)"
            "in address space %s" 
            "to (va=%016lx pa=%016lx len=%lx, prot=%lx)\n", 
            cur_region->va_start, cur_region->pa_start, cur_region->len_bytes, cur_region->protect.flags,
            ASPACE_NAME(p),
            new_region->va_start, new_region->pa_start, new_region->len_bytes, new_region->protect.flags
    );

    ASPACE_LOCK_CONF;

    ASPACE_LOCK(p);

    // WRITE ME!
    
    // first, find the region in your data structure
    // it had better exist and be identical except for the physical addresses
    uint8_t check_flag = VA_CHECK | LEN_CHECK | PROTECT_CHECK;
    int reg_eq = region_equal(cur_region, new_region, check_flag);
    if (!reg_eq) {
        DEBUG("regions differ in attributes other than physical address!\n");
        ASPACE_UNLOCK(p);
        return -1;
    }

    nk_aspace_region_t* reg_ptr = mm_update_region(
                                    p->paging_mm_struct, 
                                    cur_region,
                                    new_region,
                                    check_flag
                                );
    if (!reg_ptr) {
        DEBUG(
            "region to update"
            "(va=%016lx pa=%016lx len=%lx, prot=%lx) not FOUND", 
            cur_region->va_start, 
            cur_region->pa_start, 
            cur_region->len_bytes,
            cur_region->protect.flags
        );
        ASPACE_UNLOCK(p);
        return -1;
    }
    // next, update the region in your data structure
    // you can assume that the caller has done the work of copying the memory
    // contents to the new physical memory

    // next, update all corresponding page table entries that exist
    ph_pf_access_t access_type = access_from_region(cur_region);
    uint64_t offset = 0;

    while (offset < cur_region->len_bytes){
        uint64_t *entry;
        addr_t virtaddr = (addr_t) cur_region->va_start + (addr_t) offset;
        int ret = paging_helper_walk(p->cr3, virtaddr, access_type, &entry);
        ((ph_pte_t *) entry)->present = 0;

        offset = offset + p->chars.granularity;
    }
    // next, if we are editing the current address space of this cpu,
    // we need to either invalidate individual pages using invlpg()
    // or do a full TLB flush with a write to cr3.

    clear_cache(p, cur_region, THRESH );
    // next, if this address space is active on a different cpu, we
    // would need to do a TLB shootdown for that cpu


    // ADVANCED VERSION: allow for splitting the region - if cur_region
    // is a subset of some region, then split that region, and only move
    // the affected addresses.   The granularity of this is that reported
    // in the aspace characteristics (i.e., page granularity here).
    

    // next, remove all corresponding page table entries that exist 


    if (NK_ASPACE_GET_EAGER(new_region->protect.flags)) {
	// an eager region means that we need to build all the corresponding
	// page table entries right now, before we return
	// DRILL THE PAGE TABLES HERE
        uint64_t offset = 0;
        int ret;
        while (offset < new_region->len_bytes)
        {
            ph_pf_access_t access_type = access_from_region(new_region);
            ret = paging_helper_drill(
                p->cr3, 
                (addr_t) new_region->va_start + (addr_t) offset, 
                (addr_t) new_region->pa_start + (addr_t) offset, 
                access_type
            );

            offset = offset + p->chars.granularity;
            if (ret < 0) {
                DEBUG("Failed to drill at virtual address=%p"
                        " physical adress %p"
                        " and ret code of %d",
                        (addr_t) reg_ptr->va_start + (addr_t) offset,
                        (addr_t) reg_ptr->pa_start + (addr_t) offset,
                        ret
                );
                ASPACE_UNLOCK(p);
                return ret;
            }
        }
    } else {
        // nothing to do for uneager region
    }
    
    ASPACE_UNLOCK(p);

    return 0;
}


// called by the address space abstraction when it is switching away from
// the noted address space.   This is part of the thread context switch.
// do you care?
static int switch_from(void *state)
{
    struct nk_aspace_paging *p = (struct nk_aspace_paging *)state;
    struct nk_thread *thread = get_cur_thread();
    
    DEBUG("switching out address space %s from thread %d (%s)\n",ASPACE_NAME(p), thread->tid, THREAD_NAME(thread));
    
    return 0;
}

// called by the address space abstraction when it is switching to the
// noted address space.  This is part of the thread context switch.
static int switch_to(void *state)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;
    struct nk_thread *thread = get_cur_thread();
    
    DEBUG("switching in address space %s from thread %d (%s)\n", ASPACE_NAME(p),thread->tid,THREAD_NAME(thread));
    
    // Here you will need to install your page table hierarchy
    // first point CR3 to it
    write_cr3(p->cr3.val);

    // next make sure the interpretation bits are set in cr4
    uint64_t cr4 = read_cr4();
    cr4 &= ~CR4_MASK;
    cr4 |= p->cr4;
    write_cr4(cr4);
    
    return 0;
}

// called by the address space abstraction when a page fault or a
// general protection fault is encountered in the context of the
// current thread
//
// exp points to the hardware interrupt frame on the stack
// vec indicates which vector happened
//
static int exception(void *state, excp_entry_t *exp, excp_vec_t vec)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;
    struct nk_thread *thread = get_cur_thread();
    
    // DEBUG("exception 0x%x for address space %s in context of thread %d (%s)\n",vec,ASPACE_NAME(p),thread->tid,THREAD_NAME(thread));
    
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
    
    // It must be a page fault
    
    // find out what the fault address and the fault reason
    uint64_t virtaddr = read_cr2();
    ph_pf_error_t  error; error.val = exp->error_code;
    DEBUG("Page fault at virtaddr = %llx, error = %llx\n", virtaddr, error.val);
    
    DEBUG("Page fault at error.present = %x, "
            "error.write = %x " 
            "error.user = %x " 
            "error.rsvd_access = %x " 
            "error.ifetch = %x \n", 
            error.present,
            error.write,
            error.user,
            error.rsvd_access,
            error.ifetch    
    );
    ASPACE_LOCK_CONF;
    // DEBUG("try to lock at %p\n", &p->lock);
    ASPACE_LOCK(p);

    //
    // WRITE ME!
    //
    // DEBUG("looking for region contains %lx\n", virtaddr);
    nk_aspace_region_t * region = mm_find_reg_at_addr(p->paging_mm_struct, (addr_t) virtaddr);
    if (region == NULL) {
        // if there is no such region, this is an unfixable fault
        //   if this is a user thread, we now would signal it or kill it
        //   if it's a kernel thread, the kernel should panic
        //   if it's within an interrupt handler, the kernel should panic
        panic("Page Fault at %p, but no matching region found\n", virtaddr);
        ASPACE_UNLOCK(p);
        return -1;
    }
    // DEBUG("region found at%p\n", region);
    // Now find the region corresponding to this address
    // Is the problem that the page table entry is not present?
    // if so, drill the entry and then return from the function
    // so the faulting instruction can try agai
    //    This is the lazy construction of the page table entries

    // Assuming the page table entry is present, check the region's
    // protections and compare to the error code
    int ret;
    ph_pf_access_t access_type = access_from_region(region);


    if(!error.present){
        addr_t pa_todrill = (addr_t) region->pa_start + (addr_t) virtaddr - (addr_t) region->va_start;
        // DEBUG("pa_todrill = %016lx\n", pa_todrill);
        ret = paging_helper_drill(p->cr3, (addr_t) virtaddr, pa_todrill, access_type);
        ASPACE_UNLOCK(p);
        return ret;
    }else{
        
        int ok = (access_type.write >= error.write) && 
                    (access_type.user>= error.user) && 
                    (access_type.ifetch >= error.ifetch);

        DEBUG(
            "region.protect.write=%d, error.write=%d\n" 
            "region.protect.user=%d, error.user=%d\n" 
            "region.protect.ifetch=%d, error.ifetch=%d\n",
            access_type.write, error.write,
            access_type.user, error.user,
            access_type.ifetch, error.ifetch
        );

        if(ok){
            ASPACE_UNLOCK(p);
            panic("weird Page fault with permission ok and page present\n");
            return 0;
        } else{
            ASPACE_UNLOCK(p);
            panic("Permission not allowed\n");
            return -1;
        }
    }
    // if the region has insufficient permissions for the request,
    // then this is an unfixable fault
    //   if this is a user thread, we now would signal it or kill it
    //   if it's a kernel thread, the kernel should panic
    //   if it's within an interrupt handler, the kernel should panic
    
    ASPACE_UNLOCK(p);
    
    return 0;
}
    
// called by the address space abstraction when it wants you
// to print out info about the address space.  detailed is
// nonzero if it wants a detailed output.  Use the nk_vc_printf()
// function to print here
static int print(void *state, int detailed)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;
    struct nk_thread *thread = get_cur_thread();
    

    // basic info
    nk_vc_printf("%s: paging address space [granularity 0x%lx alignment 0x%lx]\n"
		 "   CR3:    %016lx  CR4m: %016lx\n",
		 ASPACE_NAME(p), p->chars.granularity, p->chars.alignment, p->cr3.val, p->cr4);

    if (detailed) {
        // print region set data structure here
        mm_show(p->paging_mm_struct);
        // perhaps print out all the page tables here...
    }

    return 0;
}    

//
// This structure binds together your interface functions
// with the interface definition of the address space abstraction
// it will be used later in registering an address space
//
static nk_aspace_interface_t paging_interface = {
    .destroy = destroy,
    .add_thread = add_thread,
    .remove_thread = remove_thread,
    .add_region = add_region,
    .remove_region = remove_region,
    .protect_region = protect_region,
    .move_region = move_region,
    .switch_from = switch_from,
    .switch_to = switch_to,
    .exception = exception,
    .print = print
};


//
// The address space abstraction invokes this function when
// someone asks about your implementations characterstics
//
static int   get_characteristics(nk_aspace_characteristics_t *c)
{
    // you must support 4KB page granularity and alignment
    c->granularity = c->alignment = PAGE_SIZE_4KB;
    
    return 0;
}


//
// The address space abstraction invokes this function when
// someone wants to create a new paging address space with the given
// name and characteristics
//
static struct nk_aspace * create(char *name, nk_aspace_characteristics_t *c)
{
    struct naut_info *info = nk_get_nautilus_info();
    nk_aspace_paging_t *p;
    
    p = malloc(sizeof(*p));
    
    if (!p) {
	ERROR("cannot allocate paging aspace %s\n",name);
	return 0;
    }
  
    memset(p,0,sizeof(*p));
    
    spinlock_init(&p->lock);

    // initialize your region set data structure here!
    // p->paging_mm_struct = mm_llist_create();
    p->paging_mm_struct = mm_splay_tree_create();

    // create an initial top-level page table (PML4)
    if(paging_helper_create(&(p->cr3)) == -1){
	ERROR("unable create aspace cr3 in address space %s\n", name);
    }

    // note also the cr4 bits you should maintain
    p->cr4 = nk_paging_default_cr4() & CR4_MASK;

    p->chars = *c;

    // if we supported address spaces other than long mode
    // we would also manage the EFER register here

    // Register your new paging address space with the address space
    // space abstraction
    // the registration process returns a pointer to the abstract
    // address space that the rest of the system will use
    p->aspace = nk_aspace_register(name,
				   // we want both page faults and general protection faults
				   NK_ASPACE_HOOK_PF | NK_ASPACE_HOOK_GPF,
				   // our interface functions (see above)
				   &paging_interface,
				   // our state, which will be passed back
				   // whenever any of our interface functiosn are used
				   p);
    
    if (!p->aspace) {
	ERROR("Unable to register paging address space %s\n",name);
	return 0;
    }
    
    DEBUG("paging address space %s configured and initialized (returning %p)\n", name, p->aspace);
    
    // you are returning
    return p->aspace; 
}

//
// This structure binds together the interface functions of our
// implementation with the relevant interface definition
static nk_aspace_impl_t paging = {
				.impl_name = "paging",
				.get_characteristics = get_characteristics,
				.create = create,
};


// this does linker magic to populate a table of address space
// implementations by including this implementation
nk_aspace_register_impl(paging);
