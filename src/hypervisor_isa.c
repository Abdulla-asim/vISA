#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/isa.h"

/* ============ VIRTUALIZATION ISA INSTRUCTION IMPLEMENTATIONS ============ */

/* VMENTER vmcs_ptr - Enter guest mode and start execution */
void isa_vmenter(hypervisor_t* hv, vmcs_t* vmcs) {
    if (!vmcs) {
        fprintf(stderr, "[ISA:VMENTER] Invalid VMCS pointer\n");
        return;
    }

    guest_vm_t* guest = NULL;
    for (uint32_t i = 0; i < hv->guest_count; i++) {
        if (&hv->guests[i].vcpu.vmcs == vmcs) {
            guest = &hv->guests[i];
            break;
        }
    }

    if (!guest) {
        fprintf(stderr, "[ISA:VMENTER] VMCS not associated with any guest\n");
        return;
    }

    /* Load guest state from VMCS */
    guest->vcpu.registers[0] = vmcs->guest_rax;
    guest->vcpu.registers[1] = vmcs->guest_rbx;
    guest->vcpu.registers[2] = vmcs->guest_rcx;
    guest->vcpu.registers[3] = vmcs->guest_rdx;
    guest->vcpu.pc = vmcs->guest_pc;
    guest->vcpu.priv = vmcs->guest_priv;
    guest->vcpu.guest_pgtbl_root = vmcs->guest_pgtbl_root;
    guest->vcpu.host_pgtbl_root = vmcs->host_pgtbl_root;

    /* Set trap configuration */
    guest->vcpu.vmcs.trap_config = vmcs->trap_config;

    /* Enter guest mode */
    hv->mode = MODE_GUEST;
    guest->vcpu.state = GUEST_RUNNING;
    hv->current_guest_id = guest->vm_id;

    printf("[ISA:VMENTER] Entered Guest VM %u (PC=0x%X, Trap Config=0x%X)\n", 
           guest->vm_id, guest->vcpu.pc, vmcs->trap_config);
}

/* VMRESUME vmcs_ptr - Resume guest after handling VMEXIT */
void isa_vmresume(hypervisor_t* hv, vmcs_t* vmcs) {
    if (!vmcs) {
        fprintf(stderr, "[ISA:VMRESUME] Invalid VMCS pointer\n");
        return;
    }

    guest_vm_t* guest = NULL;
    for (uint32_t i = 0; i < hv->guest_count; i++) {
        if (&hv->guests[i].vcpu.vmcs == vmcs) {
            guest = &hv->guests[i];
            break;
        }
    }

    if (!guest) {
        fprintf(stderr, "[ISA:VMRESUME] VMCS not associated with any guest\n");
        return;
    }

    /* Restore guest state from VMCS */
    guest->vcpu.registers[0] = vmcs->guest_rax;
    guest->vcpu.registers[1] = vmcs->guest_rbx;
    guest->vcpu.pc = vmcs->guest_pc;
    guest->vcpu.priv = vmcs->guest_priv;

    /* Re-enter guest mode */
    hv->mode = MODE_GUEST;
    guest->vcpu.state = GUEST_RUNNING;

    printf("[ISA:VMRESUME] Resumed Guest VM %u (PC=0x%X)\n", 
           guest->vm_id, guest->vcpu.pc);
}

/* VMCAUSE rd - Read exit cause */
uint32_t isa_vmcause(hypervisor_t* hv) {
    if (hv->mode == MODE_HOST && hv->current_guest_id < hv->guest_count) {
        guest_vm_t* guest = &hv->guests[hv->current_guest_id];
        printf("[ISA:VMCAUSE] Exit cause: 0x%X (%s)\n", 
               guest->vcpu.last_exit_cause,
               guest->vcpu.last_exit_cause == VMCAUSE_PRIVILEGED_INSTRUCTION ? "Privileged Instruction" :
               guest->vcpu.last_exit_cause == VMCAUSE_IO_INSTRUCTION ? "I/O Instruction" :
               guest->vcpu.last_exit_cause == VMCAUSE_PAGE_FAULT ? "Page Fault" : "Unknown");
        return guest->vcpu.last_exit_cause;
    }
    return VMCAUSE_NONE;
}

/* VMTRAPCFG rs - Set trap configuration bitmask */
void isa_vmtrapcfg(hypervisor_t* hv, uint32_t trap_config) {
    if (hv->mode == MODE_HOST && hv->current_guest_id < hv->guest_count) {
        guest_vm_t* guest = &hv->guests[hv->current_guest_id];
        guest->vcpu.vmcs.trap_config = trap_config;
        
        printf("[ISA:VMTRAPCFG] Trap config set to 0x%X\n", trap_config);
        printf("  - Trap privileged instructions: %s\n", 
               (trap_config & VMTRAPCFG_PRIVILEGED_INSTR) ? "YES" : "NO");
        printf("  - Trap CR writes: %s\n", 
               (trap_config & VMTRAPCFG_CR_WRITE) ? "YES" : "NO");
        printf("  - Trap I/O instructions: %s\n", 
               (trap_config & VMTRAPCFG_IO_INSTR) ? "YES" : "NO");
        printf("  - Trap page faults: %s\n", 
               (trap_config & VMTRAPCFG_PAGE_FAULT) ? "YES" : "NO");
    }
}

/* LDPGTR rs - Load guest page table root (CR3 equivalent) */
void isa_ldpgtr(hypervisor_t* hv, uint32_t guest_pgtbl) {
    if (hv->mode == MODE_HOST && hv->current_guest_id < hv->guest_count) {
        guest_vm_t* guest = &hv->guests[hv->current_guest_id];
        guest->vcpu.guest_pgtbl_root = guest_pgtbl;
        guest->vcpu.vmcs.guest_pgtbl_root = guest_pgtbl;
        printf("[ISA:LDPGTR] Guest page table root set to 0x%X (PID %u)\n", 
               guest_pgtbl, guest->vm_id);
    }
}

/* LDHPTR rs - Load host page table root */
void isa_ldhptr(hypervisor_t* hv, uint32_t host_pgtbl) {
    if (hv->mode == MODE_HOST && hv->current_guest_id < hv->guest_count) {
        guest_vm_t* guest = &hv->guests[hv->current_guest_id];
        guest->vcpu.host_pgtbl_root = host_pgtbl;
        guest->vcpu.vmcs.host_pgtbl_root = host_pgtbl;
        printf("[ISA:LDHPTR] Host page table root set to 0x%X (Guest %u)\n", 
               host_pgtbl, guest->vm_id);
    }
}

/* TLBFLUSHV - Flush guest TLB entries */
void isa_tlbflushv(hypervisor_t* hv) {
    if (hv->mode == MODE_HOST && hv->current_guest_id < hv->guest_count) {
        guest_vm_t* guest = &hv->guests[hv->current_guest_id];
        guest->vcpu.tlb_valid = false;
        guest->vcpu.tlb_entries = 0;
        printf("[ISA:TLBFLUSHV] Guest TLB flushed (Guest %u)\n", guest->vm_id);
    }
}

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

    printf("[HYPERVISOR] Initialized (Host Memory: %u KB, Max Guests: %u)\n", 
           MEMORY_SIZE / 1024, MAX_GUESTS);
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

    /* Initialize VMCS */
    guest->vcpu.vmcs.vmcs_id = guest_id;
    guest->vcpu.vmcs.exit_cause = VMCAUSE_NONE;
    guest->vcpu.vmcs.trap_config = 0;  /* No traps by default */

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

uint32_t host_translate_address(hypervisor_t* hv __attribute__((unused)), uint32_t guest_phys_addr) {
    return guest_phys_addr;  /* Direct 1:1 mapping for now */
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

    /* Use ISA instruction to enter guest */
    isa_vmenter(hv, &guest->vcpu.vmcs);

    const int TIME_SLICE = 10000;
    int total_instructions = 0;

    while (guest->vcpu.state == GUEST_RUNNING) {
        /* Execute guest time slice */
        for (int i = 0; i < TIME_SLICE && guest->vcpu.state == GUEST_RUNNING; i++) {
            uint32_t guest_virt_addr = guest->vcpu.pc;
            uint32_t guest_phys_addr = guest_translate_address(guest, guest_virt_addr);

            if (guest_phys_addr == 0xFFFFFFFF) {
                hv->mode = MODE_HOST;
                guest->vcpu.state = GUEST_BLOCKED;
                guest->vcpu.last_exit_cause = VMCAUSE_PAGE_FAULT;
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
                    if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT && instr.rs2 < REGISTER_COUNT) {
                        guest->vcpu.registers[instr.rd] = 
                            guest->vcpu.registers[instr.rs1] + guest->vcpu.registers[instr.rs2];
                    }
                    break;

                case OP_SUB:
                    if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT && instr.rs2 < REGISTER_COUNT) {
                        guest->vcpu.registers[instr.rd] = 
                            guest->vcpu.registers[instr.rs1] - guest->vcpu.registers[instr.rs2];
                    }
                    break;

                case OP_VMENTER:
                    /* Guest trying to enter nested VM (shouldn't happen, trap it) */
                    hv->mode = MODE_HOST;
                    guest->vcpu.state = GUEST_BLOCKED;
                    guest->vcpu.last_exit_cause = VMCAUSE_PRIVILEGED_INSTRUCTION;
                    break;

                case OP_HALT:
                    guest->vcpu.state = GUEST_STOPPED;
                    hv->mode = MODE_HOST;
                    break;

                default:
                    hv->mode = MODE_HOST;
                    guest->vcpu.state = GUEST_BLOCKED;
                    guest->vcpu.last_exit_cause = VMCAUSE_ILLEGAL_INSTRUCTION;
                    break;
            }

            total_instructions++;
        }

        /* Handle VMEXIT */
        if (guest->vcpu.state == GUEST_BLOCKED) {
            printf("[VMEXIT] Guest %u - Cause: 0x%X\n", guest->vm_id, guest->vcpu.last_exit_cause);
            
            if (guest->vcpu.last_exit_cause == VMCAUSE_ILLEGAL_INSTRUCTION) {
                break;
            }

            /* Use ISA instruction to resume */
            guest->vcpu.vmcs.guest_pc = guest->vcpu.pc;
            isa_vmresume(hv, &guest->vcpu.vmcs);
        }
    }

    printf("\n=========================================\n");
    printf("[HYPERVISOR] Guest VM %u stopped after %d instructions\n\n", 
           guest->vm_id, total_instructions);
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
    printf("  State: %d (0=Stopped, 1=Running, 2=Blocked, 3=Paused)\n", guest->vcpu.state);
    printf("  PC: 0x%08X\n", guest->vcpu.pc);
    printf("  SP: 0x%08X\n", guest->vcpu.sp);
    printf("  Priv: %s\n", guest->vcpu.priv == PRIV_KERNEL ? "KERNEL" : "USER");
    printf("  Guest PGTBL: 0x%08X\n", guest->vcpu.guest_pgtbl_root);
    printf("  Host PGTBL: 0x%08X\n", guest->vcpu.host_pgtbl_root);
    printf("  VMCS Trap Config: 0x%08X\n", guest->vcpu.vmcs.trap_config);
    printf("  Last Exit Cause: 0x%X\n", guest->vcpu.last_exit_cause);
    printf("  Instructions: %u\n", guest->instruction_count);
    printf("  TLB Valid: %s\n", guest->vcpu.tlb_valid ? "YES" : "NO");
}
