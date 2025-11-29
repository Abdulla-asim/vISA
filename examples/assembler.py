#!/usr/bin/env python3
"""
Simple assembler to convert ISA assembly to binary
"""

import struct
import sys

# ISA Configuration
INSTRUCTION_SIZE = 4  # 4 bytes per instruction

# Opcodes
OPCODES = {
    # Standard instructions
    'add': 0x01,
    'sub': 0x02,
    'mul': 0x03,
    'div': 0x04,
    'mov': 0x05,
    'load': 0x06,
    'store': 0x07,
    'jmp': 0x08,
    'jeq': 0x09,
    'jne': 0x0A,
    'call': 0x0B,
    'ret': 0x0C,
    
    # Immediate instructions (8-bit immediate)
    'movi': 0x0D,   # movi rd, imm8
    'addi': 0x0E,   # addi rd, rs, imm8
    'subi': 0x0F,   # subi rd, rs, imm8
    'muli': 0x10,   # muli rd, rs, imm8
    'divi': 0x11,   # divi rd, rs, imm8
    
    # System instructions
    'syscall': 0x20,
    'hypercall': 0x21,
    
    # Virtualization instructions
    'vmenter': 0x30,
    'vmresume': 0x31,
    'vmcause': 0x32,
    'vmtrapcfg': 0x33,
    'ldpgtr': 0x34,
    'ldhptr': 0x35,
    'tlbflushv': 0x36,
    
    'halt': 0xFF,
}

def parse_register(reg_str):
    """Parse register string like 'r0', 'r1', etc. or immediate like '#0x10' or bare number"""
    reg_str = reg_str.rstrip(',').strip()
    if reg_str.lower().startswith('r'):
        return int(reg_str[1:])
    elif reg_str.startswith('#'):
        # Parse immediate value (hex or decimal)
        return int(reg_str[1:], 0)  # 0 base means auto-detect hex/decimal
    else:
        # Try parsing as plain number (for immediates without # prefix)
        try:
            return int(reg_str, 0)  # 0 base means auto-detect hex/decimal
        except ValueError:
            raise ValueError(f"Invalid register/immediate: {reg_str}")

def assemble(asm_text):
    """Assemble ISA assembly code to binary with label support"""
    lines = asm_text.strip().split('\n')
    
    # First pass: collect label positions
    labels = {}
    address = 0
    for line in lines:
        line = line.strip()
        if not line or line.startswith(';'):
            continue
        
        # Check if line is a label (ends with :)
        if line.endswith(':'):
            label_name = line[:-1]
            labels[label_name] = address
            continue
        
        # Count instruction size
        parts = line.split()
        if parts and parts[0].lower() in OPCODES:
            address += INSTRUCTION_SIZE
    
    # Second pass: assemble instructions
    binary = bytearray()
    for line in lines:
        line = line.strip()
        if not line or line.startswith(';'):
            continue
        
        # Skip labels
        if line.endswith(':'):
            continue
        
        parts = line.split()
        opcode_str = parts[0].lower()
        
        if opcode_str not in OPCODES:
            raise ValueError(f"Unknown opcode: {opcode_str}")
        
        opcode = OPCODES[opcode_str]
        
        # Parse operands
        if opcode_str == 'halt':
            binary.extend(struct.pack('BBBB', opcode, 0, 0, 0))
        elif opcode_str in ['add', 'sub', 'mul', 'div']:
            # Format: add r0, r1, r2
            rd = parse_register(parts[1].rstrip(','))
            rs1 = parse_register(parts[2].rstrip(','))
            rs2 = parse_register(parts[3])
            binary.extend(struct.pack('BBBB', opcode, rd, rs1, rs2))
        elif opcode_str == 'mov':
            # Format: mov r0, r1
            rd = parse_register(parts[1].rstrip(','))
            rs1 = parse_register(parts[2])
            binary.extend(struct.pack('BBBB', opcode, rd, rs1, 0))
        elif opcode_str == 'load':
            # Format: load r0, r1  (r0 = memory[r1])
            rd = parse_register(parts[1].rstrip(','))
            rs1 = parse_register(parts[2])
            binary.extend(struct.pack('BBBB', opcode, rd, rs1, 0))
        elif opcode_str == 'store':
            # Format: store r0, r1  (memory[r0] = r1)
            rs1 = parse_register(parts[1].rstrip(','))
            rs2 = parse_register(parts[2])
            binary.extend(struct.pack('BBBB', opcode, 0, rs1, rs2))
        elif opcode_str == 'movi':
            # Format: movi r0, imm8 (r0 = immediate value)
            rd = parse_register(parts[1].rstrip(','))
            imm = parse_register(parts[2])
            imm = imm & 0xFF  # Ensure 8-bit immediate
            binary.extend(struct.pack('BBBB', opcode, rd, 0, imm))
        elif opcode_str in ['addi', 'subi', 'muli', 'divi']:
            # Format: addi r0, r1, imm8 (r0 = r1 +/- imm8)
            rd = parse_register(parts[1].rstrip(','))
            rs1 = parse_register(parts[2].rstrip(','))
            imm = parse_register(parts[3])
            imm = imm & 0xFF  # Ensure 8-bit immediate
            binary.extend(struct.pack('BBBB', opcode, rd, rs1, imm))
        elif opcode_str in ['jmp', 'jeq', 'jne']:
            # Format: jmp label  or  jeq rd, rs1, rs2
            if opcode_str == 'jmp':
                # jmp label_name
                label_name = parts[1]
                target_addr = labels.get(label_name, 0)
                rd = target_addr // INSTRUCTION_SIZE  # Store address as register index
                binary.extend(struct.pack('BBBB', opcode, rd, 0, 0))
            else:
                # jeq rd, rs1, rs2  where rd is label or register
                try:
                    # Try as label first
                    label_name = parts[1].rstrip(',')
                    if label_name in labels:
                        target_addr = labels[label_name]
                        rd = target_addr // INSTRUCTION_SIZE
                    else:
                        rd = parse_register(label_name)
                    rs1 = parse_register(parts[2].rstrip(','))
                    rs2 = parse_register(parts[3])
                    binary.extend(struct.pack('BBBB', opcode, rd, rs1, rs2))
                except:
                    raise ValueError(f"Invalid {opcode_str} syntax: {line}")
        elif opcode_str in ['vmcause', 'vmtrapcfg', 'ldpgtr', 'ldhptr']:
            # Format: vmcause r0  (single register operand)
            rd = parse_register(parts[1])
            binary.extend(struct.pack('BBBB', opcode, rd, 0, 0))
        elif opcode_str in ['vmenter', 'vmresume']:
            # Format: vmenter r0  (VMCS pointer in register)
            rd = parse_register(parts[1]) if len(parts) > 1 else 0
            binary.extend(struct.pack('BBBB', opcode, rd, 0, 0))
        elif opcode_str == 'tlbflushv':
            # Format: tlbflushv (no operands)
            binary.extend(struct.pack('BBBB', opcode, 0, 0, 0))
        elif opcode_str in ['syscall', 'hypercall']:
            # Format: syscall [r0]  (optional register for syscall number)
            rd = parse_register(parts[1]) if len(parts) > 1 else 0
            binary.extend(struct.pack('BBBB', opcode, rd, 0, 0))
        elif opcode_str in ['call', 'ret']:
            # Format: call (no operands, uses function label or register)
            #         ret (no operands)
            if opcode_str == 'call' and len(parts) > 1:
                # call label_name or call r0
                label_name = parts[1]
                if label_name in labels:
                    target_addr = labels[label_name]
                    rd = target_addr // INSTRUCTION_SIZE
                else:
                    rd = parse_register(label_name)
                binary.extend(struct.pack('BBBB', opcode, rd, 0, 0))
            else:
                # ret (no operands)
                binary.extend(struct.pack('BBBB', opcode, 0, 0, 0))
        else:
            raise ValueError(f"Unsupported instruction: {opcode_str}")
    
    return bytes(binary)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: assembler.py <input.isa> [output.bin]")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else input_file.replace('.isa', '.bin')
    
    try:
        with open(input_file, 'r') as f:
            asm_code = f.read()
        
        binary = assemble(asm_code)
        
        with open(output_file, 'wb') as f:
            f.write(binary)
        
        print(f"Assembled {input_file} -> {output_file} ({len(binary)} bytes)")
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
