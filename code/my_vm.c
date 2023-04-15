#include "my_vm.h"

unsigned long start_phys_mem;

pde_t *pgdir = NULL;
mpnode_t *mp_list = NULL;
bitmap_t *phys_bitmap = NULL, *virt_bitmap = NULL;

int pdbits, pdmask, ptbits, ptmask, offbits, offmask;
tlb_store = NULL;
queue *tlb_list = NULL;

int hits = 0;
int misses = 0;

int eviction_count = 0;

tlb_arr = (tlb * )malloc(sizeof(tlb * TLB_ENTRIES));
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

    pgdir = (pde_t *)calloc(MEMSIZE, 1);
    if ( pgdir == NULL ) return -1;

    start_phys_mem = (unsigned long) pgdir;

    offbits = log2(PGSIZE);
    pdbits = (ADDR_BITS - offbits)/2;
    ptbits = ((ADDR_BITS - offbits)%2) ? pdbits + 1 : pdbits;

    offmask = PGSIZE - 1;
    ptmask = (exp_2(ptbits) - 1) << offbits;
    pdmask = ((exp_2(pdbits) - 1) << offbits ) << ptbits;

    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them

    phys_bitmap = (bitmap_t *)calloc(sizeof(bitmap_t));
    if ( phys_bitmap == NULL ) return -1;

    phys_bitmap->map_size = NUM_PAGES;
    phys_bitmap->map_length = NUM_PAGES/8;
    phys_bitmap->bitmap = (unsigned char *)malloc(phys_bitmap->map_length);
    if ( phys_bitmap->bitmap == NULL ) return -1;

    virt_bitmap = (bitmap_t *)calloc(sizeof(bitmap_t));
    if ( virt_bitmap == NULL ) return -1;

    virt_bitmap->map_size = (PGSIZE/4) * exp_2(ptbits);
    virt_bitmap->map_length = virt_bitmap->map_size/8; 
    virt_bitmap->bitmap = (unsigned char *)malloc(virt_bitmap->map_length);
    if ( virt_bitmap->bitmap == NULL ) return -1;

    
    return 0;

}


/*
* Part 2: Add a virtual to physical page translation to the TLB.
* Feel free to extend the function arguments or return type.
*/
int
add_TLB(void *va, void *pa)
{
    
    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    // we need to find a way to store the age of a tlb
    // meaning, it is important to remove the oldest TLB in the event of an eviction.
    // for now just keep this simple implementation and add donce we have an idea
    tlb_store = (tlb *)malloc(sizeof(tlb)); 
    if(tlb_store == NULL) {
        fprintf(stderr, "Memory allocation failed");
        return 1;
    }  
    tlb_store->index = -1;
    tlb_store->virt_addr = va;
    tlb_store->phys_addr = pa;

    int i;

    for(i = 0; i < TLB_ENTRIES; i++) {
        if(tlb_arr[0] == NULL) {
            tlb_arr[0] = tlb_store;
        }
        if(tlb_arr[0] != NULL) {
            //check the array and see if there is an open slot
            if(tlb_arr[i] == NULL) {
                tlb_arr[i] = tlb_store;
            }
        }
    }

    return 0;
}


/*
* Part 2: Check TLB for a valid translation.
* Returns the physical page address.
* Feel free to extend this function and change the return type.
*/
*/
pte_t *
check_TLB(void *va) {

    /* Part 2: TLB lookup code here */
    pte *ret_phys_addr = NULL;
    tlb *tlb_store = malloc(sizeof(tlb));
    if(tlb_store == NULL) {
        fprintf(stderr, "Memory allocation failed");
        return 1;
    } 
    
    unsigned long vpn = (unsigned long)va/PGSIZE;

    if(vpn == tlb_arr[vpn%TLB_ENTRIES]->tag) {
        ret_phys_addr = tlb_store->phys_addr;
        hits += 1;
    }
    else {
        misses += 1;
        // we return NULL here so we can check this in our translate function.
        // maybe we just do the actual translation here?  
        return NULL;
    }

    free(tlb_store);
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

    unsigned long offset = va & offmask;
    unsigned long pt_index = (va & ptmask) >> offset;
    unsigned long pd_index = ((va & pdmask) >> ptbits) >> offset;
     if ( pd_index >= PGSIZE/4 ) return 0;

    pte_t *pgtable = (pte_t *)pgdir[pd_index];
    if ( pgtable == NULL ) return 0;

    unsigned long pfn = pgtable[pt_index];

    if ( pfn == 0 ) return 0;
    else return ( pfn << offbits ) | offset;

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

    unsigned long offset = va & offmask;
    unsigned long pt_index = (va & ptmask) >> offset;
    unsigned long pd_index = ((va & pdmask) >> ptbits) >> offset;
    if ( pd_index >= PGSIZE/4 ) return -1;

    if ( pgdir[pd_index] == 0 ) {
        unsigned long next_page_table = get_next_cont((exp_2(ptbits))/(PGSIZE/4));
        if ( next_page_table == 0 ) return -1;
        pgdir[pd_index] = next_page_table;
    }

    pte_t *pgtable = (pte_t *)pgdir[pd_index];

    if ( ! pgtable[pt_index] ) {
        pgtable[pt_index] = pa;
    } else {
        if ( pa == 0 ) {
            pgtable[pt_index] = pa;
        } else {
            return -1;
        }
    }

    return 0;
}


/*Function that gets the next available page
*/
unsigned long *get_next_avail(int num_pages) {

    unsigned long *avail_pages = (unsigned long *)malloc(num_pages * sizeof(unsigned long));
    unsigned long page_addr = start_phys_mem;

    int num_page = 0;

    for ( int i = 0; i < phys_bitmap->map_length; i++ ) {

        if ( phys_bitmap->bitmap[i] == 255 ) page_addr += (PGSIZE * 8); 

        else {

            unsigned char map = 1;
            for ( int j = 0; j < 8; j++ ) {

                if ( ! (phys_bitmap->bitmap[i] & map) ) {

                    avail_pages[num_page++] = page_addr;
                    if ( num_page == num_pages ) {
                        for ( int i = 0; i < num_pages; i++ ) set_bitmap(phys_bitmap, avail_pages[i], 1);
                        return avail_pages;
                    }
                    
                }

                page_addr += PGSIZE;
                map <<= 1;
            }
        } 

    }

    return NULL;
}

unsigned long get_next_cont(int num_pages) {

    unsigned long page_addr = start_phys_mem;
    unsigned long start_addr = start_phys_mem;

    int num_page = 0;

    for ( int i = 0; i < phys_bitmap->map_length; i++ ) {

        if ( phys_bitmap->bitmap[i] == 255 ) page_addr += (PGSIZE*8); 

        else {
            unsigned char map = 1;
            for ( int j = 0; j < 8; j++ ) {

                if ( ! (phys_bitmap->bitmap[i] & map) ) {
                    if ( num_page == 1 ) start_addr = page_addr;
                    if ( num_page == num_pages ) {
                        for ( int i = 0; i < num_pages; i++ ) set_bitmap(phys_bitmap, start_addr + (i*PGSIZE), 1);
                        return start_addr;
                    }
                } else num_page = 0;

                page_addr += PGSIZE;
                map <<= 1;
            }
        } 

    }

    return 1;
}

unsigned long get_next_vpn(int num_pages){

    unsigned long temp_vpn = 0;
    unsigned long vpn = 0;
    int counter = 0;

    for ( int i = 0; i < NUM_PAGES/8; i++ ) {

        if ( virt_bitmap->bitmap[i] == 255 ) temp_vpn += (PGSIZE * 8); 

        else {

            unsigned char map = 1;
            for ( int j = 0; j < 8; j++ ) {

                if ( ! (virt_bitmap->bitmap[i] & map) ) {
                    if ( counter == 0 ) vpn = temp_vpn;
                    counter++;
                    if ( counter == num_pages ) {
                        for ( int i = 0; i < num_pages; i++ ) set_bitmap(virt_bitmap, vpn + (i*PGSIZE), 1);
                        return vpn;
                    }
                } else {
                    counter = 0;
                }

                temp_vpn += PGSIZE;
                map <<= 1;
            }
        } 

    }

    return 1;

}

/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /* 
    * HINT: If the physical memory is not yet initialized, then allocate and initialize.
    */

    if ( pgdir == NULL ) {
        if ( set_physical_mem() == -1 ) return NULL;
        phys_bitmap->bitmap[0] |= 1;
    }
/* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */

    int num_pages = (num_bytes+PGSIZE-1)/PGSIZE;

    unsigned long *pfns = get_next_avail(num_pages);
    if ( !pfns ) return NULL;

    unsigned long vpn = get_next_vpn(num_pages);
    if ( vpn == 1 ) return NULL;

    for ( int i = 0; i < num_pages; i++) {
        if ( page_map(vpn + (PGSIZE*i), pfns[i]) == -1 ) return NULL;
    }

    /*if ( num_pages > 1 ) {
        mpnode_t *new = (mpnode_t *)malloc(sizeof(mpnode_t));
        new->next = mp_list;
        new->num_pages = num_pages;
        new->start_addr = vpn;
        mp_list = new;
    }*/

    return NULL;
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

   unsigned long virt_addr = (unsigned long) va;

   if ( virt_addr % PGSIZE != 0 ) return;   // Ensure address to be freed is the start of a page
    
   int num_pages = (size+PGSIZE-1)/PGSIZE;

   for ( int i = 0; i < num_pages; i++ ) {
        if ( get_bitmap(virt_bitmap, virt_addr + (i*PGSIZE)) != 1) return;  // Ensure all pages being freed are currently in use
   }

   for ( int i = 0; i < num_pages; i++ ) {
        unsigned long pa = translate(virt_addr + (i*PGSIZE));
        set_bitmap(phys_bitmap, pa, 0);     // Set all physical pages as free in physical bitmap
   }

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