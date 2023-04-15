#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
#define NUM_PAGES ((MEMSIZE)/(PGSIZE))

#define PTE_PER_PAGE ((PGSIZE)/(4))

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_ENTRIES 512

//Structure to represents TLB
struct tlb {
    /*Assume your TLB is a direct mapped TLB with number of entries as TLB_ENTRIES
    * Think about the size of each TLB entry that performs virtual to physical
    * address translation.
    */
    void *phys_addr = NULL;
    void *virt_addr = NULL;
    unsigned long tag = virt_addr/PGSIZE;
}tlb_t;

typedef struct bitmap{

    unsigned char *bitmap;
    unsigned int map_length, map_size;

}bitmap_t;

typedef struct mpnode {

    struct mbnode *next;
    unsigned long start_addr;
    unsigned int num_pages;

} mpnode_t;

int set_physical_mem();
unsigned long translate(unsigned long va);
int page_map(unsigned long va, unsigned long pa);
unsigned long *get_next_avail(int num_pages);
unsigned long get_next_cont(int num_pages);
unsigned long get_next_vpn(int num_pages);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *t_malloc(unsigned int num_bytes);
void t_free(void *va, int size);
int put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();
void set_bitmap(bitmap_t *bitmap, unsigned long addr, int value);
int get_bitmap(bitmap_t *bitmap, unsigned long addr);
void initialize_page_directory();
int exp_2(int power);

#endif
