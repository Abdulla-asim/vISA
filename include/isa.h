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

/* ============ VM EXIT REASONS ============ */
typedef enum {
    VMEXIT_INVALID_INSTRUCTION = 0x01,
    VMEXIT_PRIVILEGED_INSTRUCTION = 0x02,
    VMEXIT_SYSCALL = 0x03,
    VMEXIT_INTERRUPT = 0x04,
    VMEXIT_PAGE_FAULT = 0x05,
    VMEXIT_IO_INSTRUCTION = 0x06,
    VMEXIT_HYPERCALL = 0x07,
    VMEXIT_HALT = 0xFF
} vmexit_reason_t;

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
    OP_SYSCALL = 0x20,     /* System call (guest → host) */
    OP_HYPERCALL = 0x21,   /* Hypercall (guest → hypervisor) */
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
    uint32_t guest_cr3;       /* Guest page table base */
    
    /* Guest State */
    guest_state_t state;
    uint32_t exit_reason;     /* Last VMExit reason */
    uint32_t exit_data;       /* Additional exit data */
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

/* ============ VMCS (Virtual Machine Control Structure) ============ */
typedef struct {
    uint32_t guest_id;
    vmexit_reason_t exit_reason;
    uint32_t exit_qualification;
    uint32_t guest_linear_address;
    uint32_t guest_physical_address;
} vmcs_t;



/* ============ HYPERVISOR API ============ */
hypervisor_t* hypervisor_create(void);
void hypervisor_destroy(hypervisor_t* hv);

/* Guest VM Management */
uint32_t hypervisor_create_guest(hypervisor_t* hv, const char* guest_image);
void hypervisor_run_guest(hypervisor_t* hv, uint32_t guest_id);
void hypervisor_pause_guest(hypervisor_t* hv, uint32_t guest_id);
void hypervisor_stop_guest(hypervisor_t* hv, uint32_t guest_id);

/* VM Entry/Exit */
void vmentry(hypervisor_t* hv, guest_vm_t* guest);
void vmexit(hypervisor_t* hv, guest_vm_t* guest, vmexit_reason_t reason);
int handle_vmexit(hypervisor_t* hv, guest_vm_t* guest);

/* Memory Translation (Two-level) */
uint32_t guest_translate_address(guest_vm_t* guest, uint32_t guest_virt_addr);
uint32_t host_translate_address(hypervisor_t* hv, uint32_t guest_phys_addr);

/* Hypercall Handling */
void handle_hypercall(hypervisor_t* hv, guest_vm_t* guest, hypercall_number_t call);

/* Guest I/O */
void guest_print(hypervisor_t* hv, const char* msg);

/* Debugging */
void hypervisor_dump_state(hypervisor_t* hv);
void guest_dump_state(guest_vm_t* guest);

#endif /* ISA_H */
