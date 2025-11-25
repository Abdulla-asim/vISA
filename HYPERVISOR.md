# Hypervisor Virtualization Architecture

## What is Hypervisor Virtualization?

Unlike memory virtualization (which just isolates process address spaces), **hypervisor virtualization** allows you to run **complete isolated virtual machines** on top of a host system. Think of it like:

- **Memory virtualization**: Multiple processes on one OS
- **Hypervisor virtualization**: Multiple OSes on one computer (VMware, KVM, Hyper-V)

---

## Architecture Overview

```
┌────────────────────────────────────────────────────┐
│              HYPERVISOR (Host)                     │
│  - Manages physical resources                      │
│  - Schedules guest VMs                             │
│  - Handles VM Exits                                │
│  - Emulates hardware                               │
├────────────────────────────────────────────────────┤
│  ┌──────────────────┐  ┌──────────────────┐        │
│  │  Guest VM 1      │  │  Guest VM 2      │ ...    │
│  │  ┌────────────┐  │  │  ┌────────────┐  │        │
│  │  │ vCPU       │  │  │  │ vCPU       │  │        │
│  │  │ Registers  │  │  │  │ Registers  │  │        │
│  │  │ Page Table │  │  │  │ Page Table │  │        │
│  │  └────────────┘  │  │  └────────────┘  │        │
│  │  Guest Memory    │  │  Guest Memory    │        │
│  │  (16 KB each)    │  │  (16 KB each)    │        │
│  └──────────────────┘  └──────────────────┘        │
├────────────────────────────────────────────────────┤
│        Host Physical Memory (64 KB total)          │
│        ┌─────────────────────────────┐             │
│        │ Guest1 Pages  │ Guest2 Pages│...          │
│        └─────────────────────────────┘             │
└────────────────────────────────────────────────────┘
```

---

## Key Concepts

### 1. **VM Entry/Exit**

The CPU switches between host and guest modes:

```c
VMENTRY:  Host → Guest
  - Load guest CPU state (registers, PC)
  - Switch to guest page tables
  - Start executing guest code

VMEXIT:   Guest → Host (due to special events)
  - Save guest state
  - Switch back to host
  - Handle the cause of exit
  - Resume guest when ready
```

**Example:**
```c
vmentry(hv, guest);           /* Start running guest */
vm_execute_instruction();     /* Execute guest instruction */
/* Guest tries to access I/O device or calls hypercall */
vmexit(hv, guest, VMEXIT_IO); /* Exit back to host */
handle_vmexit();              /* Host handles it */
vmentry(hv, guest);           /* Resume guest */
```

### 2. **VM Exit Reasons**

Guests exit to hypervisor for various reasons:

| Exit Reason | Cause | Handler |
|-------------|-------|---------|
| **VMEXIT_PRIVILEGED_INSTRUCTION** | Guest tried privileged op | Deny or emulate |
| **VMEXIT_IO_INSTRUCTION** | Guest accessed I/O | Emulate device |
| **VMEXIT_PAGE_FAULT** | Virtual page not present | Load from disk |
| **VMEXIT_HYPERCALL** | Guest requests hypervisor | Handle service request |
| **VMEXIT_SYSCALL** | Guest system call | Forward to guest OS |
| **VMEXIT_HALT** | Guest halt instruction | Stop VM |

### 3. **Two-Level Address Translation**

Real hypervisors have this:

```
Guest Virtual Address (GVA)
    ↓ [Guest Page Table]
Guest Physical Address (GPA)
    ↓ [Hypervisor's EPT - Extended Page Table]
Host Physical Address (HPA) ← Actual RAM
```

In your implementation:

```c
/* Guest handles: GVA → GPA */
uint32_t guest_phys_addr = guest_translate_address(guest, guest_virt_addr);

/* Hypervisor handles: GPA → HPA */
uint32_t host_phys_addr = host_translate_address(hv, guest_phys_addr);
```

### 4. **Virtual CPU (vCPU)**

Each guest has its own virtual CPU with complete isolated state:

```c
typedef struct {
    uint32_t registers[32];        /* Isolated registers */
    uint32_t pc;                   /* Program counter */
    guest_page_table_entry_t guest_page_table[...];  /* Page tables */
} vcpu_t;
```

**Key point:** Guest thinks it has a real CPU, but it's simulated by hypervisor.

### 5. **Hypercalls**

Special instruction for guests to request hypervisor services (like syscalls but for VM):

```c
OP_HYPERCALL  /* Instruction that exits to hypervisor */

case HYPERCALL_PRINT:
    /* Guest can't access console directly, asks hypervisor */
    guest_print(hv, buffer);  /* Hypervisor prints */
    break;

case HYPERCALL_EXIT:
    /* Guest asks to be terminated */
    guest->vcpu.state = GUEST_STOPPED;
    break;
```

---

## Execution Flow

### Single Guest Execution

```c
hypervisor_run_guest(hv, guest_id)
    ├─ vmentry()              /* Switch to guest mode */
    ├─ fetch instruction from guest memory
    ├─ execute instruction
    ├─ if (special event)
    │   └─ vmexit()           /* Exit to host */
    │      └─ handle_vmexit() /* Host handles it */
    │         └─ vmentry()    /* Resume guest */
    └─ repeat until HALT
```

### Multiple Guests

```c
for each guest {
    vmentry(guest)
    for 10000 instructions {
        execute guest instruction
    }
    vmexit(guest)            /* Time slice done */
    handle_vmexit()
    switch to next guest
}
```

---

## Configuration

```c
#define MEMORY_SIZE 64KB              /* Host physical memory */
#define GUEST_PHYS_MEMORY_SIZE 16KB   /* Each guest gets 16KB */
#define GUEST_VIRT_MEMORY_SIZE 4MB    /* But can address 4MB virtual */
#define MAX_GUESTS 4                  /* 4 VMs max */
#define PAGE_SIZE 4KB                 /* Page granularity */
```

---

## Comparison: Memory Virtualization vs Hypervisor Virtualization

| Aspect | Memory Virt | Hypervisor Virt |
|--------|------------|-----------------|
| **Isolation** | Process-level | Complete VM |
| **Guest Awareness** | Aware it's a process | Thinks it's on real hardware |
| **Page Tables** | Single per process | Double (guest + hypervisor) |
| **Special Instructions** | N/A | VMEntry/VMExit |
| **Exit Handling** | Interrupt handlers | VM Exit handlers |
| **Use Case** | Multitasking OS | Running VMs |
| **Examples** | Unix processes, Windows tasks | VMware, KVM, Hyper-V |

---

## Real Hardware Features

Your implementation simulates features found in real CPUs:

- **Intel VT-x**: VMEntry, VMExit, EPT (Extended Page Tables)
- **AMD-V**: VMRUN, VMEXIT, NPT (Nested Page Tables)
- **ARM**: Hypervisor mode, Enhanced Stage-2 translation

---

## Future Enhancements

1. **Device Emulation**
   - Virtual disk controllers
   - Virtual network adapters
   - Virtual timer devices

2. **Shared Memory Between VMs**
   - Virtio-shared-mem protocol
   - Para-virtualized drivers

3. **Live Migration**
   - Save/restore VM state
   - Move running guest to different host

4. **Snapshots & Checkpointing**
   - Save VM state at a point in time
   - Rewind/replay execution

5. **Performance Optimization**
   - Shadow paging (instead of EPT)
   - Instruction caching
   - Batch VM exits
