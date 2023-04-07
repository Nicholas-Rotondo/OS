#include "my_vm.h"

unsigned long start_phys_mem;
pde_t *pg_dir = NULL;
unsigned char *phys_bitmap = NULL, *virt_bitmap = NULL;
int pdbits, pdmask, ptbits, ptmask, offbits, offmask;

/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    pg_dir = (void *)malloc(MEMSIZE);
    start_phys_mem = pg_dir;

    offbits = log2(PGSIZE);
    pdbits = (ADDR_BITS - offbits)/2;
    ptbits = ((ADDR_BITS - offbits)%2) ? pdbits + 1 : pdbits;

    offmask = PGSIZE - 1;
    ptmask = ((int)pow(2, ptbits) - 1) << offbits;
    pdmask = (((int)pow(2, pdbits) - 1) << offbits ) << ptbits;

    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them

    phys_bitmap = (unsigned char *)malloc(NUM_PAGES/8);
    virt_bitmap = (unsigned char *)malloc(((int)pow(2, ptbits)*(PGSIZE/4))/8);


}


/*
* Part 2: Add a virtual to physical page translation to the TLB.
* Feel free to extend the function arguments or return type.
*/
int
add_TLB(void *va, void *pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */

    return -1;
}


/*
* Part 2: Check TLB for a valid translation.
* Returns the physical page address.
* Feel free to extend this function and change the return type.
*/
pte_t *
check_TLB(void *va) {

    /* Part 2: TLB lookup code here */



/*This function should return a pte_t pointer*/
}


/*
* Part 2: Print TLB miss rate.
* Feel free to extend the function arguments or return type.
*/
void
print_TLB_missrate()
{
    double miss_rate = 0;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/




    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */
    unsigned long virt_addr = va;

    unsigned long offset = virt_addr & offmask;
    unsigned long pt_index = (virt_addr & ptmask) >> offset;
    unsigned long pd_index = ((virt_addr & pdmask) >> ptbits) >> offset;

    pte_t *pt_addr = pgdir[pd_index];
    pte_t *phys_addr = pt_addr[pt_index];

    //If translation not successful, then return NULL
    return NULL; 
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    unsigned long virt_addr = va;

    unsigned long offset = virt_addr & offmask;
    unsigned long pt_index = (virt_addr & ptmask) >> offset;
    unsigned long pd_index = ((virt_addr & pdmask) >> ptbits) >> offset;

    pte_t *pt_addr = pgdir[pd_index];

    if ( pt_addr == NULL ) {
        unsigned long next_page_table = get_next_cont(((int)pow(2, ptbits))/(PGSIZE/4));
        if ( next_page_table == 0 ) return -1;
        pt_addr = next_page_table;
    }

    void *pfn_addr = pt_addr[pt_index];

    if ( ! pfn_addr ) {
        pfn_addr = pa;
    } else {
        return -1;
    }

    return -1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {

    unsigned long *avail_pages = (unsigned long *)malloc(num_pages * sizeof(unsigned long));
    unsigned long page_addr = start_phys_mem;

    int num_page = 0;

    for ( int i = 0; i < NUM_PAGES/8; i++ ) {

        if ( phys_bitmap[i] == 255 ) page_addr += (PGSIZE * 8); 

        else {
            unsigned char map = 1;
            for ( int j = 0; j < 8; j++ ) {

                if ( ! (phys_bitmap[i] & map) ) {

                    avail_pages[num_page++] = page_addr;
                    phys_bitmap[i] |= map;
                    if ( num_page == num_pages ) return avail_pages;
                    
                }
                page_addr += PGSIZE;
                map <<= 1;
            }
        } 

    }
}

unsigned long get_next_cont(int num_pages) {

    unsigned long page_addr = start_phys_mem;
    unsigned long start_addr = start_phys_mem;
    int num_page = 0;

    for ( int i = 0; i < NUM_PAGES/8; i++ ) {

        if ( phys_bitmap[i] == 255 ) page_addr += 8; 

        else {
            unsigned char map = 1;
            for ( int j = 0; j < 8; j++ ) {

                if ( ! (phys_bitmap[i] & map) ) {
                    if ( num_page == 1 ) start_addr = page_addr;
                    if ( num_page == num_pages ) goto found_cont;
                } else num_page = 0;
                page_addr++;
                map <<= 1;
            }
        } 

    }

    if ( num_page != num_pages ) return NULL;

    found_cont:

    for ( int i = 0; i < num_pages; i++ ) {
        unsigned long temp = start_addr + i;
        unsigned long bitmap_index = temp / 8;
        unsigned long bitmap_offset = temp % 8;

        unsigned char map = 1 << bitmap_offset;

        virt_bitmap[bitmap_index] |= map;
    }

    return start_addr << offbits;
}

unsigned long get_next_vpn(int num_pages){

    unsigned long temp_vpn = 0;
    unsigned long vpn = 0;
    int counter = 0;

    for ( int i = 0; i < NUM_PAGES/8; i++ ) {

        if ( virt_bitmap[i] == 255 ) vpn ++; 

        else {
            unsigned char map = 1;
            for ( int j = 0; j < 8; j++ ) {

                if ( ! (virt_bitmap[i] & map) ) {
                    vpn = temp_vpn;
                    if ( ++counter == num_pages ) goto found_vpn;
                } else {
                    counter = 0;
                }
                vpn++;
                map <<= 1;
            }
        } 

    }

    found_vpn:

    for ( int i = 0; i < num_pages; i++ ) {
        unsigned long temp = vpn + i;
        unsigned long bitmap_index = temp / 8;
        unsigned long bitmap_offset = temp % 8;

        unsigned char map = 1 << bitmap_offset;

        virt_bitmap[bitmap_index] |= map;
    }

    return vpn << offbits;



}

/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /* 
    * HINT: If the physical memory is not yet initialized, then allocate and initialize.
    */

    if ( pg_dir == NULL ) {
        set_physical_mem();
        phys_bitmap[0] |= 1;
    }
/* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */

    int num_pages = (num_bytes+PGSIZE-1)/PGSIZE;

    unsigned long *pfns = get_next_avail(num_pages);
    unsigned long vpn = get_next_vpn(num_pages);

    for ( int i = 0; i < num_pages; i++) {
        if (page_map(pg_dir, vpn + (PGSIZE*i), pfns[i]) == -1) return -1;
    }

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


    /*return -1 if put_value failed and 0 if put is successfull*/

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */


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



