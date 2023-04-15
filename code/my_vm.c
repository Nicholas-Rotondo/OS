#include "my_vm.h"

unsigned long start_phys_mem;

pde_t *pgdir = NULL;
mpnode_t *mp_list = NULL;
bitmap_t *phys_bitmap = NULL, *virt_bitmap = NULL;

int pdbits, pdmask, ptbits, ptmask, offbits, offmask;

int hits = 0;
int misses = 0;

int eviction_count = 0;
int check_occupancy = 0;


// this seems okay to place here, just need to figure out
// where is the best location to free.
tlb_t *tlb_arr = (tlb_t *)malloc(sizeof(tlb_t * TLB_ENTRIES));
if(tlb_arr == NULL) {
    fprintf(stderr, "Memory allocation failed");
    return 1;
} 

/*
Function responsible for allocating and setting your physical memory 
*/
int set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    fprintf(stderr, "set_physical_mem() called\n");

    pgdir = (pde_t *)calloc(MEMSIZE, 1);
    if ( pgdir == NULL ) return -1;

    start_phys_mem = (unsigned long) pgdir;
    fprintf(stderr, "\tPage directory/Physical memory allocated starting at address %lu\n", start_phys_mem);

    offbits = log2(PGSIZE);
    pdbits = (ADDR_BITS - offbits)/2;
    ptbits = ((ADDR_BITS - offbits)%2) ? pdbits + 1 : pdbits;
    fprintf(stderr, "\tVirtual addresses will be split into %d bits for the page directory index, %d bits for the page table index, and %d bits for the offset\n", pdbits, ptbits, offbits);

    offmask = PGSIZE - 1;
    ptmask = (exp_2(ptbits) - 1) << offbits;
    pdmask = ((exp_2(pdbits) - 1) << offbits ) << ptbits;

    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them

    phys_bitmap = (bitmap_t *)malloc(sizeof(bitmap_t));
    if ( phys_bitmap == NULL ) return -1;

    phys_bitmap->map_size = NUM_PAGES;
    phys_bitmap->map_length = NUM_PAGES/8;
    phys_bitmap->bitmap = (unsigned char *)calloc(phys_bitmap->map_length, sizeof(unsigned char));
    if ( phys_bitmap->bitmap == NULL ) return -1;
    fprintf(stderr, "\tThe physical bitmap represents %d physical pages using %d indices\n", phys_bitmap->map_size, phys_bitmap->map_length);

    virt_bitmap = (bitmap_t *)malloc(sizeof(bitmap_t));
    if ( virt_bitmap == NULL ) return -1;

    virt_bitmap->map_size = (PGSIZE/4) * exp_2(ptbits);
    virt_bitmap->map_length = virt_bitmap->map_size/8; 
    virt_bitmap->bitmap = (unsigned char *)calloc(virt_bitmap->map_length, sizeof(unsigned char));
    if ( virt_bitmap->bitmap == NULL ) return -1;
    fprintf(stderr, "\tThe virtual bitmap represents %d virtual pages using %d indices\n", virt_bitmap->map_size, virt_bitmap->map_length);
   
    tlb_t *tlb_arr = (tlb_t * )malloc(sizeof(tlb_t) * TLB_ENTRIES);
    if(tlb_arr == NULL) {
        fprintf(stderr, "Memory allocation failed");
        return 1;
    } 
    
    return 0;

}


/*
* Part 2: Add a virtual to physical page translation to the TLB.
* Feel free to extend the function arguments or return type.
*/
int
add_TLB(unsigned long *va, unsigned long *pa)
{
    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    // we need to find a way to store the age of a tlb
    // meaning, it is important to remove the oldest TLB in the event of an eviction.
    // for now just keep this simple implementation and add donce we have an idea
    tlb_t *tlb = (tlb_t *)malloc(sizeof(tlb_t)); 
    if(tlb == NULL) {
        fprintf(stderr, "Memory allocation failed");
        return 1;
    }  

    int i;

    tlb->virt_addr = va;
    tlb->phys_addr = pa;

    for(i = 0; i < TLB_ENTRIES; i++) {
        if(tlb_arr[0] == NULL) {
            tlb_arr[0] = tlb;
            check_occupancy += 1;
            fprintf(stderr, "added tlb to entry 0, occupancy is: %d", check_occupancy);
    }
        if(eviction_count == TLB_ENTRIES) {
            eviction_count = 0;
            fprintf(stderr, "we reached eviction count conditional");
        }
        if(check_occupancy == TLB_ENTRIES) {
            check_occupancy = 0;
            tlb_arr[eviction_count] = NULL;
            tlb_arr[eviction_count] = tlb;
            eviction_count += 1;
            fprintf(stderr, "eviction count is: %d", eviction_count);
        }
        if(tlb_arr[0] != NULL) {
            //check the array and see if there is an open slot
            if(tlb_arr[i] == NULL) {
                tlb_arr[i] = tlb_store;
                check_occupancy += 1;
                fprintf(stderr, "added tlb to index %d, occupancy is: %d", i, check_occupancy);
            }
        }
    }
    free(tlb);
    return 0;
}


/*
* Part 2: Check TLB for a valid translation.
* Returns the physical page address.
* Feel free to extend this function and change the return type.
*/
pte_t *
check_TLB(unsigned long *va) {

    /* Part 2: TLB lookup code here */

    pte_t *ret_phys_addr = NULL;
    tlb_t *tlb = (tlb_t *)malloc(sizeof(tlb_t));
    if(tlb == NULL) {
        fprintf(stderr, "Memory allocation failed");
        return 1;
    }
    
    unsigned long vpn = (unsigned long)va/PGSIZE;

    if(vpn == tlb_arr[vpn%TLB_ENTRIES]->tag) {
        ret_phys_addr = tlb_store->phys_addr;
        hits += 1;
        fprintf(stderr, "hit count is: %d", hits);
    }
    else {
        misses += 1;
        fprintf(stderr, "miss count is: %d", misses);
        // we return NULL here so we can check this in our translate function.
        // maybe we just do the actual translation here?  
        return NULL;
    }

    free(tlb);
    return ret_phys_addr;

/*This function should return a pte_t pointer*/
}



/*
* Part 2: Print TLB miss rate.
* Feel free to extend the function arguments or return type.
*/
void
print_TLB_missrate(int hits, int misses)
{
    double miss_rate = 0;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    miss_rate = (hits/(hits+misses));

    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
unsigned long translate(unsigned long va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */

    // uncomment this once paging works.
    // pte_t *phys_addr = check_TLB(va);
    // if(phys_addr != NULL) {
    //     return 1;
    // }

    unsigned long offset = va & offmask;
    unsigned long pt_index = (va & ptmask) >> offset;
    unsigned long pd_index = ((va & pdmask) >> ptbits) >> offset;

    fprintf(stderr, "\tVirtual address was translated to page directory index %lu, page table index %lu, and offset %lu\n", pd_index, pt_index, offset);

    if ( pd_index >= PGSIZE/4 ) {
        fprintf(stderr, "\tPage directory index out of bounds\n");
        return 0;
    }
    pte_t *pgtable = (pte_t *)pgdir[pd_index];
    if ( pgtable == NULL ) {
        fprintf(stderr, "\tNo page table found at index %lu\n", pd_index);
        return 0;
    }
    fprintf(stderr, "\tPage directory indexed: Jumping to page table at physical address %lu\n", pgdir[pd_index]);
    unsigned long pfn = pgtable[pt_index];

    if ( pfn == 0 ) {
        fprintf(stderr, "\t\tPage table indexed: Translation failed\n");
        return 0;
    }
    else {
        fprintf(stderr, "\t\tPage table indexed: Translated virtual address %lu to physical address %lu with physical frame number %lu\n", va, ( pfn << offbits ) | offset, pfn);
        return ( pfn << offbits ) | offset;
    }
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int page_map(unsigned long va, unsigned long pa) {

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    fprintf(stderr, "page_map() called to map virtual address %lu to physical address %lu\n", va, pa);

    unsigned long offset = va & offmask;
    unsigned long pt_index = (va & ptmask) >> offset;
    unsigned long pd_index = ((va & pdmask) >> ptbits) >> offset;

    fprintf(stderr, "\tVirtual address was translated to page directory index %lu, page table index %lu, and offset %lu\n", pd_index, pt_index, offset);

    if ( pd_index >= PGSIZE/4 ) {
        fprintf(stderr, "\tPage directory index out of bounds\n");
        return -1;
    }

    if ( pgdir[pd_index] == 0 ) {
        fprintf(stderr, "\tNo page table found at index %lu: Calling get_next_cont() to allocate page table\n", pd_index);
        unsigned long next_page_table = get_next_cont((exp_2(ptbits))/(PGSIZE/4));
        if ( next_page_table == 0 ) {
            fprintf(stderr, "\t\tPage table could not be allocated\n");
            return -1;
        }
        fprintf(stderr, "\t\tPage table allocated with physical address %lu\n", next_page_table);
        pgdir[pd_index] = next_page_table;
    }

    pte_t *pgtable = (pte_t *)pgdir[pd_index];
    fprintf(stderr, "\tPage directory indexed: Jumping to page table at physical address %lu\n", pgdir[pd_index]);

    if ( ! pgtable[pt_index] ) {
        pgtable[pt_index] = pa;
        fprintf(stderr, "\t\tPage table indexed: Mapped physical address %lu to index %lu in page table\n", pgtable[pt_index], pt_index);
        //add_TLB(va, pa);
    } else {
        if ( pa == 0 ) {
            pgtable[pt_index] = pa;
        } else {
            fprintf(stderr, "\t\tPage table indexed: Page map failed, as physical address %lu resides at index %lu in page table\n", pgtable[pd_index], pt_index);
            return -1;
        }
    }

    return 0;
}


/*Function that gets the next available page
*/
unsigned long *get_next_avail(int num_pages) {

    fprintf(stderr, "get_next_avail() called requesting %d physical pages\n", num_pages);
    unsigned long *avail_pages = (unsigned long *)malloc(num_pages * sizeof(unsigned long));
    unsigned long page_addr = start_phys_mem;

    int num_page = 0;

    for ( int i = 0; i < phys_bitmap->map_length; i++ ) {

        if ( phys_bitmap->bitmap[i] == 255 ) {
            page_addr += (PGSIZE * 8);
            fprintf(stderr, "\tNo physical pages found at index %d | Page address updated to %lu (Offset from start is now %lu)\n", i, page_addr, page_addr-start_phys_mem);
        }

        else {

            fprintf(stderr, "\tPhysical page located within index %d | Bitmap at this index: %x", i, phys_bitmap->bitmap[i]);

            unsigned char map = 1;

            for ( int j = 0; j < 8; j++ ) {

                if ( ! (phys_bitmap->bitmap[i] & map) ) {

                    fprintf(stderr, "\t\tPhysical page %d found at bit %d with physical address %lu (Offset from start is %lu)\n", num_page + 1, j, page_addr, page_addr-start_phys_mem);

                    avail_pages[num_page++] = page_addr;
                    if ( num_page == num_pages ) {
                        fprintf(stderr, "\t%d physical pages found\n", num_page);
                        for ( int i = 0; i < num_pages; i++ ) {
                            fprintf(stderr, "\t\tPhysical page %d with physical address %lu (Offset from start is %lu)\n", i, avail_pages[i], avail_pages[i]-start_phys_mem);
                            set_bitmap(phys_bitmap, avail_pages[i], 1);
                        }
                        return avail_pages;
                    }
                    
                }

                page_addr += PGSIZE;
                map <<= 1;

                fprintf(stderr, "\t\tPage address updated to %lu (Offset from start is now %lu)\n", page_addr, page_addr-start_phys_mem);

            }
        } 

    }

    fprintf(stderr, "\tCannot find sufficient number of free physical pages\n");

    return NULL;
}

unsigned long get_next_cont(int num_pages) {

    fprintf(stderr, "get_next_cont() called requesting %d contiguous physical pages\n", num_pages);

    unsigned long page_addr = start_phys_mem;
    unsigned long start_addr = start_phys_mem;

    int num_page = 0;

    for ( int i = 0; i < phys_bitmap->map_length; i++ ) {

        if ( phys_bitmap->bitmap[i] == 255 ) {
            page_addr += (PGSIZE*8); 
            fprintf(stderr, "\tNo physical pages found at index %d | Page address updated to %lu (Offset from start is now %lu)\n", i, page_addr, page_addr-start_phys_mem);

        }

        else {

            fprintf(stderr, "\tPhysical page located within index %d | Bitmap at this index: %x", i, phys_bitmap->bitmap[i]);

            unsigned char map = 1;
            for ( int j = 0; j < 8; j++ ) {

                if ( ! (phys_bitmap->bitmap[i] & map) ) {

                    num_page++;
                    fprintf(stderr, "\t\tPhysical page %d found at bit %d with physical address %lu (Offset from start is %lu)\n", num_page, j, page_addr, page_addr-start_phys_mem);

                    if ( num_page == 1 ) {
                        start_addr = page_addr;
                        fprintf(stderr, "\t\t\tStarting address set to %lu", page_addr);
                    }

                    if ( num_page == num_pages ) {
                        fprintf(stderr, "\t%d contiguous physical pages found\n", num_page);
                        for ( int i = 0; i < num_pages; i++ ) {
                            fprintf(stderr, "\t\tPhysical page %d with physical address %lu (Offset from start is %lu)\n", i, start_addr+(i*PGSIZE), start_addr+(i*PGSIZE)-start_phys_mem);
                            set_bitmap(phys_bitmap, start_addr + (i*PGSIZE), 1);
                        }
                        return start_addr;
                    }

                } else {
                    if ( num_page ){
                        num_page = 0;
                        fprintf(stderr, "\tNot enough contiguous pages found: Counter reset\n");
                    }
                }

                page_addr += PGSIZE;
                map <<= 1;

                fprintf(stderr, "\t\tPage address updated to %lu (Offset from start is now %lu)\n", page_addr, page_addr-start_phys_mem);

            }
        } 

    }

    fprintf(stderr, "\tCannot find sufficient number of contiguous free physical pages\n");

    return 0;
}

unsigned long get_next_vpn(int num_pages){

    fprintf(stderr, "get_next_vpn() called requesting %d contiguous virtual pages\n", num_pages);

    unsigned long temp_vpn = 0;
    unsigned long vpn = 0;
    int counter = 0;

    for ( int i = 0; i < NUM_PAGES/8; i++ ) {

        if ( virt_bitmap->bitmap[i] == 255 ) {
            temp_vpn += (PGSIZE * 8); 
            fprintf(stderr, "\tNo virtual pages found at index %d | Virtual page address updated to %lu\n", i, temp_vpn);
        }
        else {

            fprintf(stderr, "\tVirtual page located within index %d | Bitmap at this index: %x", i, virt_bitmap->bitmap[i]);

            unsigned char map = 1;
            for ( int j = 0; j < 8; j++ ) {

                if ( ! (virt_bitmap->bitmap[i] & map) ) {
                    counter++;
                    fprintf(stderr, "\t\tVirtual page %d found at bit %d with virtual address %lu\n", counter, j, temp_vpn);
                    if ( counter == 1 ) {
                        vpn = temp_vpn;
                        fprintf(stderr, "\t\t\tStarting address set to %lu", vpn);
                    }
                    if ( counter == num_pages ) {
                        fprintf(stderr, "\t%d contiguous virtual pages found\n", counter);
                        for ( int i = 0; i < num_pages; i++ ) {
                            fprintf(stderr, "\t\tVirtual page %d with virtual address %lu \n", i, vpn+(i*PGSIZE));                           
                            set_bitmap(virt_bitmap, vpn + (i*PGSIZE), 1);
                        }
                        return vpn;
                    }
                } else {
                    if ( counter ) {
                        counter = 0;
                        fprintf(stderr, "\t\t\tNot enough contiguous pages found: Counter reset\n");

                    }
                    
                }

                fprintf(stderr, "\t\tVirtual address updated to %lu\n", temp_vpn);

                temp_vpn += PGSIZE;
                map <<= 1;
            }
        } 

    }

    fprintf(stderr, "\tCannot find sufficient number of contiguous free virtual pages\n");


    return 1;

}

/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /* 
    * HINT: If the physical memory is not yet initialized, then allocate and initialize.
    */

    fprintf(stderr, "t_malloc() called requesting %lu bytes\n", num_bytes);

    if ( pgdir == NULL ) {
        fprintf(stderr, "\tPage directory has not been allocated: Calling set_phys_mem()\n");
        if ( set_physical_mem() == -1 ) return NULL;
        fprintf(stderr, "\tPage directory has been allocated: Calling set_bitmap()\n");
        set_bitmap(phys_bitmap, start_phys_mem, 1);
    }
/* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */

    int num_pages = (num_bytes+PGSIZE-1)/PGSIZE;
    fprintf(stderr, "\t%d physical pages must be allocated\n", num_pages);

    fprintf(stderr, "\tCalling get_next_avail()\n");
    unsigned long *pfns = get_next_avail(num_pages);
    if ( !pfns ) return NULL;

    fprintf(stderr, "\tCalling get_next_vpn()\n");
    unsigned long vpn = get_next_vpn(num_pages);
    if ( vpn == 1 ) return NULL;


    for ( int i = 0; i < num_pages; i++) {
        fprintf(stderr, "\tCalling page_map() to map virtual address %lu to physical address %lu\n", vpn+(PGSIZE*i), pfns[i]);
        if ( page_map(vpn + (PGSIZE*i), pfns[i]) == -1 ) return NULL;
    }

    /*if ( num_pages > 1 ) {
        mpnode_t *new = (mpnode_t *)malloc(sizeof(mpnode_t));
        new->next = mp_list;
        new->num_pages = num_pages;
        new->start_addr = vpn;
        mp_list = new;
    }*/

    return (void *) vpn;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void t_free(void *va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
    * (va). Also mark the pages free in the bitmap. Perform free only if the 
    * memory from "va" to va+size is valid.

    *
    * Part 2: Also, remove the translation from the TLB
    */

   fprintf(stderr, "t_free() called on virtual address %lu with size %d\n", va, size);

   unsigned long virt_addr = (unsigned long) va;

   if ( virt_addr % PGSIZE != 0 ) {
        fprintf(stderr, "\tFree failed: Virtual address lies in the middle of a page\n");
        return;   // Ensure address to be freed is the start of a page
   }
   
   int num_pages = (size+PGSIZE-1)/PGSIZE;
    fprintf(stderr, "\tAttempting to free %d pages\n", num_pages);

   for ( int i = 0; i < num_pages; i++ ) {
        if ( get_bitmap(virt_bitmap, virt_addr + (i*PGSIZE)) != 1) {
            fprintf(stderr, "\tPage with virtual address %lu is not in use: Free failed\n", virt_addr + (i*PGSIZE));
            return;  // Ensure all pages being freed are currently in use
        }
   }
   fprintf(stderr, "\tAll virtual addresses are in use\n");

    fprintf(stderr, "\tTranslating virtual addresses to physical addresses:\n");
   for ( int i = 0; i < num_pages; i++ ) {
        unsigned long pa = translate(virt_addr + (i*PGSIZE));
        fprintf(stderr, "\t\tTranslated virtual address %lu to physical address %lu\n", virt_addr + (i*PGSIZE), pa);
        set_bitmap(phys_bitmap, pa, 0);     // Set all physical pages as free in physical bitmap
   }

    fprintf(stderr, "\tMapping virtual addresses to NULL\n");

   for ( int i = 0; i < num_pages; i++ ) {
        page_map(virt_addr + (i*PGSIZE), 0);    // Map all pages being freed to NULL
        set_bitmap(virt_bitmap, virt_addr + (i*PGSIZE), 0);     // Set all virtual pages as free in virtual bitmap
   }


}


/* The function copies data pointed by "val" to physical
* memory pages using virtual address (va)
* The function returns 0 if the put is successfull and -1 otherwise.
*/
int put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
    * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
    * than one page. Therefore, you may have to find multiple pages using translate()
    * function.
    */

   unsigned long virt_addr = (unsigned long)va;

   while ( size > 0 ) {

        void *pa = (void *)translate(virt_addr);

        unsigned long offset = virt_addr & offmask;
        int bytes_left_in_page = PGSIZE - offset;

        if ( size > bytes_left_in_page ) {
            memcpy(pa, val, bytes_left_in_page);
            size -= bytes_left_in_page;
            virt_addr += bytes_left_in_page;
            val += bytes_left_in_page;
        } else {
            memcpy(pa, val, size);
            return 0;
        }

   }

    /*return -1 if put_value failed and 0 if put is successfull*/

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */

   unsigned long virt_addr = (unsigned long)va;

   while ( size > 0 ) {

        void *pa = (void *)translate(virt_addr);

        unsigned long offset = virt_addr & offmask;
        int bytes_left_in_page = PGSIZE - offset;

        if ( size > bytes_left_in_page ) {
            memcpy(val, pa, bytes_left_in_page);
            size -= bytes_left_in_page;
            virt_addr += bytes_left_in_page;
            val += bytes_left_in_page;
        } else {
            memcpy(val, pa, size);
            return;
        }

   }


}


/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
    * matrix accessed. Similar to the code in test.c, you will use get_value() to
    * load each element and perform multiplication. Take a look at test.c! In addition to 
    * getting the values from two matrices, you will perform multiplication and 
    * store the result to the "answer array"
    */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_value( (void *)address_a, &a, sizeof(int));
                get_value( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}

void set_bitmap(bitmap_t *bitmap, unsigned long addr, int value){
    
    addr = addr >> offbits;

    int bitmap_index = addr/8;
    int bitmap_offset = addr%8;
    if ( bitmap_index >= bitmap->map_length ) return;

    unsigned char map = 1 << bitmap_offset;

    if ( value ) bitmap->bitmap[bitmap_index] |= map;
    else bitmap->bitmap[bitmap_index] &= (~map);

}

int get_bitmap(bitmap_t *bitmap, unsigned long addr){

    addr = addr >> offbits;

    int bitmap_index = addr/8;
    int bitmap_offset = addr%8;
    if ( bitmap_index >= bitmap->map_length ) return -1;

    unsigned char map = 1 << bitmap_offset;

    return (map &= bitmap->bitmap[bitmap_index]) >> bitmap_offset;

}

int exp_2(int power){
    int result = 1;
    for ( int i = 0; i < power; i++ ) result *= 2;
    return result;
}