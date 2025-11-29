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
    
    int max_ticks = 1000;  // Add safety limit
    while (!all_stopped && total_ticks < max_ticks) {
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

                    /* Fetch one instruction */
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
                        case 0x0D: op_name = "MOVI"; break;
                        case 0x0E: op_name = "ADDI"; break;
                        case 0x0F: op_name = "SUBI"; break;
                        case 0x10: op_name = "MULI"; break;
                        case 0x11: op_name = "DIVI"; break;
                        case 0xFF: op_name = "HALT"; break;
                    }
                    printf("    [G%u:0x%02X] %s r%u r%u r%u\n", guest->vm_id, guest->vcpu.pc, 
                           op_name, instr.rd, instr.rs1, instr.rs2);
                    
                    guest->vcpu.pc += INSTRUCTION_SIZE;
                    slice_count++;
                    guest->instruction_count++;
                    
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

                        case OP_MUL:
                            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT && instr.rs2 < REGISTER_COUNT) {
                                guest->vcpu.registers[instr.rd] = 
                                    guest->vcpu.registers[instr.rs1] * guest->vcpu.registers[instr.rs2];
                            }
                            break;

                        case OP_DIV:
                            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT && instr.rs2 < REGISTER_COUNT) {
                                if (guest->vcpu.registers[instr.rs2] != 0) {
                                    guest->vcpu.registers[instr.rd] = 
                                        guest->vcpu.registers[instr.rs1] / guest->vcpu.registers[instr.rs2];
                                }
                            }
                            break;

                        case OP_MOV:
                            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT) {
                                guest->vcpu.registers[instr.rd] = guest->vcpu.registers[instr.rs1];
                            }
                            break;

                        case OP_LOAD:
                            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT) {
                                uint32_t addr = guest_translate_address(guest, guest->vcpu.registers[instr.rs1]);
                                if (addr != 0xFFFFFFFF) {
                                    guest->vcpu.registers[instr.rd] = guest->guest_memory[addr];
                                }
                            }
                            break;

                        case OP_STORE:
                            if (instr.rs1 < REGISTER_COUNT && instr.rs2 < REGISTER_COUNT) {
                                uint32_t addr = guest_translate_address(guest, guest->vcpu.registers[instr.rs1]);
                                if (addr != 0xFFFFFFFF) {
                                    guest->guest_memory[addr] = guest->vcpu.registers[instr.rs2];
                                }
                            }
                            break;

                        /* ============ IMMEDIATE INSTRUCTIONS ============ */
                        case OP_MOVI:
                            if (instr.rd < REGISTER_COUNT) {
                                guest->vcpu.registers[instr.rd] = (uint32_t)instr.rs2;
                            }
                            break;

                        case OP_ADDI:
                            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT) {
                                guest->vcpu.registers[instr.rd] = 
                                    guest->vcpu.registers[instr.rs1] + (uint32_t)instr.rs2;
                            }
                            break;

                        case OP_SUBI:
                            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT) {
                                guest->vcpu.registers[instr.rd] = 
                                    guest->vcpu.registers[instr.rs1] - (uint32_t)instr.rs2;
                            }
                            break;

                        case OP_MULI:
                            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT) {
                                guest->vcpu.registers[instr.rd] = 
                                    guest->vcpu.registers[instr.rs1] * (uint32_t)instr.rs2;
                            }
                            break;

                        case OP_DIVI:
                            if (instr.rd < REGISTER_COUNT && instr.rs1 < REGISTER_COUNT) {
                                if (instr.rs2 != 0) {
                                    guest->vcpu.registers[instr.rd] = 
                                        guest->vcpu.registers[instr.rs1] / (uint32_t)instr.rs2;
                                }
                            }
                            break;

                        case OP_CALL:
                            if (guest->vcpu.sp > 3) {
                                // Save return address (current PC which is already at next instruction)
                                uint32_t return_addr = guest->vcpu.pc;
                                guest->guest_memory[guest->vcpu.sp - 3] = (return_addr >> 24) & 0xFF;
                                guest->guest_memory[guest->vcpu.sp - 2] = (return_addr >> 16) & 0xFF;
                                guest->guest_memory[guest->vcpu.sp - 1] = (return_addr >> 8) & 0xFF;
                                guest->guest_memory[guest->vcpu.sp] = return_addr & 0xFF;
                                guest->vcpu.sp -= 4;
                                
                                // Jump to function address in rd (instruction address, not byte address)
                                if (instr.rd != 0) {
                                    guest->vcpu.pc = instr.rd * INSTRUCTION_SIZE;
                                } else if (instr.rs1 < REGISTER_COUNT) {
                                    guest->vcpu.pc = guest->vcpu.registers[instr.rs1];
                                }
                            }
                            break;

                        case OP_RET:
                            if (guest->vcpu.sp + 4 <= GUEST_PHYS_MEMORY_SIZE) {
                                // Restore 32-bit return address from stack
                                // We saved at: SP-3, SP-2, SP-1, SP (before SP -= 4)
                                // Now SP points to old (SP-4), so old SP = current SP + 4
                                uint32_t saved_sp = guest->vcpu.sp + 4;  // Reconstruct original SP
                                guest->vcpu.pc = ((uint32_t)guest->guest_memory[saved_sp - 3] << 24) |
                                                ((uint32_t)guest->guest_memory[saved_sp - 2] << 16) |
                                                ((uint32_t)guest->guest_memory[saved_sp - 1] << 8) |
                                                ((uint32_t)guest->guest_memory[saved_sp]);
                                guest->vcpu.sp += 4;
                            }
                            break;

                        case OP_HALT:
                            guest->vcpu.state = GUEST_STOPPED;
                            hv->mode = MODE_HOST;
                            break;

                        default:
                            break;
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
