#include "my_vm.h"

unsigned long start_phys_mem;

pde_t *pgdir = NULL;
bitmap_t *phys_bitmap = NULL, *virt_bitmap = NULL;
tlb_t *tlb_arr;

int pdbits, pdmask, ptbits, ptmask, offbits, offmask, pages_for_pagetable;
int hits = 0, misses = 0;

char lock = 0;

int set_physical_mem() {

    pgdir = (pde_t *)calloc(MEMSIZE, 1);
    if ( pgdir == NULL ) return -1;

    start_phys_mem = (unsigned long) pgdir;

    offbits = log2(PGSIZE);
    pdbits = (ADDR_BITS - offbits) / 2;
    ptbits = ((ADDR_BITS - offbits) % 2) ? pdbits + 1 : pdbits;

    offmask = PGSIZE - 1;
    ptmask = (exp_2(ptbits) - 1) << offbits;
    pdmask = ((exp_2(pdbits) - 1) << offbits ) << ptbits;

    pages_for_pagetable = (exp_2(ptbits) + PTE_PER_PAGE - 1) / PTE_PER_PAGE;

    phys_bitmap = (bitmap_t *)malloc(sizeof(bitmap_t));
    if ( phys_bitmap == NULL ) return -1;

    phys_bitmap->map_size = PAGES_IN_MEM;
    phys_bitmap->map_length = phys_bitmap->map_size / 8;
    phys_bitmap->bitmap = (unsigned char *)calloc(phys_bitmap->map_length, sizeof(unsigned char));
    if ( phys_bitmap->bitmap == NULL ) return -1;

    virt_bitmap = (bitmap_t *)malloc(sizeof(bitmap_t));
    if ( virt_bitmap == NULL ) return -1;

    virt_bitmap->map_size = PAGES_IN_MEM;
    virt_bitmap->map_length = virt_bitmap->map_size / 8; 
    virt_bitmap->bitmap = (unsigned char *)calloc(virt_bitmap->map_length, sizeof(unsigned char));
    if ( virt_bitmap->bitmap == NULL ) return -1;
   
   
    tlb_arr = (tlb_t *)calloc(TLB_ENTRIES, sizeof(tlb_t));
    if ( tlb_arr == NULL ) return -1;

    return 0;

}

void *t_malloc(unsigned int num_bytes) {

   while ( __atomic_test_and_set(&lock, __ATOMIC_SEQ_CST) == 1 ); 

    if ( pgdir == NULL ) {

        if ( set_physical_mem() == -1 ) {
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return NULL;
        }
        set_bitmap(phys_bitmap, start_phys_mem - start_phys_mem, 1);
    
    }

    int num_pages = (num_bytes + PGSIZE - 1) / PGSIZE;

    unsigned long *pfns = get_next_avail(num_pages);
    if ( !pfns ) {
        __atomic_clear(&lock, __ATOMIC_SEQ_CST);
        return NULL;
    }

    unsigned long vpn = get_next_vpn(num_pages);
    if ( vpn == 1 ) {
        __atomic_clear(&lock, __ATOMIC_SEQ_CST);
        return NULL;
    }

    for ( int i = 0; i < num_pages; i++) {

        if ( page_map(vpn + (PGSIZE*i), pfns[i]) == -1 ) {
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return NULL;
        }

    }

    __atomic_clear(&lock, __ATOMIC_SEQ_CST);

    return (void *) vpn;
}

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
                        for ( int i = 0; i < num_pages; i++ ) set_bitmap(phys_bitmap, avail_pages[i] - start_phys_mem, 1);
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

unsigned long get_next_vpn(int num_pages){

    unsigned long temp_vpn = 0;
    unsigned long vpn = 0;
    int counter = 0;

    for ( int i = 0; i < virt_bitmap->map_length; i++ ) {

        if ( virt_bitmap->bitmap[i] == 255 ) temp_vpn += (8 * PGSIZE); 
        else {

            unsigned char map = 1;

            for ( int j = 0; j < 8; j++ ) {

                if ( ! (virt_bitmap->bitmap[i] & map) ) {

                    counter++;

                    if ( counter == 1 ) vpn = temp_vpn;
                    if ( counter == num_pages ) {
                        for ( int i = 0; i < num_pages; i++ ) set_bitmap(virt_bitmap, vpn + (i*PGSIZE), 1);
                        return vpn;
                    }

                } else counter = 0;

                temp_vpn += PGSIZE;
                map <<= 1;

            }
        } 
    }

    return 1;

}

int page_map(unsigned long va, unsigned long pa) {

    unsigned long offset = va & offmask;
    unsigned long pt_index = (va & ptmask) >> offbits;
    unsigned long pd_index = ((va & pdmask) >> ptbits) >> offbits;

    if ( pd_index >= PTE_PER_PAGE ) return -1;

    if ( pgdir[pd_index] == 0 ) {
        unsigned long next_page_table = get_next_cont(pages_for_pagetable);
        if ( next_page_table == 0 ) return -1;
        pgdir[pd_index] = next_page_table;
    }

    pte_t *pgtable = (pte_t *)pgdir[pd_index];

    if ( ! pgtable[pt_index] ) {
        pgtable[pt_index] = pa;
        add_TLB(va, pa);
    } else {
        if ( pa == 0 ) {
            pgtable[pt_index] = pa;
        } else return -1;
    }

    return 0;
}

unsigned long get_next_cont(int num_pages) {

    unsigned long page_addr = start_phys_mem;
    unsigned long start_addr = start_phys_mem;

    int num_page = 0;

    for ( int i = 0; i < phys_bitmap->map_length; i++ ) {

        if ( phys_bitmap->bitmap[i] == 255 ) {
            page_addr += (PGSIZE*8); 
        }

        else {

            unsigned char map = 1;

            for ( int j = 0; j < 8; j++ ) {

                if ( ! (phys_bitmap->bitmap[i] & map) ) {

                    num_page++;

                    if ( num_page == 1 ) start_addr = page_addr;

                    if ( num_page == num_pages ) {
                        for ( int i = 0; i < num_pages; i++ ) set_bitmap(phys_bitmap, start_addr + (i*PGSIZE) - start_phys_mem, 1);
                        return start_addr;
                    }

                } else num_page = 0;

                page_addr += PGSIZE;
                map <<= 1;

            }
        } 

    }

    return 0;
}

void t_free(void *va, int size) {

    if ( !pgdir ) return;

   while ( __atomic_test_and_set(&lock, __ATOMIC_SEQ_CST) == 1 );

   unsigned long virt_addr = (unsigned long) va;

   if ( virt_addr % PGSIZE != 0 ) {
        __atomic_clear(&lock, __ATOMIC_SEQ_CST);
        return;   
   }

   int num_pages = (size + PGSIZE - 1) / PGSIZE;

   for ( int i = 0; i < num_pages; i++ ) {

        if ( get_bitmap(virt_bitmap, virt_addr + (i*PGSIZE)) != 1) {
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return;  
        }

   }

   for ( int i = 0; i < num_pages; i++ ) {

        unsigned long pa = translate(virt_addr + (i*PGSIZE));
        if ( pa == 0 ) return;
        set_bitmap(phys_bitmap, pa - start_phys_mem, 0);  

   }


   for ( int i = 0; i < num_pages; i++ ) {

        page_map(virt_addr + (i*PGSIZE), 0);    
        set_bitmap(virt_bitmap, virt_addr + (i*PGSIZE), 0);     
        clear_TLB(virt_addr + (i*PGSIZE));

   }

   unsigned long init_pd_index = ((virt_addr & pdmask) >> ptbits) >> offbits;
   unsigned long end_pd_index = (((virt_addr + size) & pdmask) >> ptbits) >> offbits;

   for ( unsigned long i = init_pd_index; i <= end_pd_index; i++ ) {
        if ( pt_empty(i) ) {
            for ( int j = 0; j < pages_for_pagetable; j++ ) set_bitmap(phys_bitmap, pgdir[i] + (j*PGSIZE) - start_phys_mem, 0);
            pgdir[i] = 0;
        }
   }

    __atomic_clear(&lock, __ATOMIC_SEQ_CST);

    return;

}

int pt_empty(unsigned long pd_index){

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

unsigned long translate(unsigned long va) {

    unsigned long offset = va & offmask;

    unsigned long pa = check_TLB(va);
    if( pa ) return pa + offset;

    unsigned long pt_index = (va & ptmask) >> offbits;
    unsigned long pd_index = ((va & pdmask) >> ptbits) >> offbits;

    if ( pd_index >= PTE_PER_PAGE ) return 0;
    
    pte_t *pgtable = (pte_t *)pgdir[pd_index];
    if ( pgtable == NULL ) return 0;
    
    pa = pgtable[pt_index];
    
    if ( pa == 0 ) return 0;
    else {
        add_TLB(va, pa);
        return  pa + offset;
    }
}

void add_TLB(unsigned long va, unsigned long pa) {
    
    unsigned long vpn = va >> offbits;

    tlb_arr[vpn % TLB_ENTRIES].vpn = vpn;
    tlb_arr[vpn % TLB_ENTRIES].pa = pa;

    misses++;

}   

unsigned long check_TLB(unsigned long va) {

    unsigned long vpn = va >> offbits;

    if ( vpn == tlb_arr[vpn % TLB_ENTRIES].vpn ) {
        hits++;
        return tlb_arr[vpn % TLB_ENTRIES].pa;
    } else return 0;

}

void clear_TLB(unsigned long va) {

    unsigned long vpn = va >> offbits;

    if ( vpn == tlb_arr[vpn % TLB_ENTRIES].vpn ) {
        tlb_arr[vpn % TLB_ENTRIES].vpn = 0;
        tlb_arr[vpn % TLB_ENTRIES].pa = 0;
        return;
    }

}

void print_TLB_missrate() {	

    double miss_rate = ((double)misses)/((double)(hits+misses));

    fprintf(stderr, "TLB miss rate %lf\n", miss_rate);

}

int put_value(void *va, void *val, int size) {

    if ( !pgdir ) return -1;

    while ( __atomic_test_and_set(&lock, __ATOMIC_SEQ_CST) == 1 );

    unsigned long virt_addr = (unsigned long) va;

    while ( size > 0 ) {

        void *pa = (void *)translate(virt_addr);
        if ( pa == NULL ) {
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return -1;
        }

        unsigned long offset = virt_addr & offmask;
        int bytes_left_in_page = PGSIZE - offset;

        if ( size > bytes_left_in_page ) {

            memcpy(pa, val, bytes_left_in_page);
            size -= bytes_left_in_page;
            virt_addr += bytes_left_in_page;
            val += bytes_left_in_page;

        } else {

            memcpy(pa, val, size);
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return 0;
        }

    }

        __atomic_clear(&lock, __ATOMIC_SEQ_CST);

        return -1;

}

void get_value(void *va, void *val, int size) {

    if ( !pgdir ) return;

    while ( __atomic_test_and_set(&lock, __ATOMIC_SEQ_CST) == 1);

    unsigned long virt_addr = (unsigned long)va;

    while ( size > 0 ) {

        void *pa = (void *)translate(virt_addr);
        if ( pa == NULL ) {
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return;
        }

        unsigned long offset = virt_addr & offmask;
        int bytes_left_in_page = PGSIZE - offset;

        if ( size > bytes_left_in_page ) {

            memcpy(val, pa, bytes_left_in_page);
            size -= bytes_left_in_page;
            virt_addr += bytes_left_in_page;
            val += bytes_left_in_page;

        } else {

            memcpy(val, pa, size);
            __atomic_clear(&lock, __ATOMIC_SEQ_CST);
            return;

        }
    }
}

void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    if ( !pgdir ) return;

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
                //printf("Values at the index: %d, %d, %d, %d, %d\n", a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            //printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}

void set_bitmap(bitmap_t *bitmap, unsigned long addr, int value){
    
    addr = addr >> offbits;

    unsigned long bitmap_index = addr / 8;
    unsigned long bitmap_offset = addr % 8;

    if ( bitmap_index >= bitmap->map_length ) return;

    unsigned char map = 1 << bitmap_offset;

    if ( value ) bitmap->bitmap[bitmap_index] |= map;
    else bitmap->bitmap[bitmap_index] &= (~map);

}

int get_bitmap(bitmap_t *bitmap, unsigned long addr){

    addr = addr >> offbits;

    int bitmap_index = addr / 8;
    int bitmap_offset = addr % 8;
    if ( bitmap_index >= bitmap->map_length ) return -1;

    unsigned char map = 1 << bitmap_offset;

    return (map &= bitmap->bitmap[bitmap_index]) >> bitmap_offset;

}

int exp_2(int power){
    int result = 1;
    for ( int i = 0; i < power; i++ ) result *= 2;
    return result;
}