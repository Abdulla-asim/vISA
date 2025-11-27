#ifndef ISA_H
#define ISA_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================
   HYPERVISOR VIRTUALIZATION SUPPORT 
   ============================================ */

/* ISA Configuration */
#define REGISTER_COUNT 32
#define MEMORY_SIZE (64 * 1024)      /* 64 KB host physical memory */
#define GUEST_VIRT_MEMORY_SIZE (4 * 1024 * 1024)  /* 4 MB guest virtual space */
#define GUEST_PHYS_MEMORY_SIZE (16 * 1024)         /* 16 KB guest physical space */
#define PAGE_SIZE 4096               /* 4 KB pages */
#define MAX_GUESTS 4                 /* Max guest VMs */
#define INSTRUCTION_SIZE 4           /* 4 bytes per instruction */

/* ============ EXECUTION MODES ============ */
typedef enum {
    MODE_HOST = 0,      /* Hypervisor/Host mode */
    MODE_GUEST = 1      /* Guest VM mode */
} execution_mode_t;

/* Privilege Levels */
typedef enum {
    PRIV_USER = 0,
    PRIV_KERNEL = 1
} privilege_level_t;

/* ============ GUEST VM STATES ============ */
typedef enum {
    GUEST_STOPPED = 0,
    GUEST_RUNNING = 1,
    GUEST_BLOCKED = 2,
    GUEST_PAUSED = 3
} guest_state_t;

/* ============ VM EXIT CAUSES ============ */
typedef enum {
    VMCAUSE_NONE = 0x00,
    VMCAUSE_PRIVILEGED_INSTRUCTION = 0x01,  /* Guest tried privileged op */
    VMCAUSE_IO_INSTRUCTION = 0x02,           /* Guest I/O instruction */
    VMCAUSE_PAGE_FAULT = 0x03,               /* Guest page fault */
    VMCAUSE_ILLEGAL_INSTRUCTION = 0x04,      /* Illegal opcode */
    VMCAUSE_CR_WRITE = 0x05,                 /* Guest modified CR */
    VMCAUSE_TIMER = 0x06,                    /* Timer interrupt */
    VMCAUSE_EXTERNAL_INTERRUPT = 0x07,       /* External interrupt */
} vmcause_t;

/* ============ VM TRAP CONFIGURATION BITMASK ============ */
typedef enum {
    VMTRAPCFG_PRIVILEGED_INSTR = (1 << 0),   /* Trap privileged instructions */
    VMTRAPCFG_CR_WRITE = (1 << 1),           /* Trap CR/page table writes */
    VMTRAPCFG_IO_INSTR = (1 << 2),           /* Trap I/O instructions */
    VMTRAPCFG_PAGE_FAULT = (1 << 3),         /* Trap page faults */
} vmtrapcfg_bits_t;

/* ============ INTERRUPT TYPES ============ */
typedef enum {
    IRQ_SYSCALL = 0x01,
    IRQ_PAGE_FAULT = 0x02,
    IRQ_DIVIDE_BY_ZERO = 0x03,
    IRQ_INVALID_INSTRUCTION = 0x04,
    IRQ_TIMER = 0x05,
    IRQ_IO = 0x06
} interrupt_type_t;

/* ============ INSTRUCTION TYPES ============ */
typedef enum {
    /* Standard instructions */
    OP_ADD = 0x01,
    OP_SUB = 0x02,
    OP_MUL = 0x03,
    OP_DIV = 0x04,
    OP_MOV = 0x05,
    OP_LOAD = 0x06,
    OP_STORE = 0x07,
    OP_JMP = 0x08,
    OP_JEQ = 0x09,
    OP_JNE = 0x0A,
    OP_CALL = 0x0B,
    OP_RET = 0x0C,
    
    /* System instructions */
    OP_SYSCALL = 0x20,     /* System call */
    OP_HYPERCALL = 0x21,   /* Hypercall */
    
    /* ============ VIRTUALIZATION ISA INSTRUCTIONS ============ */
    OP_VMENTER = 0x30,      /* Enter guest: vmenter vmcs_ptr */
    OP_VMRESUME = 0x31,     /* Resume guest: vmresume vmcs_ptr */
    OP_VMCAUSE = 0x32,      /* Read exit cause: vmcause rd */
    OP_VMTRAPCFG = 0x33,    /* Set trap config: vmtrapcfg rs */
    OP_LDPGTR = 0x34,       /* Load guest page table root: ldpgtr rs */
    OP_LDHPTR = 0x35,       /* Load host page table root: ldhptr rs */
    OP_TLBFLUSHV = 0x36,    /* Flush guest TLB: tlbflushv */
    
    OP_HALT = 0xFF
} opcode_t;

/* ============ HYPERCALL TYPES ============ */
typedef enum {
    HYPERCALL_PRINT = 1,
    HYPERCALL_READ_MEM = 2,
    HYPERCALL_WRITE_MEM = 3,
    HYPERCALL_EXIT = 4
} hypercall_number_t;

/* Instruction Structure (32-bit) */
typedef struct {
    uint8_t opcode;
    uint8_t rd;      /* destination register */
    uint8_t rs1;     /* source register 1 */
    uint8_t rs2;     /* source register 2 */
} instruction_t;

/* ============ GUEST PAGE TABLE ============ */
typedef struct {
    uint32_t guest_physical_page;  /* Guest physical page (translated by guest) */
    bool present;
    bool writable;
    bool accessed;
    bool dirty;
} guest_page_table_entry_t;

/* ============ HOST PAGE TABLE ============ */
typedef struct {
    uint32_t host_physical_page;   /* Host physical page */
    bool present;
    bool writable;
} host_page_table_entry_t;

/* ============ EXTENDED PAGE TABLE (EPT/NPT) ============ */
/* Maps guest physical → host physical (hypervisor-managed) */
typedef struct {
    uint32_t host_physical_page;
    bool present;
    bool writable;
} ept_entry_t;

/* ============ VIRTUAL MACHINE CONTROL STRUCTURE (VMCS) ============ */
/* Stores complete guest state for save/restore */
typedef struct {
    uint32_t vmcs_id;
    
    /* Guest CPU State */
    uint32_t guest_rax, guest_rbx, guest_rcx, guest_rdx;
    uint32_t guest_rsi, guest_rdi, guest_rbp, guest_rsp;
    uint32_t guest_r8, guest_r9, guest_r10, guest_r11;
    uint32_t guest_r12, guest_r13, guest_r14, guest_r15;
    uint32_t guest_pc;      /* Program counter */
    uint32_t guest_flags;   /* Flags register */
    
    /* Memory Management */
    uint32_t guest_pgtbl_root;  /* Guest page table base (CR3 equivalent) */
    uint32_t host_pgtbl_root;   /* Host page table base */
    
    /* Guest Privilege */
    uint8_t guest_priv;     /* PRIV_USER or PRIV_KERNEL */
    
    /* Exit Information */
    vmcause_t exit_cause;       /* Why did guest exit? */
    uint32_t exit_qualification; /* Additional exit info */
    
    /* Trap Configuration */
    uint32_t trap_config;   /* Bitmask of which events cause VMEXIT */
    
} vmcs_t;

/* ============ GUEST VIRTUAL CPU (vCPU) ============ */
typedef struct {
    uint32_t guest_id;
    
    /* Guest CPU State */
    uint32_t registers[REGISTER_COUNT];
    uint32_t pc;              /* Guest program counter */
    uint32_t sp;              /* Guest stack pointer */
    privilege_level_t priv;   /* Guest privilege level */
    
    /* Guest Memory Management */
    guest_page_table_entry_t guest_page_table[GUEST_VIRT_MEMORY_SIZE / PAGE_SIZE];
    uint32_t guest_pgtbl_root;    /* Guest page table base (CR3 equiv) */
    
    /* Host Memory Management */
    uint32_t host_pgtbl_root;     /* Host page table base */
    
    /* Virtual Machine Control Structure */
    vmcs_t vmcs;              /* Stores guest state for context switching */
    
    /* TLB (Translation Lookaside Buffer) */
    uint32_t tlb_entries;     /* Number of cached translations */
    bool tlb_valid;           /* TLB state valid */
    
    /* Guest State */
    guest_state_t state;
    vmcause_t last_exit_cause;  /* Last VMEXIT reason */
    
} vcpu_t;

/* ============ GUEST VM ============ */
typedef struct {
    uint32_t vm_id;
    vcpu_t vcpu;              /* Virtual CPU */
    
    /* Guest Memory */
    uint8_t guest_memory[GUEST_PHYS_MEMORY_SIZE];  /* Guest physical memory */
    ept_entry_t ept[GUEST_PHYS_MEMORY_SIZE / PAGE_SIZE];  /* Extended page table */
    
    /* Metadata */
    guest_state_t state;
    uint32_t instruction_count;
} guest_vm_t;

/* ============ HOST HYPERVISOR ============ */
typedef struct hypervisor_t {
    /* Host state */
    execution_mode_t mode;
    uint32_t current_guest_id;
    
    /* Guest VMs */
    guest_vm_t guests[MAX_GUESTS];
    uint32_t guest_count;
    
    /* Host memory */
    uint8_t host_memory[MEMORY_SIZE];
    host_page_table_entry_t host_page_table[MEMORY_SIZE / PAGE_SIZE];
    
    /* Scheduling */
    uint32_t tick_count;
    bool halted;
} hypervisor_t;

/* ============ HYPERVISOR ISA INSTRUCTION HANDLERS ============ */

/* VMENTER vmcs_ptr - Enter guest mode */
void isa_vmenter(hypervisor_t* hv, vmcs_t* vmcs);

/* VMRESUME vmcs_ptr - Resume guest after VMEXIT */
void isa_vmresume(hypervisor_t* hv, vmcs_t* vmcs);

/* VMCAUSE rd - Read exit cause into register */
uint32_t isa_vmcause(hypervisor_t* hv);

/* VMTRAPCFG rs - Set trap configuration bitmask */
void isa_vmtrapcfg(hypervisor_t* hv, uint32_t trap_config);

/* LDPGTR rs - Load guest page table root */
void isa_ldpgtr(hypervisor_t* hv, uint32_t guest_pgtbl);

/* LDHPTR rs - Load host page table root */
void isa_ldhptr(hypervisor_t* hv, uint32_t host_pgtbl);

/* TLBFLUSHV - Flush guest TLB entries */
void isa_tlbflushv(hypervisor_t* hv);

/* ============ HYPERVISOR CORE API ============ */
hypervisor_t* hypervisor_create(void);
void hypervisor_destroy(hypervisor_t* hv);

/* Guest VM Management */
uint32_t hypervisor_create_guest(hypervisor_t* hv, const char* guest_image);
void hypervisor_run_guest(hypervisor_t* hv, uint32_t guest_id);

/* Memory Translation */
uint32_t guest_translate_address(guest_vm_t* guest, uint32_t guest_virt_addr);
uint32_t host_translate_address(hypervisor_t* hv, uint32_t guest_phys_addr);

/* Debugging */
void hypervisor_dump_state(hypervisor_t* hv);
void guest_dump_state(guest_vm_t* guest);

#endif /* ISA_H */
