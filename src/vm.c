#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/isa.h"

/* Forward declarations */
void mmu_init_page_table(vm_t* vm, process_t* process, uint32_t entry_point);
uint32_t mmu_translate_address(process_t* process, uint32_t virtual_addr);

/* Initialize VM */
vm_t* vm_create(void) {
    vm_t* vm = (vm_t*)malloc(sizeof(vm_t));
    if (!vm) return NULL;

    memset(&vm->memory, 0, sizeof(vm->memory));
    memset(vm->processes, 0, sizeof(vm->processes));
    vm->current_pid = 0;
    vm->next_pid = 1;
    vm->current_priv = PRIV_KERNEL;
    vm->tick_count = 0;
    vm->halted = false;

    return vm;
}

void vm_destroy(vm_t* vm) {
    if (vm) free(vm);
}

/* Load program into process virtual memory */
static int vm_load_program_into_process(vm_t* vm, process_t* process, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return -1;
    }

    /* Read program into physical memory */
    size_t bytes_read = fread(vm->memory.pages, 1, PAGE_SIZE, file);
    fclose(file);

    if (bytes_read == 0) {
        fprintf(stderr, "Error: No program data loaded\n");
        return -1;
    }

    printf("Loaded %zu bytes (PID %u)\n", bytes_read, process->pid);
    return 0;
}

/* Create a new process */
uint32_t vm_create_process(vm_t* vm, const char* filename) {
    if (vm->next_pid >= MAX_PROCESSES) {
        fprintf(stderr, "Error: Maximum processes reached\n");
        return 0;
    }

    uint32_t pid = vm->next_pid++;
    process_t* process = &vm->processes[pid];

    process->pid = pid;
    process->state = PROC_READY;
    process->priv = PRIV_USER;
    process->entry_point = 0;
    process->exit_code = 0;

    /* Initialize memory and page tables */
    mmu_init_page_table(vm, process, process->entry_point);

    /* Load program */
    if (vm_load_program_into_process(vm, process, filename) != 0) {
        return 0;
    }

    printf("Created process PID %u\n", pid);
    return pid;
}

/* Context switch to next runnable process */
void vm_schedule_next(vm_t* vm) {
    int next_pid = -1;

    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (vm->processes[i].state == PROC_READY) {
            next_pid = i;
            break;
        }
    }

    if (next_pid >= 0) {
        vm->current_pid = next_pid;
        vm->processes[next_pid].state = PROC_RUNNING;
    } else {
        vm->halted = true;
    }
}

/* Execute single instruction */
static void vm_execute_instruction(vm_t* vm) {
    process_t* proc = &vm->processes[vm->current_pid];

    /* Fetch instruction from virtual address */
    uint32_t phys_addr = mmu_translate_address(proc, proc->pc);
    if (phys_addr == 0xFFFFFFFF) {
        fprintf(stderr, "Page fault at PC\n");
        proc->state = PROC_BLOCKED;
        return;
    }

    instruction_t instr;
    instr.opcode = vm->memory.pages[phys_addr];
    instr.rd = vm->memory.pages[phys_addr + 1];
    instr.rs1 = vm->memory.pages[phys_addr + 2];
    instr.rs2 = vm->memory.pages[phys_addr + 3];

    proc->pc += INSTRUCTION_SIZE;

    /* Execute instruction */
    switch (instr.opcode) {
        case OP_ADD:
            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT && instr.rs2 < REGISTER_COUNT) {
                proc->registers[instr.rd] = proc->registers[instr.rs1] + proc->registers[instr.rs2];
            }
            break;

        case OP_SUB:
            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT && instr.rs2 < REGISTER_COUNT) {
                proc->registers[instr.rd] = proc->registers[instr.rs1] - proc->registers[instr.rs2];
            }
            break;

        case OP_MUL:
            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT && instr.rs2 < REGISTER_COUNT) {
                proc->registers[instr.rd] = proc->registers[instr.rs1] * proc->registers[instr.rs2];
            }
            break;

        case OP_DIV:
            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT && instr.rs2 < REGISTER_COUNT) {
                if (proc->registers[instr.rs2] != 0) {
                    proc->registers[instr.rd] = proc->registers[instr.rs1] / proc->registers[instr.rs2];
                } else {
                    fprintf(stderr, "Error: Division by zero (PID %u)\n", proc->pid);
                    vm_handle_interrupt(vm, IRQ_DIVIDE_BY_ZERO, 0);
                }
            }
            break;

        case OP_MOV:
            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT) {
                proc->registers[instr.rd] = proc->registers[instr.rs1];
            }
            break;

        case OP_SYSCALL:
            vm_handle_interrupt(vm, IRQ_SYSCALL, instr.rd);  /* Syscall number in rd */
            break;

        case OP_HALT:
            proc->state = PROC_TERMINATED;
            break;

        default:
            fprintf(stderr, "Error: Unknown opcode 0x%02X at PC=0x%X (PID %u)\n", 
                    instr.opcode, proc->pc, proc->pid);
            proc->state = PROC_BLOCKED;
    }
}

/* Execute program with context switching */
void vm_run(vm_t* vm) {
    vm_schedule_next(vm);  /* Start first ready process */

    int instruction_count = 0;
    const int MAX_INSTRUCTIONS = 10000000;
    const int TIME_SLICE = 1000;  /* Instructions per time slice */

    while (!vm->halted && instruction_count < MAX_INSTRUCTIONS) {
        process_t* proc = &vm->processes[vm->current_pid];

        if (proc->state != PROC_RUNNING) {
            vm_schedule_next(vm);
            continue;
        }

        /* Execute time slice */
        for (int i = 0; i < TIME_SLICE && proc->state == PROC_RUNNING; i++) {
            vm_execute_instruction(vm);
            instruction_count++;
        }

        /* Schedule next process (round-robin) */
        if (proc->state == PROC_RUNNING) {
            proc->state = PROC_READY;
        }
        vm_schedule_next(vm);
    }

    printf("Executed %d instructions\n", instruction_count);
}

/* Handle interrupts and system calls */
void vm_handle_interrupt(vm_t* vm, interrupt_type_t irq, uint32_t data) {
    process_t* proc = &vm->processes[vm->current_pid];

    switch (irq) {
        case IRQ_SYSCALL:
            if (data == SYSCALL_EXIT) {
                proc->state = PROC_TERMINATED;
                printf("Process %u exited with code %u\n", proc->pid, proc->registers[0]);
            } else if (data == SYSCALL_WRITE) {
                printf("[PID %u] WRITE: %u\n", proc->pid, proc->registers[1]);
            }
            break;

        case IRQ_DIVIDE_BY_ZERO:
            printf("Divide by zero exception (PID %u)\n", proc->pid);
            proc->state = PROC_TERMINATED;
            break;

        case IRQ_PAGE_FAULT:
            printf("Page fault (PID %u)\n", proc->pid);
            proc->state = PROC_BLOCKED;
            break;

        default:
            printf("Unhandled interrupt 0x%X\n", irq);
    }
}

/* Dump VM state for debugging */
void vm_dump_state(vm_t* vm) {
    printf("\n=== VM STATE ===\n");
    printf("Current PID: %u\n", vm->current_pid);
    printf("Tick Count: %u\n", vm->tick_count);

    printf("\n=== PROCESSES ===\n");
    for (uint32_t i = 0; i < vm->next_pid; i++) {
        process_t* p = &vm->processes[i];
        printf("PID %u: State=%d, PC=0x%08X, SP=0x%08X, Exit=%u\n",
               p->pid, p->state, p->pc, p->sp, p->exit_code);
    }
}
