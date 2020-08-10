#include "assert.h"
#include "node_struct.h"
#include <nautilus/nautilus.h>

int virtual_insert(mm_struct_t * self, nk_aspace_region_t * region) {
    // should never be called as a virtual function 
    assert(0);
    return 0;
}

void virtual_show(mm_struct_t * self) {
    // should never be called as a virtual function 
    assert(0);
}

nk_aspace_region_t * virtual_check_overlap(mm_struct_t * self, nk_aspace_region_t * region) {
    // should never be called as a virtual function 
    assert(0);
    return 0;
}

int virtual_remove(mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags) {
    // should never be called as a virtual function 
    assert(0);
    return 0;
}

nk_aspace_region_t* virtual_contains(mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags) {
    // should never be called as a virtual function 
    assert(0);
    return 0;
}

nk_aspace_region_t * virtual_find_reg_at_addr (mm_struct_t * self, addr_t address) {
    // should never be called as a virtual function 
    assert(0);
    return 0;
}

nk_aspace_region_t * virtual_update_region (
    mm_struct_t * self, 
    nk_aspace_region_t * cur_region, 
    nk_aspace_region_t * new_region, 
    uint8_t eq_flag
) {
    // should never be called as a virtual function 
    assert(0);
    return 0;
}


int mm_struct_init(mm_struct_t * self) {
    vtbl *vptr = (vtbl *) malloc (sizeof(vtbl));
    
    vptr->insert = &virtual_insert;
    vptr->show = &virtual_show;
    vptr->check_overlap = &virtual_check_overlap;
    vptr->remove = &virtual_remove;
    vptr->contains = &virtual_contains;
    vptr->find_reg_at_addr = &virtual_find_reg_at_addr;
    vptr->update_region = &virtual_update_region;

    self->vptr = vptr;
    self->size = 0;

    return 0;
}

mm_struct_t * mm_struct_create() {
    mm_struct_t *my_struct = (mm_struct_t *) malloc(sizeof(mm_struct_t));

    if (! my_struct) {
        ERROR_PRINT("cannot allocate a linked list data structure to track region mapping\n");
        return 0;
    }

    mm_struct_init(my_struct);

    return my_struct;
}

int mm_insert(mm_struct_t * self, nk_aspace_region_t * region) {
    return (* self->vptr->insert) (self, region);
}

void mm_show(mm_struct_t * self) {
    (*self->vptr->show) (self);
}

nk_aspace_region_t * mm_check_overlap(mm_struct_t * self, nk_aspace_region_t * region) {
    return (* self->vptr->check_overlap) (self, region);
}

int mm_remove(mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags) {
    return (* self->vptr->remove) (self, region, check_flags);
}

nk_aspace_region_t* mm_contains(mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags) {
    return (* self->vptr->contains) (self, region, check_flags);
}

nk_aspace_region_t * mm_find_reg_at_addr (mm_struct_t * self, addr_t address) {
    return (* self->vptr->find_reg_at_addr) (self, address);
}

nk_aspace_region_t * mm_update_region (
    mm_struct_t * self, 
    nk_aspace_region_t * cur_region, 
    nk_aspace_region_t * new_region, 
    uint8_t eq_flag
) {
    return (* self->vptr->update_region) (self, cur_region, new_region, eq_flag);
}

int region_equal(nk_aspace_region_t * regionA, nk_aspace_region_t * regionB, uint8_t check_flags) {
    if (check_flags & VA_CHECK) {
        if (regionA->va_start != regionB->va_start) return 0;
    }

    if (check_flags & PA_CHECK) {
        if (regionA->pa_start != regionB->pa_start) return 0;
    }

    if (check_flags & LEN_CHECK) {
        if (regionA->len_bytes != regionB->len_bytes) return 0;
    }

    if (check_flags & PROTECT_CHECK) {
        if (regionA->protect.flags != regionB->protect.flags) return 0;
    }

    return 1;
}

int region_update(nk_aspace_region_t * dest, nk_aspace_region_t * src, uint8_t eq_flags) {
    if (!(eq_flags & VA_CHECK)) {
        dest->va_start = src->va_start;
    }

    if (!(eq_flags & PA_CHECK)) {
        dest->pa_start = src->pa_start;
    }

    if (!(eq_flags & LEN_CHECK)) {
        dest->len_bytes = src->len_bytes;
    }

    if (!(eq_flags & PROTECT_CHECK)) {
        dest->protect = src->protect;
    }

    return 0;
}   