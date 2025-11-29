# vISA Architecture Diagrams

## 1. High-Level System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     Host System (64KB)                       │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  HYPERVISOR (Host Mode)                              │   │
│  │  ├─ Scheduler (round-robin time-slicing)            │   │
│  │  ├─ VMENTER/VMEXIT handler                          │   │
│  │  ├─ Memory manager                                   │   │
│  │  └─ Instruction execution engine                     │   │
│  └──────────────────────────────────────────────────────┘   │
│                            ↓                                 │
│  ┌──────────────┬──────────────┬──────────────┬───────────┐ │
│  │   Guest 0    │   Guest 1    │   Guest 2    │  Guest 3  │ │
│  │   (16KB)     │   (16KB)     │   (16KB)     │  (16KB)   │ │
│  │   r0-r31     │   r0-r31     │   r0-r31     │ r0-r31    │ │
│  │   Memory     │   Memory     │   Memory     │ Memory    │ │
│  │   Page Tbl   │   Page Tbl   │   Page Tbl   │ Page Tbl  │ │
│  │   VMCS       │   VMCS       │   VMCS       │ VMCS      │ │
│  └──────────────┴──────────────┴──────────────┴───────────┘ │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## 2. Instruction Encoding

```
┌─ 4-byte Instruction Format ─┐
├─────────────────────────────┤
│ Byte 0: OPCODE (8-bit)      │  Example: ADD r2, r0, r1
│ Byte 1: rd (register, 5-bit)│  Opcode: 0x01
│ Byte 2: rs1 (register, 5-bit)  rd: 2 (r2)
│ Byte 3: rs2 (register, 5-bit)  rs1: 0 (r0)
└─────────────────────────────┘  rs2: 1 (r1)
                                   
Binary: 0x01 0x02 0x00 0x01
Hex:    01020001
```

## 3. Execution Pipeline (What happens when guest runs)

```
┌─────────────────────────────────────────────────────────┐
│                   Hypervisor Main Loop                  │
│                                                         │
│  1. SELECT GUEST (round-robin)                         │
│     ↓                                                   │
│  2. CHECK IF GUEST RUNNING                             │
│     ├─ No → SKIP TO NEXT GUEST                         │
│     └─ Yes → CONTINUE                                  │
│     ↓                                                   │
│  3. FETCH INSTRUCTION from guest_memory[PC]            │
│     ├─ Decode: opcode, rd, rs1, rs2                    │
│     ↓                                                   │
│  4. EXECUTE INSTRUCTION                                │
│     ├─ ADD: reg[rd] = reg[rs1] + reg[rs2]             │
│     ├─ MOV: reg[rd] = reg[rs1]                        │
│     ├─ LOAD: reg[rd] = memory[reg[rs1]]               │
│     ├─ HALT: guest.state = STOPPED                    │
│     └─ ...other instructions                           │
│     ↓                                                   │
│  5. UPDATE PC += 4 bytes                               │
│     ↓                                                   │
│  6. DECREMENT TIME SLICE                               │
│     ├─ Slice done? → YIELD TO NEXT GUEST              │
│     └─ Slice remaining? → LOOP TO STEP 3              │
│     ↓                                                   │
│  7. ALL GUESTS STOPPED? → EXIT                         │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## 4. Memory Address Translation (Two-Level)

```
Guest 0 Virtual Address: 0x1234
            ↓
        ┌───────────────────────────┐
        │  Page Table Lookup        │
        │  Page = 0x1234 / 4096 = 0 │
        │  Entry.present = true      │
        │  Entry.phys_page = 0       │
        └───────────────────────────┘
            ↓
Guest 0 Physical Address: 0 * 4096 + 0x234 = 0x0234
            ↓
        ┌──────────────────────────┐
        │ Direct Access (no 3rd level) │
        │ guest_memory[0x0234]      │
        └──────────────────────────┘

VS.

Guest 1 Virtual Address: 0x1234 (same as Guest 0!)
            ↓
        ┌───────────────────────────┐
        │  Page Table Lookup        │
        │  Page = 0x1234 / 4096 = 0 │
        │  Entry.present = true      │
        │  Entry.phys_page = 0       │
        └───────────────────────────┘
            ↓
Guest 1 Physical Address: 0 * 4096 + 0x234 = 0x0234
            ↓
        ┌──────────────────────────┐
        │ DIFFERENT guest_memory!  │
        │ (Guest 1's 16KB block)    │
        │ guest_memory[0x0234]      │
        └──────────────────────────┘

RESULT: Same virtual address → DIFFERENT physical memory
        → Automatic process isolation!
```

## 5. Scheduler Timeline (2 guests, 2-instruction slice)

```
Time:    Instructions:
│        ┌─────────────────┐
├─→ 0    │ Guest 0: 2 instr│  r0, r1, ...
│        └─────────────────┘
│        ┌─────────────────┐
├─→ 1    │ Guest 1: 2 instr│  r5, r6, ...
│        └─────────────────┘
│        ┌─────────────────┐
├─→ 2    │ Guest 0: 2 instr│  ADD, MOV, ...
│        └─────────────────┘
│        ┌─────────────────┐
├─→ 3    │ Guest 1: 2 instr│  ADD, HALT
│        └─────────────────┘
│        ┌─────────────────┐
├─→ 4    │ Guest 0: 1 instr│  HALT (done)
│        └─────────────────┘

Final State:
  Guest 0: 5 instructions, STOPPED
  Guest 1: 4 instructions, STOPPED
  Scheduler: 5 total ticks
```

## 6. VMENTER/VMEXIT Transition

```
             HOST MODE                      GUEST MODE
        (Hypervisor Running)            (Guest Running)

    ┌──────────────────────┐
    │                      │
    │   Hypervisor         │
    │   - Mode = HOST      │
    │   - Controls ALL CPUs│
    │   - Trap handler     │
    │                      │
    └──────────────────────┘
            ↓
         VMENTER (execute guest)
         ├─ Save host state (implicit)
         ├─ Load guest state from VMCS
         ├─ Set mode = GUEST
         └─ Jump to guest PC
            ↓
    ┌──────────────────────┐
    │                      │
    │   Guest 0            │
    │   - Mode = GUEST     │
    │   - Executes own code│
    │   - Can't do privs   │
    │                      │
    └──────────────────────┘
            ↓
         VMEXIT (trap)
         ├─ Guest tries VMENTER (privileged!)
         ├─ Hypervisor catches it
         ├─ Set mode = HOST
         ├─ Save guest state to VMCS
         └─ Return to hypervisor
            ↓
    Back to HOST MODE
    ├─ Record exit reason
    ├─ Can handle trap if needed
    ├─ Prepare guest for resume
    └─ Loop back to scheduler
```

## 7. Data Structure Relationships

```
hypervisor_t (THE HOST)
│
├─ host_memory[64KB]
│  │
│  └─ Guest memories are slices of this
│
├─ guests[4] array of guest_vm_t
│  │
│  ├─ guests[0]
│  │  ├─ guest_memory[16KB]
│  │  │  └─ Contains binary code/data
│  │  │
│  │  └─ vcpu_t
│  │     ├─ registers[32]
│  │     ├─ pc (program counter)
│  │     ├─ guest_page_table[4096]
│  │     ├─ vmcs (guest state snapshot)
│  │     └─ state (RUNNING/STOPPED/etc)
│  │
│  ├─ guests[1]
│  │  └─ (similar structure)
│  │
│  ├─ guests[2]
│  │  └─ (similar structure)
│  │
│  └─ guests[3]
│     └─ (similar structure)
│
├─ mode (HOST or GUEST)
├─ current_guest_id
└─ tick_count
```

## 8. Instruction Execution Dispatch Table

```
Fetched Opcode
      ↓
   Switch(opcode)
      ├─ 0x01 → ADD: reg[rd] = reg[rs1] + reg[rs2]
      ├─ 0x02 → SUB: reg[rd] = reg[rs1] - reg[rs2]
      ├─ 0x03 → MUL: reg[rd] = reg[rs1] * reg[rs2]
      ├─ 0x04 → DIV: reg[rd] = reg[rs1] / reg[rs2]
      ├─ 0x05 → MOV: reg[rd] = reg[rs1]
      ├─ 0x06 → LOAD: reg[rd] = mem[reg[rs1]]
      ├─ 0x07 → STORE: mem[reg[rs1]] = reg[rs2]
      ├─ 0x08 → JMP: pc = reg[rs1]
      ├─ 0x09 → JEQ: if reg[rs1]==reg[rs2]: pc=reg[rd]
      ├─ 0x0A → JNE: if reg[rs1]!=reg[rs2]: pc=reg[rd]
      ├─ 0x0B → CALL: push_pc(); pc = reg[rs1]
      ├─ 0x0C → RET: pc = pop_stack()
      ├─ 0x20 → SYSCALL: VMEXIT
      ├─ 0x21 → HYPERCALL: VMEXIT
      ├─ 0x30 → VMENTER: Enter guest (hypervisor only)
      ├─ 0x31 → VMRESUME: Resume guest (hypervisor only)
      ├─ 0x32 → VMCAUSE: Read exit cause
      ├─ 0x33 → VMTRAPCFG: Set trap config
      ├─ 0x34 → LDPGTR: Load guest page table
      ├─ 0x35 → LDHPTR: Load host page table
      ├─ 0x36 → TLBFLUSHV: Flush TLB
      ├─ 0xFF → HALT: guest.state = STOPPED
      └─ default → Illegal instruction!
```

## 9. File Organization

```
d:\Programming\vISA\
│
├─ include\
│  └─ isa.h
│     ├─ Opcode definitions (0x01, 0x02, ..., 0xFF)
│     ├─ Register count, memory sizes
│     ├─ Structure definitions:
│     │  ├─ instruction_t (opcode, rd, rs1, rs2)
│     │  ├─ vcpu_t (registers, PC, page tables, VMCS)
│     │  ├─ vmcs_t (saved guest state)
│     │  ├─ guest_vm_t (one full guest VM)
│     │  └─ hypervisor_t (the host)
│     └─ Function declarations
│
├─ src\
│  ├─ hypervisor_isa.c
│  │  ├─ isa_vmenter()
│  │  ├─ isa_vmresume()
│  │  ├─ Instruction handlers (ADD, MOV, HALT, etc.)
│  │  ├─ guest_translate_address() (VA→PA translation)
│  │  ├─ hypervisor_run_guest() (execution loop)
│  │  └─ Debug functions
│  │
│  └─ main.c
│     ├─ hypervisor_create()
│     ├─ hypervisor_create_guest()
│     ├─ Round-robin scheduler
│     ├─ Instruction tracing
│     └─ Final state dump
│
├─ examples\
│  ├─ assembler.py
│  │  ├─ OPCODES dict (mnemonic → hex)
│  │  └─ Converts .isa text → .bin binary
│  │
│  └─ programs\
│     ├─ guest1.isa / guest1.bin (5 instructions)
│     ├─ guest2.isa / guest2.bin (4 instructions)
│     ├─ guest3.isa / guest3.bin (5 instructions)
│     ├─ arithmetic.isa / arithmetic.bin (4 instructions)
│     ├─ hypervisor_example.isa / hypervisor_example.bin
│     ├─ long1.isa / long1.bin (9 instructions)
│     └─ long2.isa / long2.bin (8 instructions)
│
├─ build\
│  └─ vISA.exe (compiled hypervisor executable)
│
└─ CMakeLists.txt (build configuration)
```

## 10. Complete Example Trace

Program: 2 guests, 2-instruction time slice

```
Input:
  guest1.bin = [MOV r0,r0 | MOV r1,r1 | ADD r2,r0,r1 | MOV r3,r2 | HALT]
  guest2.bin = [MOV r5,r5 | MOV r6,r6 | ADD r7,r5,r6 | HALT]

Execution Timeline:

[HYPERVISOR] Initialized (64 KB host memory, max 4 guests)
[HYPERVISOR] Loaded Guest 0 (20 bytes)
[HYPERVISOR] Loaded Guest 1 (16 bytes)

[SCHEDULER] Starting time-sliced execution (2 instructions per slice)

[TICK 0] Running Guest VM 0 time slice...
  [G0:0x00] MOV r0 r0 r0          ← PC=0x00, execute, PC→0x04
  [G0:0x04] MOV r1 r1 r0          ← PC=0x04, execute, PC→0x08
  [Guest 0 completed 2 instructions this slice, total: 2]

[TICK 1] Running Guest VM 1 time slice...
  [G1:0x00] MOV r5 r5 r0
  [G1:0x04] MOV r6 r6 r0
  [Guest 1 completed 2 instructions this slice, total: 2]

[TICK 2] Running Guest VM 0 time slice...
  [G0:0x08] ADD r2 r0 r1
  [G0:0x0C] MOV r3 r2 r0
  [Guest 0 completed 2 instructions this slice, total: 4]

[TICK 3] Running Guest VM 1 time slice...
  [G1:0x08] ADD r7 r5 r6
  [G1:0x0C] HALT r0 r0 r0         ← Guest 1 stops here
  [Guest 1 completed 2 instructions this slice, total: 4]

[TICK 4] Running Guest VM 0 time slice...
  [G0:0x10] HALT r0 r0 r0         ← Guest 0 stops here
  [Guest 0 completed 1 instructions this slice, total: 5]

[SCHEDULER] All guests stopped after 5 scheduling rounds

Final State:
  Guest 0: PC=0x14, instructions=5, state=STOPPED
  Guest 1: PC=0x10, instructions=4, state=STOPPED
  System: mode=HOST, ticks=5
```

This shows **fair CPU distribution**: each guest got CPU time in round-robin fashion!
