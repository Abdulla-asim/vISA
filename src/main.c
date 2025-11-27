#include <stdio.h>
#include "../include/isa.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <guest_image.bin> [guest2.bin ...]\n", argv[0]);
        fprintf(stderr, "Example: %s examples/programs/test.bin\n", argv[0]);
        return 1;
    }

    printf("======================================\n");
    printf("  vISA Hypervisor (ISA-Based)\n");
    printf("  Virtualization via ISA Instructions\n");
    printf("======================================\n\n");

    /* Create hypervisor */
    hypervisor_t* hv = hypervisor_create();
    if (!hv) {
        fprintf(stderr, "[ERROR] Failed to create hypervisor\n");
        return 1;
    }

    /* Load guest VMs */
    for (int i = 1; i < argc; i++) {
        uint32_t guest_id = hypervisor_create_guest(hv, argv[i]);
        if (guest_id == 0) {
            fprintf(stderr, "[ERROR] Failed to create guest from %s\n", argv[i]);
            hypervisor_destroy(hv);
            return 1;
        }
    }

    printf("\n");

    /* Run guests with round-robin scheduling (time-sliced) */
    printf("[SCHEDULER] Starting time-sliced execution (2 instructions per slice)\n\n");
    
    uint32_t total_ticks = 0;
    bool all_stopped = false;
    
    while (!all_stopped) {
        all_stopped = true;
        
        for (uint32_t i = 1; i <= hv->guest_count; i++) {
            guest_vm_t* guest = &hv->guests[i - 1];
            
            if (guest->vcpu.state != GUEST_STOPPED) {
                all_stopped = false;
                printf("[TICK %u] Running Guest VM %u time slice...\n", total_ticks, guest->vm_id);
                
                /* Run single time slice for this guest */
                const int TIME_SLICE = 2;
                int slice_count = 0;
                
                if (guest->vcpu.state == GUEST_STOPPED) {
                    isa_vmenter(hv, &guest->vcpu.vmcs);
                }
                
                while (guest->vcpu.state == GUEST_RUNNING && slice_count < TIME_SLICE) {
                    uint32_t guest_virt_addr = guest->vcpu.pc;
                    uint32_t guest_phys_addr = guest_translate_address(guest, guest_virt_addr);

                    if (guest_phys_addr == 0xFFFFFFFF) {
                        hv->mode = MODE_HOST;
                        guest->vcpu.state = GUEST_BLOCKED;
                        guest->vcpu.last_exit_cause = VMCAUSE_PAGE_FAULT;
                        break;
                    }

                    /* Fetch and execute one instruction */
                    instruction_t instr;
                    instr.opcode = guest->guest_memory[guest_phys_addr];
                    instr.rd = guest->guest_memory[guest_phys_addr + 1];
                    instr.rs1 = guest->guest_memory[guest_phys_addr + 2];
                    instr.rs2 = guest->guest_memory[guest_phys_addr + 3];
                    
                    /* Print instruction before execution */
                    const char* op_name = "???";
                    switch(instr.opcode) {
                        case 0x01: op_name = "ADD"; break;
                        case 0x02: op_name = "SUB"; break;
                        case 0x03: op_name = "MUL"; break;
                        case 0x04: op_name = "DIV"; break;
                        case 0x05: op_name = "MOV"; break;
                        case 0x06: op_name = "LOAD"; break;
                        case 0x07: op_name = "STORE"; break;
                        case 0x08: op_name = "JMP"; break;
                        case 0x09: op_name = "JEQ"; break;
                        case 0x0A: op_name = "JNE"; break;
                        case 0x0B: op_name = "CALL"; break;
                        case 0x0C: op_name = "RET"; break;
                        case 0xFF: op_name = "HALT"; break;
                    }
                    printf("    [G%u:0x%02X] %s r%u r%u r%u\n", guest->vm_id, guest->vcpu.pc, 
                           op_name, instr.rd, instr.rs1, instr.rs2);
                    
                    guest->vcpu.pc += INSTRUCTION_SIZE;
                    slice_count++;
                    guest->instruction_count++;
                    
                    /* Execute (simplified - just track execution) */
                    if (instr.opcode == OP_HALT) {
                        guest->vcpu.state = GUEST_STOPPED;
                        hv->mode = MODE_HOST;
                    }
                }
                
                printf("  [Guest %u completed %d instructions this slice, total: %u]\n", 
                       guest->vm_id, slice_count, guest->instruction_count);
                total_ticks++;
            }
        }
    }
    
    printf("\n[SCHEDULER] All guests stopped after %u scheduling rounds\n\n", total_ticks);

    /* Final state */
    hypervisor_dump_state(hv);

    hypervisor_destroy(hv);
    return 0;
}
