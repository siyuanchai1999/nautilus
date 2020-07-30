#include <nautilus/aspace.h>
// #include <nautilus/paging.h>
struct mm_struct_vtbl;

typedef struct mem_map_struct
{
    struct mm_struct_vtbl *vptr;
    unsigned int size;
} mm_struct_t;

typedef struct mm_struct_vtbl
{
    int (* insert) (mm_struct_t * self, nk_aspace_region_t * region);
    void (* show) (mm_struct_t * self);
    nk_aspace_region_t * (* check_overlap) (mm_struct_t * self, nk_aspace_region_t * region);
    int (* remove) (mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags);
    nk_aspace_region_t* (* contains) (mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags);
    nk_aspace_region_t * (* find_reg_at_addr) (mm_struct_t * self, addr_t address);
    
    nk_aspace_region_t * (* update_region) (
        mm_struct_t * self, 
        nk_aspace_region_t * cur_region, 
        nk_aspace_region_t * new_region, 
        uint8_t eq_flag
    );

} vtbl;

/*
insert region into the VA-PA map tracking data structure
*/
int mm_insert(mm_struct_t * self, nk_aspace_region_t * region);

/*
printout the VA-PA map tracking data structure
*/
void mm_show(mm_struct_t * self);

/*
check if region overlaps with existing region in the data structure
If overlapped, return the pointer to region in the data structure that overlaps with input region;
otherwise, return NULL;
*/
nk_aspace_region_t * mm_check_overlap(mm_struct_t * self, nk_aspace_region_t * region);

/*
remove if region overlaps with existing region in the data structure
return 
*/
int mm_remove(mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags);


nk_aspace_region_t* mm_contains(mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags);

nk_aspace_region_t * mm_find_reg_at_addr (mm_struct_t * self, addr_t address);

nk_aspace_region_t * mm_update_region (
    mm_struct_t * self, 
    nk_aspace_region_t * cur_region, 
    nk_aspace_region_t * new_region, 
    uint8_t eq_flag
);


int virtual_insert(mm_struct_t * self, nk_aspace_region_t * region);

void virtual_show(mm_struct_t * self);

nk_aspace_region_t * virtual_check_overlap(mm_struct_t * self, nk_aspace_region_t * region) ;

int virtual_remove(mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags);

nk_aspace_region_t* virtual_contains(mm_struct_t * self, nk_aspace_region_t * region, uint8_t check_flags);

nk_aspace_region_t * virtual_find_reg_at_addr (mm_struct_t * self, addr_t address);

nk_aspace_region_t * virtual_update_region (
    mm_struct_t * self, 
    nk_aspace_region_t * cur_region, 
    nk_aspace_region_t * new_region, 
    uint8_t eq_flag
);



int mm_struct_init(mm_struct_t * self);


#define VA_CHECK 1
#define PA_CHECK 2
#define LEN_CHECK 4
#define PROTECT_CHECK 8

int region_equal(nk_aspace_region_t * regionA, nk_aspace_region_t * regionB, uint8_t check_flags);

/*
    update every attribute that's not setup in eq_flags in dest from src. 
*/
int region_update(nk_aspace_region_t * dest, nk_aspace_region_t * src, uint8_t eq_flags);
