# Virtualization Architecture

This document explains the virtualization support added to vISA.

## Core Concepts

### 1. **Privilege Levels**
- **PRIV_KERNEL** (1) - Can execute all instructions, modify page tables, handle interrupts
- **PRIV_USER** (0) - Limited instruction set, cannot modify system state directly

```c
typedef enum {
    PRIV_USER = 0,
    PRIV_KERNEL = 1
} privilege_level_t;
```

### 2. **Memory Management Unit (MMU)**

Each process has its own virtual address space (4 MB), mapped to shared physical memory (64 KB) through page tables.

**Key Features:**
- Virtual-to-physical address translation
- Per-process page tables (4 KB pages)
- Page faults detection
- Access tracking (accessed, dirty bits)

**File:** `src/mmu.c`

```c
uint32_t phys_addr = mmu_translate_address(process, virt_addr);
```

### 3. **Process Control Block (PCB)**

Each process has its own:
- 32 registers (isolated state)
- Program counter (PC)
- Stack pointer (SP)
- Page table (virtual memory space)
- Process ID (PID)
- State (READY, RUNNING, BLOCKED, TERMINATED)

```c
typedef struct {
    uint32_t pid;
    process_state_t state;
    privilege_level_t priv;
    uint32_t registers[REGISTER_COUNT];
    uint32_t pc, sp, fp;
    page_table_entry_t page_table[PAGES_PER_PROCESS];
} process_t;
```

### 4. **Context Switching & Scheduling**

- **Round-Robin scheduling** with time slices (1000 instructions)
- Automatic context switching between runnable processes
- Process states: READY → RUNNING → (→ BLOCKED) → TERMINATED

```c
void vm_schedule_next(vm_t* vm);  /* Switch to next process */
void vm_run(vm_t* vm);             /* Main execution loop */
```

### 5. **Interrupts & System Calls**

Processes can trigger interrupts through:
- **IRQ_SYSCALL** - System calls (exit, write, read, malloc, free)
- **IRQ_PAGE_FAULT** - Virtual address not in physical memory
- **IRQ_DIVIDE_BY_ZERO** - Arithmetic error
- **IRQ_INVALID_INSTRUCTION** - Unknown opcode
- **IRQ_TIMER** - Time slice expired
- **IRQ_IO** - I/O operation

```c
void vm_handle_interrupt(vm_t* vm, interrupt_type_t irq, uint32_t data);
```

**System Call Example:**
```c
case SYSCALL_EXIT:
    proc->state = PROC_TERMINATED;
    printf("Process %u exited with code %u\n", proc->pid, proc->registers[0]);
    break;
```

## Virtualization Pipeline

```
┌─────────────────────────────────────────────────────┐
│             Virtual Machine (vm_t)                  │
├─────────────────────────────────────────────────────┤
│ Processes[0..15] (Each with own memory space)      │
│ ┌──────────────┐  ┌──────────────┐                 │
│ │ Process 0    │  │ Process 1    │  ...            │
│ │ Registers    │  │ Registers    │                 │
│ │ PageTable    │  │ PageTable    │                 │
│ │ State        │  │ State        │                 │
│ └──────────────┘  └──────────────┘                 │
├─────────────────────────────────────────────────────┤
│        Physical Memory (64 KB, shared)              │
│        ┌─────────────────────────────┐             │
│        │  Page 0 (Prog 0)            │             │
│        │  Page 1 (Prog 1)            │             │
│        │  Page 2 (Stack 0)           │             │
│        │  ...                        │             │
│        └─────────────────────────────┘             │
└─────────────────────────────────────────────────────┘
```

## Multi-Process Execution

### Example: Running Two Programs

```bash
./vISA program1.bin program2.bin
```

**Execution Flow:**
1. Create Process 1 with virtual address space 0-4MB, map to physical pages
2. Create Process 2 with separate virtual address space 0-4MB, map to different physical pages
3. Scheduler alternates between them (1000 instructions each)
4. Page faults/interrupts trigger context switches
5. Both processes run "concurrently" (preempted scheduling)

### Address Translation Example

```c
Process 1 virtual 0x1000
    ↓
Check Page Table[0] → physical_page=2
    ↓
Physical Address = (2 * 4096) + 0x000 = 0x2000
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
