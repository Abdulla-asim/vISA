#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/isa.h"

/* ============ HYPERVISOR INITIALIZATION ============ */
hypervisor_t* hypervisor_create(void) {
    hypervisor_t* hv = (hypervisor_t*)malloc(sizeof(hypervisor_t));
    if (!hv) return NULL;

    memset(hv->host_memory, 0, sizeof(hv->host_memory));
    memset(hv->guests, 0, sizeof(hv->guests));
    hv->mode = MODE_HOST;
    hv->current_guest_id = 0;
    hv->guest_count = 0;
    hv->tick_count = 0;
    hv->halted = false;

    printf("[HYPERVISOR] Initialized with %d MB host memory\n", MEMORY_SIZE / (1024 * 1024));
    return hv;
}

void hypervisor_destroy(hypervisor_t* hv) {
    if (hv) free(hv);
}

/* ============ GUEST VM CREATION ============ */
uint32_t hypervisor_create_guest(hypervisor_t* hv, const char* guest_image) {
    if (hv->guest_count >= MAX_GUESTS) {
        fprintf(stderr, "[HYPERVISOR] Maximum guests reached\n");
        return 0;
    }

    uint32_t guest_id = hv->guest_count++;
    guest_vm_t* guest = &hv->guests[guest_id];

    guest->vm_id = guest_id;
    guest->state = GUEST_STOPPED;
    guest->instruction_count = 0;

    /* Initialize vCPU */
    guest->vcpu.guest_id = guest_id;
    guest->vcpu.state = GUEST_STOPPED;
    memset(guest->vcpu.registers, 0, sizeof(guest->vcpu.registers));
    guest->vcpu.pc = 0;
    guest->vcpu.sp = GUEST_PHYS_MEMORY_SIZE - 1;
    guest->vcpu.priv = PRIV_USER;

    /* Initialize guest memory */
    memset(guest->guest_memory, 0, sizeof(guest->guest_memory));
    memset(guest->ept, 0, sizeof(guest->ept));

    /* Load guest image */
    FILE* file = fopen(guest_image, "rb");
    if (!file) {
        fprintf(stderr, "[HYPERVISOR] Failed to load guest image: %s\n", guest_image);
        return 0;
    }

    size_t bytes_read = fread(guest->guest_memory, 1, GUEST_PHYS_MEMORY_SIZE, file);
    fclose(file);

    if (bytes_read == 0) {
        fprintf(stderr, "[HYPERVISOR] Guest image is empty\n");
        return 0;
    }

    printf("[HYPERVISOR] Created Guest VM %u (loaded %zu bytes)\n", guest_id, bytes_read);
    return guest_id + 1;  /* Return 1-based ID */
}

/* ============ GUEST MEMORY TRANSLATION ============ */
/* Guest Virtual → Guest Physical (guest's job) */
uint32_t guest_translate_address(guest_vm_t* guest, uint32_t guest_virt_addr) {
    if (guest_virt_addr >= GUEST_VIRT_MEMORY_SIZE) {
        fprintf(stderr, "[GUEST %u] Virtual address 0x%X out of bounds\n", 
                guest->vm_id, guest_virt_addr);
        return 0xFFFFFFFF;
    }

    uint32_t page_num = guest_virt_addr / PAGE_SIZE;
    uint32_t offset = guest_virt_addr % PAGE_SIZE;

    guest_page_table_entry_t* pte = &guest->vcpu.guest_page_table[page_num];

    if (!pte->present) {
        fprintf(stderr, "[GUEST %u] Page fault at virt 0x%X (page %u not present)\n", 
                guest->vm_id, guest_virt_addr, page_num);
        return 0xFFFFFFFF;
    }

    pte->accessed = true;
    uint32_t guest_phys_addr = (pte->guest_physical_page * PAGE_SIZE) + offset;

    return guest_phys_addr;
}

/* Guest Physical → Host Physical (hypervisor's job) */
uint32_t host_translate_address(hypervisor_t* hv, uint32_t guest_phys_addr) {
    /* For now, simple 1:1 mapping with offset */
    return guest_phys_addr;  /* Direct mapping */
}

/* ============ VM ENTRY (Host → Guest) ============ */
void vmentry(hypervisor_t* hv, guest_vm_t* guest) {
    hv->mode = MODE_GUEST;
    guest->vcpu.state = GUEST_RUNNING;
    printf("[VMENTRY] Entering Guest VM %u at PC=0x%X\n", guest->vm_id, guest->vcpu.pc);
}

/* ============ VM EXIT (Guest → Host) ============ */
void vmexit(hypervisor_t* hv, guest_vm_t* guest, vmexit_reason_t reason) {
    hv->mode = MODE_HOST;
    guest->vcpu.state = GUEST_BLOCKED;
    guest->vcpu.exit_reason = reason;
    printf("[VMEXIT] Guest VM %u exited: reason=0x%X\n", guest->vm_id, reason);
}

/* ============ VM EXIT HANDLING ============ */
int handle_vmexit(hypervisor_t* hv, guest_vm_t* guest) {
    vmexit_reason_t reason = guest->vcpu.exit_reason;

    switch (reason) {
        case VMEXIT_INVALID_INSTRUCTION:
            printf("[HYPERVISOR] Guest %u: Invalid instruction\n", guest->vm_id);
            return -1;

        case VMEXIT_PRIVILEGED_INSTRUCTION:
            printf("[HYPERVISOR] Guest %u: Attempted privileged operation\n", guest->vm_id);
            return -1;

        case VMEXIT_SYSCALL:
            printf("[HYPERVISOR] Guest %u: System call\n", guest->vm_id);
            break;

        case VMEXIT_HYPERCALL:
            printf("[HYPERVISOR] Guest %u: Hypercall (request to hypervisor)\n", guest->vm_id);
            handle_hypercall(hv, guest, guest->vcpu.exit_data);
            break;

        case VMEXIT_PAGE_FAULT:
            printf("[HYPERVISOR] Guest %u: Page fault - setting up EPT\n", guest->vm_id);
            /* Hypervisor handles paging - could swap from disk, etc. */
            break;

        case VMEXIT_IO_INSTRUCTION:
            printf("[HYPERVISOR] Guest %u: I/O operation\n", guest->vm_id);
            break;

        case VMEXIT_HALT:
            printf("[HYPERVISOR] Guest %u: Halted\n", guest->vm_id);
            guest->vcpu.state = GUEST_STOPPED;
            return 0;

        default:
            fprintf(stderr, "[HYPERVISOR] Unknown exit reason: 0x%X\n", reason);
            return -1;
    }

    return 0;  /* Continue guest */
}

/* ============ HYPERCALL HANDLING ============ */
void handle_hypercall(hypervisor_t* hv, guest_vm_t* guest, hypercall_number_t call) {
    switch (call) {
        case HYPERCALL_PRINT:
            printf("[GUEST %u] ", guest->vm_id);
            guest_print(hv, "[printing from guest]");
            break;

        case HYPERCALL_EXIT:
            printf("[HYPERVISOR] Guest %u requesting exit\n", guest->vm_id);
            guest->vcpu.state = GUEST_STOPPED;
            break;

        default:
            printf("[HYPERVISOR] Unknown hypercall: %u\n", call);
    }
}

/* ============ GUEST I/O ============ */
void guest_print(hypervisor_t* hv, const char* msg) {
    printf("%s\n", msg);
}

/* ============ GUEST EXECUTION ============ */
void hypervisor_run_guest(hypervisor_t* hv, uint32_t guest_id) {
    if (guest_id == 0 || guest_id > hv->guest_count) {
        fprintf(stderr, "[HYPERVISOR] Invalid guest ID\n");
        return;
    }

    guest_vm_t* guest = &hv->guests[guest_id - 1];

    printf("\n[HYPERVISOR] Starting Guest VM %u\n", guest->vm_id);
    printf("=========================================\n\n");

    const int TIME_SLICE = 10000;  /* Instructions per time slice */
    int total_instructions = 0;

    while (guest->vcpu.state == GUEST_RUNNING) {
        vmentry(hv, guest);

        /* Execute guest time slice */
        for (int i = 0; i < TIME_SLICE && guest->vcpu.state == GUEST_RUNNING; i++) {
            /* Simulate instruction fetch from guest memory */
            uint32_t guest_virt_addr = guest->vcpu.pc;
            uint32_t guest_phys_addr = guest_translate_address(guest, guest_virt_addr);

            if (guest_phys_addr == 0xFFFFFFFF) {
                vmexit(hv, guest, VMEXIT_PAGE_FAULT);
                break;
            }

            /* Fetch instruction */
            instruction_t instr;
            instr.opcode = guest->guest_memory[guest_phys_addr];
            instr.rd = guest->guest_memory[guest_phys_addr + 1];
            instr.rs1 = guest->guest_memory[guest_phys_addr + 2];
            instr.rs2 = guest->guest_memory[guest_phys_addr + 3];

            guest->vcpu.pc += INSTRUCTION_SIZE;

            /* Execute instruction */
            switch (instr.opcode) {
                case OP_ADD:
                    guest->vcpu.registers[instr.rd] = 
                        guest->vcpu.registers[instr.rs1] + guest->vcpu.registers[instr.rs2];
                    break;

                case OP_SUB:
                    guest->vcpu.registers[instr.rd] = 
                        guest->vcpu.registers[instr.rs1] - guest->vcpu.registers[instr.rs2];
                    break;

                case OP_HYPERCALL:
                    guest->vcpu.exit_data = instr.rd;  /* Hypercall number in rd */
                    vmexit(hv, guest, VMEXIT_HYPERCALL);
                    break;

                case OP_HALT:
                    vmexit(hv, guest, VMEXIT_HALT);
                    break;

                default:
                    vmexit(hv, guest, VMEXIT_INVALID_INSTRUCTION);
                    break;
            }

            total_instructions++;
        }

        /* Handle VM exit */
        if (guest->vcpu.state == GUEST_BLOCKED) {
            if (handle_vmexit(hv, guest) != 0) {
                break;  /* Fatal error */
            }
            guest->vcpu.state = GUEST_RUNNING;  /* Resume */
        }
    }

    printf("\n=========================================\n");
    printf("[HYPERVISOR] Guest VM %u stopped after %d instructions\n\n", 
           guest->vm_id, total_instructions);
}

void hypervisor_pause_guest(hypervisor_t* hv, uint32_t guest_id) {
    if (guest_id > 0 && guest_id <= hv->guest_count) {
        hv->guests[guest_id - 1].vcpu.state = GUEST_PAUSED;
        printf("[HYPERVISOR] Paused Guest VM %u\n", guest_id);
    }
}

void hypervisor_stop_guest(hypervisor_t* hv, uint32_t guest_id) {
    if (guest_id > 0 && guest_id <= hv->guest_count) {
        hv->guests[guest_id - 1].vcpu.state = GUEST_STOPPED;
        printf("[HYPERVISOR] Stopped Guest VM %u\n", guest_id);
    }
}

/* ============ DEBUGGING ============ */
void hypervisor_dump_state(hypervisor_t* hv) {
    printf("\n[HYPERVISOR STATE]\n");
    printf("Mode: %s\n", hv->mode == MODE_HOST ? "HOST" : "GUEST");
    printf("Guests: %u/%u\n", hv->guest_count, MAX_GUESTS);
    printf("Ticks: %u\n", hv->tick_count);

    for (uint32_t i = 0; i < hv->guest_count; i++) {
        guest_dump_state(&hv->guests[i]);
    }
}

void guest_dump_state(guest_vm_t* guest) {
    printf("\n  [GUEST %u STATE]\n", guest->vm_id);
    printf("  State: %d\n", guest->vcpu.state);
    printf("  PC: 0x%08X\n", guest->vcpu.pc);
    printf("  SP: 0x%08X\n", guest->vcpu.sp);
    printf("  Priv: %s\n", guest->vcpu.priv == PRIV_KERNEL ? "KERNEL" : "USER");
    printf("  Instructions: %u\n", guest->instruction_count);
}
