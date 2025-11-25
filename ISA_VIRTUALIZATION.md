# ISA-Based Virtualization Instructions

This document explains the virtualization support added as **ISA instructions** rather than microarchitecture-only features.

## Overview

Instead of implementing virtualization as hidden hardware features, you now have explicit **ISA instructions** that the hypervisor uses to manage guest VMs. This makes virtualization a first-class part of the ISA design.

```
Host Code:
    vmenter &vmcs1      ; ISA instruction - enter guest
    vmcause r0          ; ISA instruction - read exit reason
    vmtrapcfg r1        ; ISA instruction - set traps
    ldhptr r2           ; ISA instruction - load page table
    vmresume &vmcs1     ; ISA instruction - resume guest
```

---

## Virtualization ISA Instructions

### **1. VMENTER vmcs_ptr**

**Mnemonic:** `vmenter <vmcs_address>`

**Opcode:** `0x30`

**Format:**
```
┌────────┬───────┬────────┬────────┐
│ Opcode │  rd   │  rs1   │  rs2   │
│ 0x30   │ unused│ unused │ unused │
└────────┴───────┴────────┴────────┘
(VMCS pointer passed in register or memory)
```

**Purpose:** Transition from host mode to guest mode

**What it does:**
1. Save host CPU state (implicit, for recovery)
2. Load guest CPU state from VMCS:
   - General-purpose registers (RAX, RBX, RCX, RDX, etc.)
   - Program counter (PC)
   - Privilege level
   - Page table root (CR3 equivalent)
3. Switch CPU to guest execution mode
4. Start executing guest instructions

**Example:**
```c
vmcs_t vmcs = {...};           /* Prepare VMCS with guest state */
isa_vmenter(hv, &vmcs);        /* Execute VMENTER instruction */
/* Now running in guest mode */
```

**Hardware action:**
```
┌─────────────────────────────────┐
│ Host Mode (kernel)              │
│  vmenter &vmcs                  │
└─────────────┬───────────────────┘
              ↓
┌─────────────────────────────────┐
│ Guest Mode (User VM)            │
│  [executing guest instructions] │
└─────────────────────────────────┘
```

---

### **2. VMEXIT (Implicit - Not an instruction)**

**Purpose:** Automatic trap from guest mode back to host mode

**What triggers it:**
- Privileged instruction in guest (if `VMTRAPCFG_PRIVILEGED_INSTR` bit set)
- CR/Page table write in guest (if `VMTRAPCFG_CR_WRITE` bit set)
- I/O instruction in guest (if `VMTRAPCFG_IO_INSTR` bit set)
- Page fault in guest (if `VMTRAPCFG_PAGE_FAULT` bit set)
- Illegal instruction
- Halt instruction

**Hardware action automatically:**
1. Save guest state into VMCS
2. Write exit reason into `VMCAUSE` register
3. Switch back to host mode
4. Jump to hypervisor trap handler

**Example flow:**
```
Guest running:
    mov cr3, r0         ← Tries to modify page table
    
VMEXIT triggered automatically:
    ✗ Save guest state to VMCS
    ✗ Set VMCAUSE = VMCAUSE_CR_WRITE
    ✗ Switch to host mode
    ✗ Hypervisor trap handler invoked
```

---

### **3. VMRESUME vmcs_ptr**

**Mnemonic:** `vmresume <vmcs_address>`

**Opcode:** `0x31`

**Purpose:** Resume guest execution after handling a VMEXIT

**What it does:**
1. Load guest state from VMCS
2. Re-enter guest mode
3. Continue from saved PC

**Unlike VMENTER:**
- VMENTER: Initial entry to guest (from scratch)
- VMRESUME: Resuming after hypervisor handled something

**Example:**
```c
/* Guest exited */
isa_vmcause(hv);                    /* Check why it exited */

/* Hypervisor handles the issue */
hypervisor_emulate_io();            /* Fix page table, emulate I/O, etc. */

/* Resume guest */
isa_vmresume(hv, &vmcs);            /* Continue guest execution */
```

---

### **4. VMCAUSE rd**

**Mnemonic:** `vmcause <dest_register>`

**Opcode:** `0x32`

**Purpose:** Read the exit reason of the last VMEXIT

**What it does:**
1. Read the exit cause from VMCS
2. Place it in destination register

**Exit cause values:**
```c
VMCAUSE_NONE = 0x00
VMCAUSE_PRIVILEGED_INSTRUCTION = 0x01   /* Guest tried privileged op */
VMCAUSE_IO_INSTRUCTION = 0x02           /* Guest I/O instruction */
VMCAUSE_PAGE_FAULT = 0x03               /* Guest page fault */
VMCAUSE_ILLEGAL_INSTRUCTION = 0x04      /* Illegal opcode */
VMCAUSE_CR_WRITE = 0x05                 /* Guest modified CR */
VMCAUSE_TIMER = 0x06                    /* Timer interrupt */
VMCAUSE_EXTERNAL_INTERRUPT = 0x07       /* External interrupt */
```

**Example:**
```c
vmcause r0          ; Read exit cause into R0
cmp r0, #0x02       ; Is it I/O instruction?
je handle_io        ; Jump if yes
cmp r0, #0x03       ; Is it page fault?
je handle_pf        ; Jump if yes
```

---

### **5. VMTRAPCFG rs**

**Mnemonic:** `vmtrapcfg <config_register>`

**Opcode:** `0x33`

**Purpose:** Configure which guest actions cause VMEXIT

**What it does:**
1. Read bitmask from source register
2. Store in VMCS trap configuration
3. Future guest actions matching these bits will trigger VMEXIT

**Configuration bitmask:**
```c
bit0: VMTRAPCFG_PRIVILEGED_INSTR    /* Trap privileged instructions */
bit1: VMTRAPCFG_CR_WRITE            /* Trap page table writes */
bit2: VMTRAPCFG_IO_INSTR            /* Trap I/O instructions */
bit3: VMTRAPCFG_PAGE_FAULT          /* Trap page faults */
```

**Example:**
```c
/* Allow all guest actions (no traps) */
mov r0, #0x00
vmtrapcfg r0

/* Trap page table writes + I/O instructions */
mov r0, #0x06       ; bits 1 + 2
vmtrapcfg r0

/* Trap everything */
mov r0, #0x0F       ; all 4 bits
vmtrapcfg r0
```

---

### **6. LDPGTR rs** (Load guest Page Table Root)

**Mnemonic:** `ldpgtr <register>`

**Opcode:** `0x34`

**Purpose:** Set the guest's page table base (CR3 equivalent)

**What it does:**
1. Read guest page table base address from register
2. Store in VMCS `guest_pgtbl_root`
3. Future guest address translations use this root

**Why needed:**
- Each guest has its own virtual → physical translation
- Guest page tables are stored in guest physical memory
- Hypervisor must set up initial page tables for each guest

**Example:**
```c
mov r0, #0x1000         ; Guest page table at 0x1000
ldpgtr r0               ; Load as guest CR3

/* Now guest address translation:
   Guest VA → [guest_pgtbl_root + offset] → Guest PA
*/
```

---

### **7. LDHPTR rs** (Load Host Page Table Root)

**Mnemonic:** `ldhptr <register>`

**Opcode:** `0x35`

**Purpose:** Set the hypervisor's own page table base

**What it does:**
1. Read host page table base from register
2. Store in VMCS (or host-only register)
3. This is used for **extended page table (EPT)** / nested paging

**Two-level translation:**
```
Guest Virtual Address
    ↓ [Guest page table at LDPGTR]
Guest Physical Address
    ↓ [Host page table at LDHPTR / EPT]
Host Physical Address (actual RAM)
```

**Why needed:**
- Hypervisor needs its own page tables for host memory
- Guests can't modify host page tables
- Enables memory isolation between guests

**Example:**
```c
mov r0, #0x10000        ; Host page table at 0x10000
ldhptr r0               ; Set host CR3

mov r1, #0x20000        ; Guest 1 page table at 0x20000
ldpgtr r1               ; Set guest CR3

/* Now:
   Guest VA → (using 0x20000) → Guest PA → (using 0x10000) → Host PA
*/
```

---

### **8. TLBFLUSHV** (Flush guest TLB)

**Mnemonic:** `tlbflushv`

**Opcode:** `0x36`

**No operands** - implicit

**Purpose:** Invalidate cached guest address translations

**What it does:**
1. Clear all cached guest VA → PA mappings
2. Mark guest TLB as invalid
3. Next guest address translation will go through page tables

**Why needed:**
- When guest changes page tables, old translations are stale
- When hypervisor remaps guest physical memory
- TLB (Translation Lookaside Buffer) = cache of recent translations

**Example:**
```c
/* Guest changes its page table root */
ldpgtr new_pgtbl        ; Load new guest page table

tlbflushv               ; Flush old cached translations
                        ; Forces re-translation through new tables

/* Now guest uses updated page tables */
```

---

## VMCS Structure (Virtual Machine Control Structure)

The VMCS stores all persistent guest state:

```c
typedef struct {
    /* Guest General-Purpose Registers */
    uint32_t guest_rax, guest_rbx, guest_rcx, guest_rdx;
    uint32_t guest_rsi, guest_rdi, guest_rbp, guest_rsp;
    uint32_t guest_r8...r15;
    
    /* Guest Control State */
    uint32_t guest_pc;           /* Program counter */
    uint32_t guest_flags;        /* CPU flags */
    uint8_t guest_priv;          /* Privilege level */
    
    /* Guest Memory Translation */
    uint32_t guest_pgtbl_root;   /* Guest page table base (CR3 equiv) */
    uint32_t host_pgtbl_root;    /* Host page table base */
    
    /* Exit Information */
    vmcause_t exit_cause;        /* Why did guest exit? */
    uint32_t exit_qualification; /* Additional details */
    
    /* Hypervisor Configuration */
    uint32_t trap_config;        /* Trap configuration bitmask */
    
} vmcs_t;
```

---

## Execution Flow with ISA Instructions

### **Hypervisor enters guest:**
```
1. Prepare VMCS with guest state
2. Execute: vmenter &vmcs
   ↓ CPU saves host state
   ↓ CPU loads guest state from VMCS
   ↓ CPU switches to guest mode
3. Guest runs...
```

### **Guest triggers VMEXIT:**
```
1. Guest executes privileged instruction (e.g., "mov cr3, r0")
2. Hardware automatically:
   ↓ Checks if trap bit is set in VMTRAPCFG
   ↓ Saves guest state to VMCS
   ↓ Writes exit reason to VMCAUSE (e.g., VMCAUSE_CR_WRITE)
   ↓ Switches back to host mode
3. Hypervisor trap handler executes
```

### **Hypervisor handles VMEXIT:**
```
1. Execute: vmcause r0
   ↓ Reads exit reason into R0
2. Check reason (e.g., "cmp r0, #0x05" for CR write)
3. Emulate the operation or fix the issue
4. Update VMCS if needed (e.g., new CR value)
```

### **Hypervisor resumes guest:**
```
1. Execute: vmresume &vmcs
   ↓ CPU loads guest state from VMCS
   ↓ CPU switches back to guest mode
   ↓ Guest continues from saved PC
2. Guest resumes execution...
```

---

## Real Hardware Analogies

Your ISA instructions map directly to real CPU features:

| Your ISA Instruction | Intel VT-x | AMD-V | ARM |
|----------------------|-----------|-------|-----|
| VMENTER | VMLAUNCH/VMRESUME | VMRUN | ENTRY |
| VMEXIT | Auto trap | Auto trap | Auto trap |
| VMCAUSE | VMCS Exit Reason | VMCB Exit Code | ESR_EL2 |
| VMTRAPCFG | VMCS Trap Mask | VMCB Intercept | HCR_EL2 |
| LDPGTR | VMCS Guest CR3 | VMCB Guest CR3 | TTBR0_EL1 |
| LDHPTR | VMCS Host CR3 | Host CR3 | TTBR1_EL2 |
| TLBFLUSHV | INVVPID | TLB Flush | TLBI |

---

## Benefits of ISA-Based Virtualization

✅ **Explicit** - Virtualization is part of the ISA, not hidden
✅ **Portable** - Any compiler/assembler can generate these instructions  
✅ **Learnable** - Clear what's happening at instruction level
✅ **Extensible** - Easy to add more VM management instructions later
✅ **Realistic** - Matches how real CPUs handle virtualization

---

## Future Extensions

You could add more ISA instructions for:

- **VMCREATE** - Create new VMCS
- **VMDESTROY** - Destroy VMCS
- **VMIO rd, port** - Guest I/O operations
- **VMCALL** - Guest → Hypervisor calls (like syscall but for VMs)
- **VMIRQ vector** - Inject interrupts into guest
