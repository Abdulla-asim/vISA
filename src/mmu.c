#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/isa.h"

/* Translate virtual address to physical address using page tables */
uint32_t mmu_translate_address(process_t* process, uint32_t virtual_addr) {
    if (virtual_addr >= VIRT_MEMORY_SIZE) {
        fprintf(stderr, "Error: Virtual address 0x%X out of bounds\n", virtual_addr);
        return 0xFFFFFFFF;  /* Invalid address */
    }

    /* Extract page and offset */
    uint32_t page_num = virtual_addr / PAGE_SIZE;
    uint32_t offset = virtual_addr % PAGE_SIZE;

    if (page_num >= PAGES_PER_PROCESS) {
        fprintf(stderr, "Error: Page number %u out of bounds\n", page_num);
        return 0xFFFFFFFF;
    }

    page_table_entry_t* pte = &process->page_table[page_num];

    if (!pte->present) {
        fprintf(stderr, "Error: Page fault at virtual address 0x%X (page %u not present)\n", 
                virtual_addr, page_num);
        return 0xFFFFFFFF;  /* Page fault */
    }

    /* Mark page as accessed */
    pte->accessed = true;

    /* Calculate physical address */
    uint32_t physical_page = pte->physical_page;
    uint32_t physical_addr = (physical_page * PAGE_SIZE) + offset;

    return physical_addr;
}

/* Load a page from disk (stub for now) */
void mmu_load_page(vm_t* vm, process_t* process, uint32_t virtual_page) {
    if (virtual_page >= PAGES_PER_PROCESS) {
        fprintf(stderr, "Error: Invalid page number\n");
        return;
    }

    /* Find a free physical page */
    uint32_t free_page = 0;
    bool found = false;

    for (uint32_t i = 0; i < (MEMORY_SIZE / PAGE_SIZE); i++) {
        if (!(vm->memory.page_bitmap & (1 << i))) {
            free_page = i;
            found = true;
            vm->memory.page_bitmap |= (1 << i);  /* Mark as used */
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "Error: No free physical pages\n");
        return;
    }

    /* Set up page table entry */
    process->page_table[virtual_page].physical_page = free_page;
    process->page_table[virtual_page].present = true;
    process->page_table[virtual_page].writable = true;

    printf("Loaded page %u to physical page %u\n", virtual_page, free_page);
}

/* Dump page table for debugging */
void mmu_dump_page_table(process_t* process) {
    printf("\n=== Page Table for PID %u ===\n", process->pid);
    int pages_shown = 0;
    for (uint32_t i = 0; i < PAGES_PER_PROCESS && pages_shown < 16; i++) {
        page_table_entry_t* pte = &process->page_table[i];
        if (pte->present) {
            printf("Virtual Page %3u -> Physical Page %2u (RW: %d, A: %d, D: %d)\n",
                   i, pte->physical_page, pte->writable, pte->accessed, pte->dirty);
            pages_shown++;
        }
    }
}

/* Initialize page table for a new process */
void mmu_init_page_table(vm_t* vm, process_t* process, uint32_t entry_point) {
    memset(process->page_table, 0, sizeof(process->page_table));

    /* Allocate initial page for code */
    mmu_load_page(vm, process, 0);

    /* Allocate stack space (grow downward from end of virtual address space) */
    uint32_t stack_page = PAGES_PER_PROCESS - 1;
    mmu_load_page(vm, process, stack_page);

    /* Set up initial register state */
    memset(process->registers, 0, sizeof(process->registers));
    process->pc = entry_point;
    process->sp = VIRT_MEMORY_SIZE - 1;  /* Stack at end of virtual space */
    process->fp = process->sp;
}
