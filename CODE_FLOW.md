# vISA Code Flow: From Source to Execution

## 1. Compilation Flow

```
CMakeLists.txt
    ├─ Finds C compiler (gcc/clang)
    ├─ Includes: include/isa.h
    ├─ Source files:
    │  ├─ src/main.c (entry point)
    │  ├─ src/hypervisor_isa.c (execution engine)
    │  └─ Links together
    └─ Output: build/vISA.exe (296 KB executable)
```

## 2. Assembly → Binary Conversion Flow

### Input File: examples/programs/arithmetic.isa
```
mov r0, r0
mov r1, r1
add r2, r0, r1
mov r3, r2
halt
```

### Python Assembler Processing (assembler.py)

**Step 1: Load OPCODES dictionary**
```python
OPCODES = {
    'mov': 0x05,
    'add': 0x01,
    'halt': 0xFF,
    ... (all 22 instructions)
}
```

**Step 2: For each line, parse instruction**
```python
Line 1: "mov r0, r0"
  ├─ Split by space: ["mov", "r0,", "r0"]
  ├─ Look up mnemonic: 'mov' → opcode 0x05
  ├─ Parse operands:
  │  ├─ parse_register("r0") → 0
  │  ├─ parse_register("r0") → 0
  │  └─ (Third operand defaults to 0)
  ├─ Create instruction: [0x05, 0x00, 0x00, 0x00]
  └─ Append to binary array
```

**Step 3: Write binary to file**
```python
struct.pack('BBBB', 0x05, 0x00, 0x00, 0x00)  # 4 bytes per instruction
```

### Output File: examples/programs/arithmetic.bin
```
Binary (hex):
  05 00 00 00  (MOV r0, r0)
  05 01 01 00  (MOV r1, r1)
  01 02 00 01  (ADD r2, r0, r1)
  05 03 02 00  (MOV r3, r2)
  FF 00 00 00  (HALT)

Total: 20 bytes (5 instructions × 4 bytes/instruction)
```

## 3. Execution: Start to Finish

### Program Entry Point: main.c

```c
int main(int argc, char* argv[]) {
    // argv[0] = program name
    // argv[1] = first guest binary path
    // argv[2] = second guest binary path (optional)
    // ...
    
    // PHASE 1: INITIALIZATION
    printf("vISA Hypervisor Starting...");
    
    hypervisor_t* hv = hypervisor_create();
    // Allocates hypervisor_t structure
    // Sets: mode = MODE_HOST
    //       host_memory = 64KB zeroed
    //       guests = empty array
    //       guest_count = 0
    
    
    // PHASE 2: LOAD GUESTS
    for (int i = 1; i < argc; i++) {
        uint32_t guest_id = hypervisor_create_guest(hv, argv[i]);
        // For each binary file:
        // 1. Open file
        // 2. Read bytes into guest_memory
        // 3. Initialize page tables (identity map)
        // 4. Set initial registers to 0
        // 5. Set PC to 0x00
        // 6. Set state to GUEST_RUNNING
        // 7. Increment guest_count
    }
    
    
    // PHASE 3: SCHEDULE & EXECUTE GUESTS
    printf("[SCHEDULER] Starting time-sliced execution\n");
    
    uint32_t total_ticks = 0;
    bool all_stopped = false;
    
    while (!all_stopped) {
        all_stopped = true;
        
        // LOOP: For each guest in round-robin order
        for (uint32_t i = 1; i <= hv->guest_count; i++) {
            guest_vm_t* guest = &hv->guests[i - 1];
            
            // CONDITIONAL: Is this guest still running?
            if (guest->vcpu.state != GUEST_STOPPED) {
                all_stopped = false;
                printf("[TICK %u] Running Guest VM %u\n", total_ticks++, guest->vm_id);
                
                // TIME SLICE: Execute up to 2 instructions
                const int TIME_SLICE = 2;
                int slice_count = 0;
                
                while (guest->vcpu.state == GUEST_RUNNING && slice_count < TIME_SLICE) {
                    // ========== INSTRUCTION FETCH & EXECUTE ==========
                    
                    uint32_t guest_virt_addr = guest->vcpu.pc;
                    
                    // TRANSLATE: Virtual → Physical address
                    uint32_t guest_phys_addr = guest_translate_address(guest, guest_virt_addr);
                    
                    if (guest_phys_addr == 0xFFFFFFFF) {
                        // PAGE FAULT
                        guest->vcpu.state = GUEST_BLOCKED;
                        guest->vcpu.last_exit_cause = VMCAUSE_PAGE_FAULT;
                        break;
                    }
                    
                    // FETCH: Read 4 bytes from guest memory
                    instruction_t instr;
                    instr.opcode = guest->guest_memory[guest_phys_addr];
                    instr.rd = guest->guest_memory[guest_phys_addr + 1];
                    instr.rs1 = guest->guest_memory[guest_phys_addr + 2];
                    instr.rs2 = guest->guest_memory[guest_phys_addr + 3];
                    
                    // TRACE: Print instruction being executed
                    printf("    [G%u:0x%02X] %s r%u r%u r%u\n", 
                           guest->vm_id, guest->vcpu.pc, 
                           opcode_name[instr.opcode], 
                           instr.rd, instr.rs1, instr.rs2);
                    
                    // UPDATE PC: Move to next instruction
                    guest->vcpu.pc += 4;
                    
                    // EXECUTE: Dispatch to instruction handler
                    switch (instr.opcode) {
                        
                        case OP_MOV:
                            if (instr.rd < 32 && instr.rs1 < 32) {
                                guest->vcpu.registers[instr.rd] = 
                                    guest->vcpu.registers[instr.rs1];
                                printf("  MOV r%u = r%u\n", instr.rd, instr.rs1);
                            }
                            break;
                        
                        case OP_ADD:
                            if (instr.rd < 32 && instr.rs1 < 32 && instr.rs2 < 32) {
                                guest->vcpu.registers[instr.rd] = 
                                    guest->vcpu.registers[instr.rs1] + 
                                    guest->vcpu.registers[instr.rs2];
                                printf("  ADD r%u = r%u(0x%X) + r%u(0x%X) = 0x%X\n",
                                       instr.rd, instr.rs1, guest->vcpu.registers[instr.rs1],
                                       instr.rs2, guest->vcpu.registers[instr.rs2],
                                       guest->vcpu.registers[instr.rd]);
                            }
                            break;
                        
                        // ... (other instruction cases)
                        
                        case OP_HALT:
                            guest->vcpu.state = GUEST_STOPPED;
                            hv->mode = MODE_HOST;
                            printf("  HALT\n");
                            break;
                        
                        default:
                            guest->vcpu.state = GUEST_BLOCKED;
                            guest->vcpu.last_exit_cause = VMCAUSE_ILLEGAL_INSTRUCTION;
                            break;
                    }
                    
                    // BOOKKEEPING
                    slice_count++;
                    guest->instruction_count++;
                    
                }  // End of time slice
                
                printf("  [Guest %u completed %d instructions, total: %u]\n",
                       guest->vm_id, slice_count, guest->instruction_count);
            }
        }  // End of guest loop
    }  // End of scheduler loop
    
    
    // PHASE 4: CLEANUP & REPORT
    printf("\n[HYPERVISOR STATE]\n");
    hypervisor_dump_state(hv);  // Print final statistics
    
    hypervisor_destroy(hv);  // Free memory
    return 0;
}
```

## 4. Instruction Execution Deep Dive: ADD Example

### Scenario: Execute "ADD r2, r0, r1"

```
BEFORE EXECUTION:
  guest->vcpu.registers[0] = 0x00000005  (r0 = 5)
  guest->vcpu.registers[1] = 0x00000003  (r1 = 3)
  guest->vcpu.registers[2] = 0x00000000  (r2 = 0, uninitialized)
  guest->vcpu.pc = 0x08

FETCH PHASE:
  guest_phys_addr = 0x08
  instruction_t instr:
    .opcode = guest->guest_memory[0x08] = 0x01  (ADD)
    .rd = guest->guest_memory[0x09] = 0x02      (r2)
    .rs1 = guest->guest_memory[0x0A] = 0x00     (r0)
    .rs2 = guest->guest_memory[0x0B] = 0x01     (r1)

UPDATE PC:
  guest->vcpu.pc = 0x08 + 4 = 0x0C

EXECUTE:
  case OP_ADD:
    rd_value = guest->vcpu.registers[0] + guest->vcpu.registers[1]
    rd_value = 0x05 + 0x03 = 0x08
    guest->vcpu.registers[2] = 0x08

TRACE OUTPUT:
  [G0:0x08] ADD r2 r0 r1
    ADD r2 = r0(0x5) + r1(0x3) = 0x8

AFTER EXECUTION:
  guest->vcpu.registers[0] = 0x00000005  (unchanged)
  guest->vcpu.registers[1] = 0x00000003  (unchanged)
  guest->vcpu.registers[2] = 0x00000008  (result!)
  guest->vcpu.pc = 0x0C                  (next instruction)
  guest->instruction_count++ = 6
```

## 5. Page Table Translation Example

### Setup
```
Guest's virtual address space: 0x0000 - 0x3FFF (16KB)
Guest's physical address space: 0x0000 - 0x3FFF (16KB)
Page size: 4KB (0x1000 bytes)
```

### Guest accesses virtual address 0x1234

```
STEP 1: Calculate page number
  page_num = 0x1234 / 0x1000 = 1 (page 1)

STEP 2: Look up page table entry
  pte = &guest->vcpu.guest_page_table[1]
  pte.present = true (initialized to present)
  pte.guest_physical_page = 1 (identity mapped)

STEP 3: Calculate offset within page
  offset = 0x1234 % 0x1000 = 0x234

STEP 4: Calculate physical address
  guest_phys_addr = (1 * 0x1000) + 0x234 = 0x1234

RESULT:
  Virtual  0x1234 → Physical 0x1234
  (Same address, but in guest's memory space)
```

## 6. Two-Guest Context Switch Scenario

### Initial State: Guest 0 running
```
TICK 0:
  hv->current_guest_id = 0
  guest = &hv->guests[0]
  
  for 2 instructions:
    Fetch from guest[0].guest_memory
    Execute in guest[0].vcpu.registers
    Update guest[0].vcpu.pc
  
  guest[0].instruction_count += 2
```

### Switch: Guest 0 → Guest 1
```
IMPLICIT CONTEXT SWITCH:
  (We don't explicitly save/restore - each guest has its own memory!)
  
  Guest 0's state automatically preserved:
    - guest[0].vcpu.registers[0-31] (unchanged in memory)
    - guest[0].vcpu.pc (updated to next instruction)
    - guest[0].guest_memory (unchanged)
  
  Loop moves to i=1 (next guest)
```

### Next State: Guest 1 running
```
TICK 1:
  hv->current_guest_id = 1
  guest = &hv->guests[1]
  
  for 2 instructions:
    Fetch from guest[1].guest_memory (DIFFERENT MEMORY!)
    Execute in guest[1].vcpu.registers (DIFFERENT REGISTERS!)
    Update guest[1].vcpu.pc
  
  guest[1].instruction_count += 2
```

### Resume: Guest 0 again
```
TICK 2:
  Loop wraps back to i=0
  guest = &hv->guests[0]
  
  Guest 0's state is EXACTLY where we left it!
    - guest[0].vcpu.pc = 0x08 (where we stopped)
    - guest[0].vcpu.registers still hold their values
    - guest[0].guest_memory still valid
  
  for 2 instructions:
    Continue from PC = 0x08 (not 0x00!)
    Execute next 2 instructions
```

## 7. Memory Layout During Execution

### Host (Hypervisor) View

```
Host Memory: 64KB total

Offset 0x0000 to 0x3FFF (16KB): Guest 0 memory
  │ guest[0].guest_memory[0x0000-0x3FFF]
  │ Contains: guest 0's code + data

Offset 0x4000 to 0x7FFF (16KB): Guest 1 memory
  │ guest[1].guest_memory[0x0000-0x3FFF]
  │ Contains: guest 1's code + data

Offset 0x8000 to 0xBFFF (16KB): Guest 2 memory
  │ guest[2].guest_memory[0x0000-0x3FFF]
  │ Contains: guest 2's code + data

Offset 0xC000 to 0xFFFF (16KB): Guest 3 memory
  │ guest[3].guest_memory[0x0000-0x3FFF]
  │ Contains: guest 3's code + data
```

### Guest 0 View (Guest sees addresses 0x0000-0x3FFF)

```
Guest Virtual: 0x0000 to 0x3FFF
         ↓ (page table translation)
Guest Physical: 0x0000 to 0x3FFF (in guest[0].guest_memory)
         ↓ (no further translation, direct access)
Host Physical: 0x0000 to 0x3FFF (in hypervisor's host_memory)
```

### Guest 1 View (Same virtual addresses!)

```
Guest Virtual: 0x0000 to 0x3FFF
         ↓ (same page table translation!)
Guest Physical: 0x0000 to 0x3FFF (in guest[1].guest_memory)
         ↓ (no further translation, direct access)
Host Physical: 0x4000 to 0x7FFF (DIFFERENT! In hypervisor's host_memory)
         ↑ (Because guest[1] starts at 0x4000 in host!)
```

## 8. Complete Execution Trace: 2 Guests, 2-Instr Slice

### Initialization
```
1. hypervisor_create()
   hv->host_memory = [64KB zeroed]
   hv->mode = MODE_HOST
   hv->guest_count = 0

2. Load Guest 0 from "guest1.bin"
   Read 20 bytes
   Store in guest[0].guest_memory[0x0000-0x0013]
   Initialize page tables
   hv->guest_count = 1

3. Load Guest 1 from "guest2.bin"
   Read 16 bytes
   Store in guest[1].guest_memory[0x0000-0x000F]
   Initialize page tables
   hv->guest_count = 2
```

### Scheduler Loop
```
all_stopped = false
total_ticks = 0

ITERATION 1:
  for i=1 to 2:
    i=1 → guest[0]
      if state == RUNNING:
        all_stopped = false
        print "[TICK 0] Running Guest VM 0"
        for instr=1 to 2:
          fetch, decode, execute
          print trace
        total_ticks++
    
    i=2 → guest[1]
      if state == RUNNING:
        all_stopped = false
        print "[TICK 1] Running Guest VM 1"
        for instr=1 to 2:
          fetch, decode, execute
          print trace
        total_ticks++

ITERATION 2:
  (Repeat, all guests still running)
  
  TICK 2: Guest 0 again (resumes from PC where it left off)
  TICK 3: Guest 1 again

ITERATION 3:
  TICK 4: Guest 0 (might hit HALT here)
  TICK 5: Guest 1 (might hit HALT here)

After loop:
  all_stopped = true (both guests in STOPPED state)
  exit loop

Print final state
Free memory
Return 0
```

---

## Summary: Call Stack Flow

```
main()
├─ hypervisor_create()
│
├─ for each guest:
│  └─ hypervisor_create_guest(hv, binary_path)
│     ├─ fopen() read binary
│     ├─ fread() into guest_memory
│     ├─ initialize page tables
│     └─ return guest_id
│
├─ scheduler loop:
│  for i=1 to guest_count:
│    ├─ fetch_instruction(guest.memory[guest.pc])
│    ├─ decode (opcode, rd, rs1, rs2)
│    ├─ switch(opcode) {
│    │    case ADD: execute_add()
│    │    case MOV: execute_mov()
│    │    case HALT: set_state_stopped()
│    │    ...
│    │ }
│    └─ print_trace()
│
├─ hypervisor_dump_state()
│  ├─ print each guest state
│  └─ print registers, PC, etc.
│
└─ hypervisor_destroy()
   └─ free(hv)
```

This is the complete execution flow from program start to finish!
