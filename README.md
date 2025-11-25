# vISA - Virtual ISA Emulator

A custom Instruction Set Architecture (ISA) emulator written in C, designed with virtualization in mind.

## Project Structure

- **`src/`** - Core VM implementation
  - `vm.c/h` - Main virtual machine (CPU execution loop)
  - `main.c` - Entry point
  
- **`include/`** - Public headers
  - `isa.h` - ISA definitions and API
  
- **`examples/`** - Example programs and tools
  - `assembler.py` - Convert assembly to binary
  - `programs/` - Example ISA programs

- **`tests/`** - Unit tests (to be added)

## Building

### Prerequisites
- CMake 3.10+
- GCC/Clang C compiler

### Build Steps

```bash
cd d:\Programming\vISA
mkdir build
cd build
cmake ..
cmake --build .
```

### On Windows PowerShell

```powershell
cd d:\Programming\vISA
mkdir build -Force
cd build
cmake ..
cmake --build .
```

## Usage

### 1. Create an ISA Program

Write assembly in `.isa` format:

```asm
; examples/programs/test.isa
mov r0, r1
add r2, r0, r1
halt
```

### 2. Assemble to Binary

```bash
python examples/assembler.py examples/programs/test.isa examples/programs/test.bin
```

### 3. Run on the VM

```bash
./vISA examples/programs/test.bin
```

## Answering Your Questions

### Will I be able to execute custom programs?

**Yes!** Here's how the pipeline works:

1. **Write programs** in ISA assembly (`.isa` files)
2. **Assemble** them to binary using the Python assembler
3. **Load & execute** on the VM by running the emulator

### For Virtualization Later

The architecture is designed for easy extension:

- Add **system calls** via a new opcode (e.g., `OP_SYSCALL`)
- Implement **memory protection** (separate kernel/user spaces)
- Add **privileged instructions** for kernel-mode operations
- Create **interrupt handling** for I/O and exceptions
- Add **VM context switching** for multiple virtual processes

## Basic ISA Instructions

| Instruction | Format | Description |
|-------------|--------|-------------|
| ADD | `add rd, rs1, rs2` | rd = rs1 + rs2 |
| SUB | `sub rd, rs1, rs2` | rd = rs1 - rs2 |
| MUL | `mul rd, rs1, rs2` | rd = rs1 * rs2 |
| DIV | `div rd, rs1, rs2` | rd = rs1 / rs2 |
| MOV | `mov rd, rs1` | rd = rs1 |
| HALT | `halt` | Stop execution |

## Architecture Details

- **32 Registers** (R0-R31)
- **64 KB Memory** (extensible)
- **32-bit Instructions** (1 opcode byte + 3 operand bytes)
- **Program Counter (PC)** for instruction sequencing
- **Stack Pointer (SP)** for future stack operations

## Next Steps

1. Implement missing instructions (LOAD, STORE, JMP, CALL, etc.)
2. Add I/O instructions for program output
3. Build a more sophisticated assembler
4. Add interrupt/exception handling
5. Implement virtual memory and paging
6. Add multi-task support for full virtualization

## Extending with Virtualization

For virtualization support, you'll want:

- **Execution contexts** to track multiple virtual processes
- **Memory translation** (virtual to physical addresses)
- **Privilege levels** (kernel vs. user mode)
- **System calls** for privileged operations
- **Timer interrupts** for preemptive multitasking
