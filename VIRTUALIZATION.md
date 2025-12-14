# Hypervisor-Based Virtualization Architecture

This document explains the hypervisor virtualization system in vISA, which runs multiple isolated guest virtual machines.

## Architecture Overview

vISA implements **hypervisor-based virtualization** where:
- The **Hypervisor** (host mode) manages all resources and guest VMs
- Each **Guest VM** runs in isolated guest mode with:
  - 32 virtual CPU registers (r0-r31)
  - 16 KB isolated guest physical memory
  - Separate page tables (2-level translation)
  - Virtual VMCS (VM Control Structure)
  - Independent execution state

## Core Concepts

### 1. **Virtual CPU (vCPU)**

Each guest has an isolated vCPU with:
- 32 general-purpose registers (r0-r31)
- Program counter (PC)
- Guest state tracking
- Virtual VMCS (VM Control Structure)

```c
typedef struct {
    uint32_t registers[32];      /* Guest CPU registers */
    uint32_t pc;                 /* Program counter */
    guest_state_t state;         /* RUNNING, STOPPED, BLOCKED, PAUSED */
    vmcs_t vmcs;                 /* VM Control Structure */
} vcpu_t;
```

### 2. **Guest Memory Model**

Each guest has:
- **Virtual Address Space**: 4 MB (guest-visible)
- **Physical Memory**: 16 KB (actual allocated)
- **2-Level Translation**:
  - Guest page table: Virtual → Guest Physical
  - Hypervisor: Guest Physical → Host Physical

```c
/* Guest virtual address translation */
uint32_t guest_phys = guest_translate_address(guest, guest_virt);

/* Access guest memory */
uint8_t data = guest->guest_memory[guest_phys];
```

### 3. **Context Switching & Scheduling**

- **Round-robin scheduling** with 2-instruction time slices
- Automatic context switching between guests
- Guest states: RUNNING → STOPPED/BLOCKED → PAUSED

```c
/* Main hypervisor loop */
while (!all_stopped) {
    for (each guest) {
        if (guest->state == GUEST_RUNNING) {
            run_guest_time_slice(guest, 2);  /* 2 instructions per slice */
        }
    }
}
```

### 4. **Privilege Levels** (Defined but not enforced in current impl)

- **PRIV_KERNEL** (1) - Full access (hypervisor)
- **PRIV_USER** (0) - Limited (guests)

### 5. **VM Exit Causes**

Guests trigger VM exits for special events:
- **VMCAUSE_PRIVILEGED_INSTRUCTION** - Guest tried privileged op
- **VMCAUSE_IO_INSTRUCTION** - Guest I/O instruction
- **VMCAUSE_PAGE_FAULT** - Guest page fault
- **VMCAUSE_ILLEGAL_INSTRUCTION** - Unknown opcode
- **VMCAUSE_CR_WRITE** - Guest modified page table
- **VMCAUSE_TIMER** - Time slice expired

```c
/* Guest attempts privileged instruction */
if (instr.opcode == OP_PRIVILEGED) {
    guest->vcpu.last_exit_cause = VMCAUSE_PRIVILEGED_INSTRUCTION;
    guest->vcpu.state = GUEST_BLOCKED;  /* Halt guest */
}
```

## Multi-Guest Execution

### Example: Running Two Guests

```bash
./vISA guest1.bin guest2.bin
```

**Execution Flow:**
1. Hypervisor creates Guest 1 with 16 KB isolated memory
2. Hypervisor creates Guest 2 with separate 16 KB isolated memory
3. Scheduler alternates between them (2 instructions per time slice)
4. Each guest runs independently
5. Page faults or special events cause VM exits
6. Hypervisor handles exits and resumes guest
7. Process continues until all guests halt

### Address Translation Example

```
Guest 1 Virtual Address: 0x1234
    ↓ (Guest Page Table Lookup)
Guest 1 Physical Address: 0x0234 (within guest's 16KB region)
    ↓ (Access guest memory)
Data accessed from: guest->guest_memory[0x0234]
```

## Extending Virtualization

### Future Enhancements

1. **Virtual Memory with Paging**
   - Swap pages to disk when memory full
   - Implement page replacement algorithms (LRU, FIFO)

2. **Privilege Separation**
   - Kernel mode for system calls
   - User mode for application code
   - Privilege check on sensitive instructions

3. **Inter-Process Communication (IPC)**
   - Pipes, message queues, shared memory
   - Mutex/semaphore primitives

4. **I/O Virtualization**
   - Virtual device drivers
   - Console I/O through system calls
   - Disk I/O abstraction

5. **Advanced Scheduling**
   - Priority-based scheduling
   - Multi-level feedback queues
   - Real-time scheduling

6. **Memory Protection**
   - Copy-on-write for fork()
   - Separate read/write permissions per page
   - Guard pages for stack overflow detection

## Configuration

Key constants in `include/isa.h`:

```c
#define MEMORY_SIZE (64 * 1024)           /* Physical memory */
#define VIRT_MEMORY_SIZE (4 * 1024 * 1024) /* Per-process virtual space */
#define PAGE_SIZE 4096                     /* Memory page size */
#define MAX_PROCESSES 16                   /* Maximum simultaneous processes */
#define REGISTER_COUNT 32                  /* Registers per process */
```

Adjust these for your needs (e.g., more processes, larger virtual space).
