#include "my_vm.h"

unsigned long start_phys_mem;

pde_t *pgdir = NULL;
bitmap_t *phys_bitmap = NULL, *virt_bitmap = NULL;

int pdbits, pdmask, ptbits, ptmask, offbits, offmask;

int hits = 0;
int misses = 0;

char lock = 0;

int set_physical_mem() {

    pgdir = (pde_t *)calloc(MEMSIZE, 1);
    if ( pgdir == NULL ) return -1;

    start_phys_mem = (unsigned long) pgdir;

    offbits = log2(PGSIZE);
    pdbits = (ADDR_BITS - offbits)/2;
    ptbits = ((ADDR_BITS - offbits)%2) ? pdbits + 1 : pdbits;

    offmask = PGSIZE - 1;
    ptmask = (exp_2(ptbits) - 1) << offbits;
    pdmask = ((exp_2(pdbits) - 1) << offbits ) << ptbits;

    phys_bitmap = (bitmap_t *)malloc(sizeof(bitmap_t));
    if ( phys_bitmap == NULL ) return -1;

    phys_bitmap->map_size = NUM_PAGES;
    phys_bitmap->map_length = NUM_PAGES/8;
    phys_bitmap->bitmap = (unsigned char *)calloc(phys_bitmap->map_length, sizeof(unsigned char));
    if ( phys_bitmap->bitmap == NULL ) return -1;

    virt_bitmap = (bitmap_t *)malloc(sizeof(bitmap_t));
    if ( virt_bitmap == NULL ) return -1;

    virt_bitmap->map_size = (PGSIZE/4) * exp_2(ptbits);
    virt_bitmap->map_length = virt_bitmap->map_size/8; 
    virt_bitmap->bitmap = (unsigned char *)calloc(virt_bitmap->map_length, sizeof(unsigned char));
    if ( virt_bitmap->bitmap == NULL ) return -1;
   
   
    tlb_arr = (tlb_t *)calloc(TLB_ENTRIES, sizeof(tlb_t));
    if ( tlb_arr == NULL ) return -1;

    return 0;

}

void add_TLB(unsigned long va, unsigned long pa) {
    
    unsigned long vpn = va >> offbits;

    tlb_arr[vpn % TLB_ENTRIES].vpn = vpn;
    tlb_arr[vpn % TLB_ENTRIES].pa = pa;

    misses++;

}   

unsigned long check_TLB(unsigned long va) {

    unsigned long vpn = va >> offbits;
    unsigned long offset = va & offmask;

    if ( vpn == tlb_arr[vpn % TLB_ENTRIES].vpn ) {
        hits++;
        return tlb_arr[vpn % TLB_ENTRIES].pa;
    } else return 0;

}

void print_TLB_missrate() {	

    double miss_rate = ((double)misses)/((double)(hits+misses));

    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);

}

unsigned long translate(unsigned long va) {

    unsigned long offset = va & offmask;

    unsigned long pa = check_TLB(va);
    if( pa ) return pa + offset;

    unsigned long pt_index = (va & ptmask) >> offbits;
    unsigned long pd_index = ((va & pdmask) >> ptbits) >> offbits;

    if ( pd_index >= PGSIZE/4 ) return 0;
    
    pte_t *pgtable = (pte_t *)pgdir[pd_index];
    if ( pgtable == NULL ) return 0;
    
    pa = pgtable[pt_index];
    
    if ( pa == 0 ) return 0;
    else {
        add_TLB(va, pa);
        return  pa + offset;
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
    unsigned long pt_index = (va & ptmask) >> offbits;
    unsigned long pd_index = ((va & pdmask) >> ptbits) >> offbits;

    fprintf(stderr, "page_map(): \tVirtual address was translated to page directory index %lu, page table index %lu, and offset %lu\n", pd_index, pt_index, offset);

    if ( pd_index >= PGSIZE/4 ) {
        fprintf(stderr, "page_map(): \tPage directory index out of bounds\n");
        return -1;
    }

    if ( pgdir[pd_index] == 0 ) {
        fprintf(stderr, "page_map(): \tNo page table found at index %lu: Calling get_next_cont() to allocate page table\n", pd_index);
        int pages_for_pagetable = (exp_2(ptbits)+(PGSIZE/4)-1)/(PGSIZE/4);
        unsigned long next_page_table = get_next_cont(pages_for_pagetable);
        if ( next_page_table == 0 ) {
            fprintf(stderr, "page_map(): \t\tPage table could not be allocated\n");
            return -1;
        }
        fprintf(stderr, "page_map(): \t\tPage table allocated with physical address %lu\n", next_page_table);
        pgdir[pd_index] = next_page_table;
    }

    pte_t *pgtable = (pte_t *)pgdir[pd_index];
    fprintf(stderr, "page_map(): \tPage directory indexed: Jumping to page table at physical address %lu\n", pgdir[pd_index]);

    if ( ! pgtable[pt_index] ) {
        pgtable[pt_index] = pa;
        fprintf(stderr, "page_map(): \t\tPage table indexed: Mapped physical address %lu to index %lu in page table\n", pgtable[pt_index], pt_index);
        add_TLB(va, pa);
    } else {
        if ( pa == 0 ) {
            pgtable[pt_index] = pa;
        } else {
            fprintf(stderr, "page_map(): \t\tPage table indexed: Page map failed, as physical address %lu resides at index %lu in page table\n", pgtable[pd_index], pt_index);
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
            fprintf(stderr, "get_next_avail(): \tNo physical pages found at index %d | Page address updated to %lu (Offset from start is now %lu)\n", i, page_addr, page_addr-start_phys_mem);
        }

        else {

            fprintf(stderr, "get_next_avail(): \tPhysical page located within index %d | Bitmap at this index: %x\n", i, phys_bitmap->bitmap[i]);

            unsigned char map = 1;

            for ( int j = 0; j < 8; j++ ) {

                if ( ! (phys_bitmap->bitmap[i] & map) ) {

                    fprintf(stderr, "get_next_avail(): \t\tPhysical page %d found at bit %d with physical address %lu (Offset from start is %lu)\n", num_page + 1, j, page_addr, page_addr-start_phys_mem);

                    avail_pages[num_page++] = page_addr;
                    if ( num_page == num_pages ) {
                        fprintf(stderr, "get_next_avail(): \t%d physical pages found\n", num_page);
                        for ( int i = 0; i < num_pages; i++ ) {
                            fprintf(stderr, "get_next_avail(): \t\tPhysical page %d with physical address %lu (Offset from start is %lu)\n", i, avail_pages[i], avail_pages[i]-start_phys_mem);
                            set_bitmap(phys_bitmap, avail_pages[i] - start_phys_mem, 1);
                        }
                        return avail_pages;
                    }
                    
                }

                page_addr += PGSIZE;
                map <<= 1;

                fprintf(stderr, "get_next_avail(): \t\tPage address updated to %lu (Offset from start is now %lu)\n", page_addr, page_addr-start_phys_mem);

            }
        } 

    }

    fprintf(stderr, "get_next_avail(): \tCannot find sufficient number of free physical pages\n");

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
            fprintf(stderr, "get_next_cont(): \tNo physical pages found at index %d | Page address updated to %lu (Offset from start is now %lu)\n", i, page_addr, page_addr-start_phys_mem);

        }

        else {

            fprintf(stderr, "get_next_cont(): \tPhysical page located within index %d | Bitmap at this index: %x\n", i, phys_bitmap->bitmap[i]);

            unsigned char map = 1;
            for ( int j = 0; j < 8; j++ ) {

                if ( ! (phys_bitmap->bitmap[i] & map) ) {

                    num_page++;
                    fprintf(stderr, "get_next_cont(): \t\tPhysical page %d of %d found at bit %d with physical address %lu (Offset from start is %lu)\n", num_page, num_pages, j, page_addr, page_addr-start_phys_mem);

                    if ( num_page == 1 ) {
                        start_addr = page_addr;
                        fprintf(stderr, "get_next_cont(): \t\t\tStarting address set to %lu\n", page_addr);
                    }

                    if ( num_page == num_pages ) {
                        fprintf(stderr, "get_next_cont(): \t%d contiguous physical pages found\n", num_page);
                        for ( int i = 0; i < num_pages; i++ ) {
                            fprintf(stderr, "get_next_cont(): \t\tPhysical page %d with physical address %lu (Offset from start is %lu)\n", i, start_addr+(i*PGSIZE), start_addr+(i*PGSIZE)-start_phys_mem);
                            set_bitmap(phys_bitmap, start_addr + (i*PGSIZE) - start_phys_mem, 1);
                        }
                        return start_addr;
                    }

                } else {
                    if ( num_page ){
                        num_page = 0;
                        fprintf(stderr, "get_next_cont(): \tNot enough contiguous pages found: Counter reset\n");
                    }
                }

                page_addr += PGSIZE;
                map <<= 1;

                fprintf(stderr, "get_next_cont(): \t\tPage address updated to %lu (Offset from start is now %lu)\n", page_addr, page_addr-start_phys_mem);

            }
        } 

    }

    fprintf(stderr, "get_next_cont(): \tCannot find sufficient number of contiguous free physical pages\n");

    return 0;
}

unsigned long get_next_vpn(int num_pages){

    fprintf(stderr, "get_next_vpn() called requesting %d contiguous virtual pages\n", num_pages);

    unsigned long temp_vpn = 0;
    unsigned long vpn = 0;
    int counter = 0;

    for ( int i = 0; i < NUM_PAGES/8; i++ ) {

        if ( virt_bitmap->bitmap[i] == 255 ) {
            temp_vpn += (8 * PGSIZE); 
            fprintf(stderr, "get_next_vpn(): \tNo virtual pages found at index %d | Virtual address updated to %lu\n", i, temp_vpn);
        }
        else {

            fprintf(stderr, "get_next_vpn(): \tVirtual page located within index %d | Bitmap at this index: %x\n", i, virt_bitmap->bitmap[i]);

            unsigned char map = 1;
            for ( int j = 0; j < 8; j++ ) {

                if ( ! (virt_bitmap->bitmap[i] & map) ) {
                    counter++;
                    fprintf(stderr, "get_next_vpn(): \t\tVirtual page %d found at bit %d with virtual address %lu\n", counter, j, temp_vpn);
                    if ( counter == 1 ) {
                        vpn = temp_vpn;
                        fprintf(stderr, "get_next_vpn(): \t\t\tStarting address set to %lu\n", vpn);
                    }
                    if ( counter == num_pages ) {
                        fprintf(stderr, "get_next_vpn(): \t%d contiguous virtual pages found\n", counter);
                        for ( int i = 0; i < num_pages; i++ ) {
                            fprintf(stderr, "get_next_vpn(): \t\tVirtual page %d with virtual address %lu \n", i, vpn+(i*PGSIZE));                           
                            set_bitmap(virt_bitmap, vpn + (i*PGSIZE), 1);
                        }
                        return vpn;
                    }
                } else {
                    if ( counter ) {
                        counter = 0;
                        fprintf(stderr, "get_next_vpn(): \t\t\tNot enough contiguous pages found: Counter reset\n");

                    }
                    
                }

                fprintf(stderr, "get_next_vpn(): \t\tVirtual address updated to %lu\n", temp_vpn);

                temp_vpn += PGSIZE;
                map <<= 1;
            }
        } 

    }

    fprintf(stderr, "get_next_vpn(): \tCannot find sufficient number of contiguous free virtual pages\n");


    return 1;

}

/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /* 
    * HINT: If the physical memory is not yet initialized, then allocate and initialize.
    */

   while ( __atomic_test_and_set(&lock, __ATOMIC_SEQ_CST) == 1 ); 

    fprintf(stderr, "t_malloc() called requesting %u bytes\n", num_bytes);

    if ( pgdir == NULL ) {
        fprintf(stderr, "t_malloc(): \tPage directory has not been allocated: Calling set_phys_mem()\n");
        if ( set_physical_mem() == -1 ) {
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return NULL;
        }
        fprintf(stderr, "t_malloc(): \tPage directory has been allocated: Calling set_bitmap()\n");
        set_bitmap(phys_bitmap, start_phys_mem - start_phys_mem, 1);
    }
/* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */

    int num_pages = (num_bytes+PGSIZE-1)/PGSIZE;
    fprintf(stderr, "t_malloc(): \t%d physical pages must be allocated\n", num_pages);

    fprintf(stderr, "t_malloc(): \tCalling get_next_avail()\n");
    unsigned long *pfns = get_next_avail(num_pages);
    if ( !pfns ) {
        __atomic_clear(&lock, __ATOMIC_SEQ_CST);
        return NULL;
    }

    fprintf(stderr, "t_malloc(): \tCalling get_next_vpn()\n");
    unsigned long vpn = get_next_vpn(num_pages);
    fprintf(stderr, "t_malloc(): \tVirtual address received: %lu\n", vpn);
    if ( vpn == 1 ) {
        __atomic_clear(&lock, __ATOMIC_SEQ_CST);
        return NULL;
    }

    for ( int i = 0; i < num_pages; i++) {
        fprintf(stderr, "t_malloc(): \tCalling page_map() to map virtual address %lu to physical address %lu\n", vpn+(PGSIZE*i), pfns[i]);
        if ( page_map(vpn + (PGSIZE*i), pfns[i]) == -1 ) {
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return NULL;
        }
        add_TLB(vpn + (PGSIZE*i), pfns[i]);
    }

    __atomic_clear(&lock, __ATOMIC_SEQ_CST);

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

   while ( __atomic_test_and_set(&lock, __ATOMIC_SEQ_CST) );

   fprintf(stderr, "t_free() called on virtual address %lu with size %d\n", (unsigned long)va, size);

   unsigned long virt_addr = (unsigned long) va;

   if ( virt_addr % PGSIZE != 0 ) {
        fprintf(stderr, "t_free(): \tFree failed: Virtual address lies in the middle of a page\n");
        __atomic_clear(&lock, __ATOMIC_SEQ_CST);
        return;   // Ensure address to be freed is the start of a page
   }
   
   int num_pages = (size+PGSIZE-1)/PGSIZE;
     fprintf(stderr, "t_free(): \tAttempting to free %d pages\n", num_pages);

   for ( int i = 0; i < num_pages; i++ ) {
        if ( get_bitmap(virt_bitmap, virt_addr + (i*PGSIZE)) != 1) {
            fprintf(stderr, "t_free(): \tPage with virtual address %lu is not in use: Free failed\n", virt_addr + (i*PGSIZE));
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return;  // Ensure all pages being freed are currently in use
        }
   }
   fprintf(stderr, "t_free(): \tAll virtual addresses are in use\n");

    fprintf(stderr, "t_free(): \tTranslating virtual addresses to physical addresses:\n");
   for ( int i = 0; i < num_pages; i++ ) {
        unsigned long pa = translate(virt_addr + (i*PGSIZE));
        fprintf(stderr, "t_free(): \t\tTranslated virtual address %lu to physical address %lu, will search in bitmap for %lu\n", virt_addr + (i*PGSIZE), pa, pa - start_phys_mem);
        set_bitmap(phys_bitmap, pa - start_phys_mem, 0);     // Set all physical pages as free in physical bitmap
   }

    fprintf(stderr, "t_free(): \tMapping virtual addresses to NULL\n");

   for ( int i = 0; i < num_pages; i++ ) {
        page_map(virt_addr + (i*PGSIZE), 0);    // Map all pages being freed to NULL
        set_bitmap(virt_bitmap, virt_addr + (i*PGSIZE), 0);     // Set all virtual pages as free in virtual bitmap
        if ( check_TLB(virt_addr + (i*PGSIZE)) ) add_TLB(virt_addr + (i*PGSIZE), 0);
   }

   unsigned long pd_index = ((virt_addr & pdmask) >> ptbits) >> offbits;

    fprintf(stderr, "t_free(): \tChecking to see if page table at page directory index %lu can be freed\n", pd_index);

   if ( pt_empty(pd_index) ) {
        fprintf(stderr, "t_free(): \t\tPage table can be freed\n");
        int pages_for_pagetable = (exp_2(ptbits)+(PGSIZE/4)-1)/(PGSIZE/4);
        for ( int i = 0; i < pages_for_pagetable; i++ ) {
            set_bitmap(phys_bitmap, pgdir[pd_index] + (i*PGSIZE) - start_phys_mem, 0);
        }
        pgdir[pd_index] = 0;
   } else {
        fprintf(stderr, "t_free(): \t\tPage table cannot be freed\n");
   }

    __atomic_clear(&lock, __ATOMIC_SEQ_CST);

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

   while ( __atomic_test_and_set(&lock, __ATOMIC_SEQ_CST) == 1 );

   fprintf(stderr, "put_value() called to copy %d bits from address %lu to virtual address %lu\n", size, (unsigned long) val, (unsigned long) va);

   unsigned long virt_addr = (unsigned long)va;

   while ( size > 0 ) {

        void *pa = (void *)translate(virt_addr);
        fprintf(stderr, "put_value(): \tTranslated virtual address %lu to physical address %lu\n", virt_addr, (unsigned long) pa);

        unsigned long offset = virt_addr & offmask;
        int bytes_left_in_page = PGSIZE - offset;
        fprintf(stderr, "put_value(): \tAddress translated with offset of %lu | No more than %d bytes can be written to the physical page\n", offset, bytes_left_in_page);

        if ( size > bytes_left_in_page ) {
            fprintf(stderr, "put_value(): \tPerforming copy from %lu to %lu of %d bytes\n", (unsigned long) val, (unsigned long) pa, bytes_left_in_page);
            memcpy(pa, val, bytes_left_in_page);
            size -= bytes_left_in_page;
            virt_addr += bytes_left_in_page;
            val += bytes_left_in_page;
            fprintf(stderr, "put_value(): \t\tCopy performed: Virtual address updated to %lu, input address updated to %lu, and size updated to %d\n", virt_addr, (unsigned long) val, size);
        } else {
            fprintf(stderr, "put_value(): \tPerforming copy from %lu to %lu of %d bytes\n", (unsigned long) val, (unsigned long) pa, size);
            memcpy(pa, val, size);
            virt_addr += size;
            val += size;
            size -= size;
            fprintf(stderr, "put_value(): \t\tCopy performed: Virtual address updated to %lu, input address updated to %lu, and size updated to %d\n", virt_addr, (unsigned long) val, size);
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return 0;
        }

   }

    __atomic_clear(&lock, __ATOMIC_SEQ_CST);

    /*return -1 if put_value failed and 0 if put is successfull*/

    return -1;

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */

   while ( __atomic_test_and_set(&lock, __ATOMIC_SEQ_CST) == 1);

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
            virt_addr += size;
            val += size;
            size -= size;
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
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
                printf("Values at the index: %d, %d, %d, %d, %d\n", a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}

void set_bitmap(bitmap_t *bitmap, unsigned long addr, int value){
    
    addr = addr >> offbits;

    unsigned long bitmap_index = addr/8;
    unsigned long bitmap_offset = addr%8;

    if ( value == 0 ) {
        fprintf(stderr, "set_bitmap: \tShifted Address: %lu | Index: %lu | Offset: %lu\n", addr, bitmap_index, bitmap_offset);
    }

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

int pt_empty(unsigned long pd_index){

    int pages_for_pagetable = (exp_2(ptbits)+(PGSIZE/4)-1)/(PGSIZE/4);
    unsigned long start_virt_addr = pd_index << (ptbits + offbits);
    
    int i = exp_2(ptbits);
    unsigned long temp_addr = start_virt_addr;

    while ( i > 0 ){
        if ( i < 8 ) {
            if ( get_bitmap(virt_bitmap, temp_addr) != 0) return 0;
            temp_addr += PGSIZE;
            i--;
        } else {
            if ( temp_addr % (PGSIZE*8) == 0) {
                if(virt_bitmap->bitmap[temp_addr/(PGSIZE*8)] != 0 ) return 0;
                temp_addr += (8*PGSIZE);
                i -= 8;
            } else {
                if ( get_bitmap(virt_bitmap, temp_addr) != 0) return 0;
                temp_addr += PGSIZE;
                i--;
            }
        }
    }

    return 1;



}

void print_bitmaps(){

    for ( int i = 1; i < 1024; i++ ) {
        if (virt_bitmap->bitmap[i] != 0 || phys_bitmap->bitmap[i] != 0) {
            fprintf(stderr, "Virtual: %x | Physical: %x\n", virt_bitmap->bitmap[i], phys_bitmap->bitmap[i]);
        }
    }

}

int exp_2(int power){
    int result = 1;
    for ( int i = 0; i < power; i++ ) result *= 2;
    return result;
}