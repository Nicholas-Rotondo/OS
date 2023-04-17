#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 4096
#define BITS_FOR_OFFSET (log2(PGSIZE))

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024

// Size of "physcial memory"
#define MEMSIZE 1024*1024*1024

// Number of bits in an address
#define ADDR_BITS 32

// Number of pages in memory
#define PAGES_IN_MEM ((MEMSIZE)/(PGSIZE))

// Number of page table entries in a page
#define PTE_PER_PAGE ((PGSIZE)/(4))

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_ENTRIES 512

//Structure to represents TLB
typedef struct tlb {
    /*Assume your TLB is a direct mapped TLB with number of entries as TLB_ENTRIES
    * Think about the size of each TLB entry that performs virtual to physical
    * address translation.
    */
    unsigned long pa;
    unsigned long vpn;

}tlb_t;

typedef struct bitmap{

    unsigned char *bitmap;
    unsigned int map_length, map_size;

}bitmap_t;

int set_physical_mem();
void *t_malloc(unsigned int num_bytes);
unsigned long *get_next_avail(int num_pages);
unsigned long get_next_vpn(int num_pages);
int page_map(unsigned long va, unsigned long pa);
unsigned long get_next_cont(int num_pages);
void t_free(void *va, int size);
int pt_empty(unsigned long pd_index);
unsigned long translate(unsigned long va);
void add_TLB(unsigned long va, unsigned long pa);
unsigned long check_TLB(unsigned long va);
void clear_TLB(unsigned long va);
void print_TLB_missrate();
int put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void set_bitmap(bitmap_t *bitmap, unsigned long addr, int value);
int get_bitmap(bitmap_t *bitmap, unsigned long addr);
int exp_2(int power);

#endif
