#!/usr/bin/env python3
"""
Simple assembler to convert ISA assembly to binary
"""

import struct
import sys

# Opcodes
OPCODES = {
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
    'halt': 0xFF,
}

def parse_register(reg_str):
    """Parse register string like 'r0', 'r1', etc."""
    if reg_str.lower().startswith('r'):
        return int(reg_str[1:])
    raise ValueError(f"Invalid register: {reg_str}")

def assemble(asm_text):
    """Assemble ISA assembly code to binary"""
    binary = bytearray()
    lines = asm_text.strip().split('\n')
    
    for line in lines:
        line = line.strip()
        if not line or line.startswith(';'):
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
