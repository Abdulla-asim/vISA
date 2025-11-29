# vISA Hypervisor: Complete System Architecture Guide

## Part 1: What is vISA?

**vISA** = Virtual Instruction Set Architecture + Hypervisor

It's a complete virtualization system written in C that:
- Defines a custom 32-bit instruction set (22 instructions total)
- Implements a hypervisor that runs multiple guest virtual machines
- Provides CPU scheduling via round-robin time-slicing
- Demonstrates hardware virtualization concepts

Think of it like:
- Guest Programs → Binary compiled with your ISA
- Hypervisor → Runs in "host mode", manages all guests
- Each Guest VM → Isolated execution environment with own registers, memory, state

---

## Part 2: The ISA (Instruction Set Architecture)

### What is an ISA?

An ISA is the **contract between software and hardware** - it defines:
- What instructions exist (ADD, MOV, etc.)
- What registers are available (r0-r31)
- Memory format
- How instructions encode into binary

### vISA's 22 Instructions

**Arithmetic (4 instructions):**
```
ADD rd, rs1, rs2    → rd = rs1 + rs2        (Opcode: 0x01)
SUB rd, rs1, rs2    → rd = rs1 - rs2        (Opcode: 0x02)
MUL rd, rs1, rs2    → rd = rs1 * rs2        (Opcode: 0x03)
DIV rd, rs1, rs2    → rd = rs1 / rs2        (Opcode: 0x04)
```

**Memory Operations (3 instructions):**
```
MOV rd, rs1         → rd = rs1              (Opcode: 0x05)
LOAD rd, [rs1]      → rd = memory[rs1]      (Opcode: 0x06)
STORE [rs1], rs2    → memory[rs1] = rs2     (Opcode: 0x07)
```

**Control Flow (4 instructions):**
```
JMP rs1             → PC = rs1              (Opcode: 0x08)
JEQ rd, rs1, rs2    → if rs1==rs2: PC=rd   (Opcode: 0x09)
JNE rd, rs1, rs2    → if rs1!=rs2: PC=rd   (Opcode: 0x0A)
CALL rs1            → Save PC, jump rs1     (Opcode: 0x0B)
RET                 → Pop PC from stack     (Opcode: 0x0C)
```

**Virtualization (7 instructions - the hypervisor uses these):**
```
VMENTER vmcs        → Enter guest mode      (Opcode: 0x30)
VMRESUME vmcs       → Resume after exit     (Opcode: 0x31)
VMCAUSE rd          → Read exit reason      (Opcode: 0x32)
VMTRAPCFG rs        → Set trap bitmask      (Opcode: 0x33)
LDPGTR rs           → Load guest page table (Opcode: 0x34)
LDHPTR rs           → Load host page table  (Opcode: 0x35)
TLBFLUSHV           → Flush guest TLB       (Opcode: 0x36)
```

**System Instructions (2 instructions):**
```
SYSCALL             → Trap to hypervisor    (Opcode: 0x20)
HYPERCALL           → Hypervisor call       (Opcode: 0x21)
```

**Special (1 instruction):**
```
HALT                → Stop execution        (Opcode: 0xFF)
```

---

## Part 3: Instruction Encoding (Binary Format)

### 32-bit Fixed Format

Every instruction is exactly **4 bytes**:
```
Byte 0: OPCODE (0x00-0xFF)
Byte 1: rd (destination register, 0-31)
Byte 2: rs1 (source register 1, 0-31)
Byte 3: rs2 (source register 2, 0-31)
```

**Example:**
```
Instruction: ADD r2, r0, r1
Binary:      01 02 00 01
Hex:         0x01020001

Byte breakdown:
  0x01 → ADD opcode
  0x02 → rd = r2 (destination)
  0x00 → rs1 = r0 (first source)
  0x01 → rs2 = r1 (second source)
```

### File Structure

Files are organized:
```
include/isa.h              ← ISA definitions (opcodes, structures, types)
src/hypervisor_isa.c       ← Hypervisor implementation (execution engine)
src/main.c                 ← Entry point (scheduler, guest management)
examples/assembler.py      ← Converts .isa text → .bin binary
examples/programs/*.isa    ← Guest programs (human-readable assembly)
```

---

## Part 4: The Assembler (Python)

### How Assembler Works

The assembler (`examples/assembler.py`) converts text assembly to binary:

**Input (arithmetic.isa):**
```
mov r0, r0
mov r1, r1
add r2, r0, r1
halt
```

**Parsing Process:**
1. Read line: `mov r0, r0`
2. Look up opcode: MOV → 0x05
3. Parse operands: rd=r0 (0), rs1=r0 (0)
4. Encode as 4 bytes: `05 00 00 00`
5. Repeat for all lines

**Output (arithmetic.bin):**
```
Byte 0-3:   05 00 00 00  (MOV r0, r0)
Byte 4-7:   05 01 01 00  (MOV r1, r1)
Byte 8-11:  01 02 00 01  (ADD r2, r0, r1)
Byte 12-15: FF 00 00 00  (HALT)
Total: 16 bytes
```

### Running the Assembler

```bash
python assembler.py input.isa output.bin
```

This creates a binary that the hypervisor can load and execute.

---

## Part 5: The Hypervisor Architecture

### Core Data Structures (in include/isa.h)

**1. vcpu_t - Virtual CPU (guest's virtualized CPU)**
```c
typedef struct {
    uint32_t registers[REGISTER_COUNT];     // r0-r31 (32 registers per guest)
    uint32_t pc;                            // Program Counter (points to next instruction)
    uint32_t sp;                            // Stack Pointer
    uint8_t priv;                           // Privilege level (KERNEL or USER)
    
    guest_page_table_entry_t guest_page_table[4096];  // Guest's VA→PA translation table
    uint32_t guest_pgtbl_root;              // Base address of guest page table
    uint32_t host_pgtbl_root;               // Hypervisor page table
    
    vmcs_t vmcs;                            // Virtual Machine Control Structure
    uint32_t last_exit_cause;               // Why guest exited to hypervisor
    bool tlb_valid;                         // TLB validity flag
    
    guest_state_t state;                    // RUNNING, STOPPED, BLOCKED, PAUSED
} vcpu_t;
```

**2. vmcs_t - Virtual Machine Control Structure (guest state save/restore)**
```c
typedef struct {
    uint32_t vmcs_id;
    uint32_t guest_rax, guest_rbx, guest_rcx, guest_rdx;  // Saved registers
    uint32_t guest_pc;                                     // Saved program counter
    uint8_t guest_priv;                                    // Saved privilege
    uint32_t guest_pgtbl_root;                             // Saved page table
    uint32_t host_pgtbl_root;
    uint32_t trap_config;                                  // Trap configuration bitmask
    uint32_t exit_cause;                                   // Why we exited
} vmcs_t;
```

**3. guest_vm_t - One guest virtual machine**
```c
typedef struct {
    uint32_t vm_id;                              // 0, 1, 2, 3...
    guest_state_t state;                         // Running or stopped
    uint8_t guest_memory[GUEST_PHYS_MEMORY_SIZE]; // 16KB physical memory for this guest
    extended_page_table_entry_t ept[256];        // Extended Page Table
    vcpu_t vcpu;                                 // The guest's virtual CPU
    uint32_t instruction_count;                  // Total instructions executed
} guest_vm_t;
```

**4. hypervisor_t - The hypervisor host**
```c
typedef struct {
    uint8_t host_memory[MEMORY_SIZE];        // 64KB total host memory
    guest_vm_t guests[MAX_GUESTS];           // Max 4 guests
    uint32_t guest_count;                    // How many guests created
    execution_mode_t mode;                   // MODE_HOST or MODE_GUEST
    uint32_t current_guest_id;               // Which guest running now
    uint32_t tick_count;                     // Scheduler ticks
    bool halted;
} hypervisor_t;
```

---

## Part 6: Execution Flow - Step by Step

### Scenario: Running 2 guest programs with 2-instruction time slice

**Setup Phase:**
```
1. main() calls hypervisor_create()
   → Allocates hypervisor_t, initializes to HOST mode
   
2. For each guest binary:
   hypervisor_create_guest(hv, "guest1.bin")
   → Allocates guest_vm_t
   → Loads binary into guest_memory
   → Sets up identity-mapped page tables
   → Starts guest in RUNNING state
   
3. Result: 2 guest_vm_t structures loaded in hv->guests[0] and hv->guests[1]
```

**Execution Phase (Round-Robin Scheduler):**

```
Main loop in main.c:

while (!all_stopped) {
    all_stopped = true
    
    for each guest {
        if guest.state != STOPPED {
            all_stopped = false
            
            // Run this guest for its time slice
            for 2 instructions {
                1. Fetch instruction from guest_memory[guest.pc]
                2. Decode: extract opcode, rd, rs1, rs2
                3. Execute instruction (modify registers/memory)
                4. Increment PC by 4
                5. Print trace: [G0:0x00] MOV r0 r0 r0
            }
        }
    }
}
```

### Detailed Example: Guest 0 and Guest 1 running

**TICK 0: Guest 0's turn (2-instruction slice)**
```
PC = 0x00: Fetch instruction
  Memory[0x00] = 0x05 (MOV opcode)
  Memory[0x01] = 0x00 (rd = r0)
  Memory[0x02] = 0x00 (rs1 = r0)
  Memory[0x03] = 0x00 (rs2 = r0)
  
Decode: MOV r0, r0
Execute: r0 = r0 (no-op, stays 0)
PC = 0x04

Print: [G0:0x00] MOV r0 r0 r0

PC = 0x04: Fetch next instruction
  Memory[0x04] = 0x05 (MOV opcode)
  Memory[0x05] = 0x01 (rd = r1)
  Memory[0x06] = 0x01 (rs1 = r1)
  Memory[0x07] = 0x00 (rs2 = r0)
  
Decode: MOV r1, r1
Execute: r1 = r1 (no-op, stays 0)
PC = 0x08

Print: [G0:0x04] MOV r1 r1 r0

Slice complete: Guest 0 executed 2 instructions, now PC=0x08
Guest 0's state: registers unchanged, PC moved forward
```

**TICK 1: Guest 1's turn (context switch)**
```
[Context switch: Host saves Guest 0's state, loads Guest 1's state]

Guest 1 PC = 0x00 (starts fresh)
Guest 1 registers = all zeros

PC = 0x00: Execute MOV r5, r5
  [G1:0x00] MOV r5 r5 r0
  r5 = 0
  PC = 0x04

PC = 0x04: Execute MOV r6, r6
  [G1:0x04] MOV r6 r6 r0
  r6 = 0
  PC = 0x08

Slice complete: Guest 1 executed 2 instructions
```

**TICK 2: Guest 0 resumes**
```
[Context switch: Host loads Guest 0's state again]

Guest 0 PC = 0x08 (where we left off!)
Guest 0 registers = r0:0, r1:0 (preserved from before)

PC = 0x08: Execute ADD r2, r0, r1
  [G0:0x08] ADD r2 r0 r1
  r2 = r0 + r1 = 0 + 0 = 0
  PC = 0x0C

PC = 0x0C: Execute MOV r3, r2
  [G0:0x0C] MOV r3 r2 r0
  r3 = r2 = 0
  PC = 0x10

Slice complete: Guest 0 total = 4 instructions
```

This repeats until all guests execute HALT.

---

## Part 7: Memory Management (Two-Level Address Translation)

### Problem: Guest needs isolated memory

If Guest 0 and Guest 1 both use address 0x1000, they need different physical locations!

### Solution: Virtual Address → Physical Address Translation

**Two levels:**
```
Guest Virtual Address (what guest uses)
    ↓ [page table lookup]
Guest Physical Address (in guest's 16KB space)
    ↓ [no further translation - direct mapping]
Host Physical Address (in hypervisor's 64KB space)
```

**Implementation in hypervisor_isa.c:**
```c
uint32_t guest_translate_address(guest_vm_t* guest, uint32_t guest_virt_addr) {
    // Translate guest VA to guest PA
    uint32_t page_num = guest_virt_addr / PAGE_SIZE;  // 4KB pages
    
    guest_page_table_entry_t* pte = &guest->vcpu.guest_page_table[page_num];
    
    if (!pte->present) {
        return 0xFFFFFFFF;  // Page fault!
    }
    
    uint32_t offset = guest_virt_addr % PAGE_SIZE;
    uint32_t guest_phys_addr = (pte->guest_physical_page * PAGE_SIZE) + offset;
    
    return guest_phys_addr;  // Final address in guest memory
}
```

**Example:**
```
Guest 0 accesses virtual address 0x1000:
  Page = 0x1000 / 4096 = page 0
  guest->guest_page_table[0].present = true
  guest->guest_page_table[0].guest_physical_page = 0
  Physical address = 0 * 4096 + 0 = 0x0000
  Actually reads/writes: guest->guest_memory[0x0000]

Guest 1 accesses virtual address 0x1000:
  Page = 0x1000 / 4096 = page 0
  guest->guest_page_table[0].present = true
  guest->guest_page_table[0].guest_physical_page = 0
  Physical address = 0 * 4096 + 0 = 0x0000
  Actually reads/writes: guest->guest_memory[0x0000]  ← Different memory!
```

Each guest has its own guest_memory[16KB], so isolation is automatic.

---

## Part 8: Virtualization Instructions (VMENTER/VMEXIT)

### What are VMENTER and VMEXIT?

**VMENTER**: Transition from hypervisor (host mode) → guest (guest mode)
**VMEXIT**: Transition from guest → hypervisor (when guest needs hypervisor help)

### VMENTER Implementation

```c
void isa_vmenter(hypervisor_t* hv, vmcs_t* vmcs) {
    // 1. Save host state (implicit in real hardware)
    
    // 2. Load guest state from VMCS
    guest->vcpu.registers[0] = vmcs->guest_rax;
    guest->vcpu.registers[1] = vmcs->guest_rbx;
    guest->vcpu.pc = vmcs->guest_pc;
    guest->vcpu.priv = vmcs->guest_priv;
    
    // 3. Switch to guest mode
    hv->mode = MODE_GUEST;
    guest->vcpu.state = GUEST_RUNNING;
    
    printf("[VMENTER] Guest %u entered (PC=0x%X)\n", guest->vm_id, guest->vcpu.pc);
}
```

### VMEXIT Implementation

When guest tries privileged operations (like VMENTER itself), it traps:

```c
case OP_VMENTER:
    // Guest shouldn't execute VMENTER!
    hv->mode = MODE_HOST;
    guest->vcpu.state = GUEST_BLOCKED;
    guest->vcpu.last_exit_cause = VMCAUSE_PRIVILEGED_INSTRUCTION;
    printf("[VMEXIT] Privileged instruction trap\n");
    break;
```

This is **trap-and-emulate**: Guest tries privileged op → hypervisor catches it → handles it

---

## Part 9: Scheduler (Round-Robin Time-Slicing)

### How the Scheduler Works

**Key insight**: Modern CPUs can't run multiple processes truly in parallel (on single core).
Solution: Give each guest small time quantum, switch rapidly.

```
Pseudocode (from main.c):

total_ticks = 0
while any_guest_running:
    for each guest in order:
        if guest_not_stopped:
            TIME_SLICE = 2  // Each guest runs 2 instructions
            
            while instructions_in_slice < TIME_SLICE and guest_running:
                fetch_and_execute_one_instruction()
            
            print "[TICK %u] Guest %u completed %d instructions"
            total_ticks++
```

### Example Timeline (2 guests, 2-instruction slice)

```
TICK 0: Guest 0 → 2 instructions (total: 2)
TICK 1: Guest 1 → 2 instructions (total: 2)
TICK 2: Guest 0 → 2 instructions (total: 4)
TICK 3: Guest 1 → 2 instructions (total: 4)
TICK 4: Guest 0 → 1 instruction  (total: 5, then HALT)
TICK 5: Guest 1 → 1 instruction  (total: 4, then HALT)

Result:
- Guest 0 executed 5 instructions across 3 ticks
- Guest 1 executed 4 instructions across 2 ticks
- Fair CPU time distribution
```

---

## Part 10: Complete Execution Walkthrough

### Starting the System

```bash
.\build\vISA.exe examples\programs\guest1.bin examples\programs\guest2.bin
```

### What Happens Inside

**1. Program loads and calls main()**
```c
int main(int argc, char* argv[]) {
    hypervisor_t* hv = hypervisor_create();
    // hv initialized: mode=HOST, 0 guests, 64KB memory
```

**2. Load guests**
```c
for (int i = 1; i < argc; i++) {
    uint32_t guest_id = hypervisor_create_guest(hv, argv[i]);
    // Reads guest1.bin (20 bytes) → guest 0
    // Reads guest2.bin (16 bytes) → guest 1
}
```

**3. Start scheduler**
```c
while (!all_stopped) {
    for (uint32_t i = 1; i <= hv->guest_count; i++) {
        // Execute guest time slice
        // Print trace for each instruction
    }
}
```

**4. Guest execution details**

For each instruction in the slice:
```
a) Load instruction from guest_memory[PC]
b) Decode: extract opcode, rd, rs1, rs2
c) Switch on opcode (ADD, MOV, HALT, etc.)
d) Execute: modify guest registers/memory
e) Increment PC
f) Print trace line
```

**5. When guest hits HALT**
```c
case OP_HALT:
    guest->vcpu.state = GUEST_STOPPED;
    printf("[HALT] Guest stopped\n");
    break;
```

Scheduler skips stopped guests. When all stopped, exit loop.

**6. Print final statistics**
```
[HYPERVISOR STATE]
- Guests: 2/4
- Each guest's PC, registers, instruction count, state
```

---

## Part 11: Key Concepts Summary

| Concept | What | Why |
|---------|------|-----|
| **ISA** | 22 instruction definitions | Define what software can do |
| **Hypervisor** | Host software running all guests | Multiplexes guests on single CPU |
| **vCPU** | Virtual CPU (guest's registers+state) | Each guest gets isolated CPU |
| **VMCS** | Save/restore guest state | Hypervisor remembers guest state across context switches |
| **Two-level translation** | VA→PA→HA mapping | Each guest gets isolated memory |
| **Scheduler** | Time-slicing (round-robin) | Fair CPU distribution |
| **VMENTER** | Enter guest mode | Start guest execution |
| **VMEXIT** | Exit to hypervisor | Guest traps, hypervisor handles |
| **Page table** | VA→PA lookup table | Virtual memory implementation |

---

## Part 12: Running Your Own Programs

### Create a guest program (my_prog.isa)
```
mov r0, r0          ; Set r0 to 0
mov r1, r1          ; Set r1 to 0
add r2, r0, r1      ; r2 = r0 + r1 = 0
halt                ; Stop
```

### Assemble it
```bash
cd d:\Programming\vISA\examples
python assembler.py programs/my_prog.isa programs/my_prog.bin
```

### Run with hypervisor
```bash
cd d:\Programming\vISA
.\build\vISA.exe examples\programs\my_prog.bin
```

### What you'll see
```
[ISA:VMENTER] Entered Guest VM 0
[EXEC] PC=0x00  Op=0x05 rd=0 rs1=0 rs2=0
  MOV r0 = r0 (value 0x0)
[EXEC] PC=0x04  Op=0x05 rd=1 rs1=1 rs2=0
  MOV r1 = r1 (value 0x0)
[EXEC] PC=0x08  Op=0x01 rd=2 rs1=0 rs2=1
  ADD r2 = r0(0x0) + r1(0x0) = 0x0
[EXEC] PC=0x0C  Op=0xFF rd=0 rs1=0 rs2=0

[HYPERVISOR] Guest VM 0 stopped after 4 instructions
```

---

## Summary

vISA is a **complete, functional hypervisor** demonstrating:
1. **ISA Design**: 22 instructions for arithmetic, memory, control flow, and virtualization
2. **Binary Encoding**: Fixed 4-byte instruction format
3. **Assembler**: Converts human-readable assembly to binary
4. **Hypervisor Core**: VMENTER/VMEXIT, trap-and-emulate
5. **Memory Isolation**: Two-level address translation
6. **CPU Scheduling**: Round-robin time-slicing
7. **Multiple VMs**: 4 guest VMs can run simultaneously

It's a miniature version of real hypervisors like KVM, Xen, or VMware!
