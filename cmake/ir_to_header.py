#!/usr/bin/env python3

import sys
import hashlib

def main():
    if len(sys.argv) != 4:
        print("Usage: ir_to_header.py <input.ll> <output.h> <var_name>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    var_name = sys.argv[3]
    
    with open(input_file, 'r') as f:
        ir_content = f.read()
    
    # Generate unique delimiter
    content_hash = hashlib.md5(ir_content.encode()).hexdigest()[:8]
    delimiter = f"RUFUS_{content_hash}"
    
    with open(output_file, 'w') as f:
        f.write(f'// Auto-generated from {input_file}\n')
        f.write(f'#pragma once\n\n')
        f.write(f'namespace rufus {{\n')
        f.write(f'namespace embedded {{\n\n')
        f.write(f'inline const char* {var_name} = R"{delimiter}(\n')
        f.write(ir_content)
        f.write(f'\n){delimiter}";\n\n')
        f.write(f'}} // namespace embedded\n')
        f.write(f'}} // namespace rufus\n')

if __name__ == '__main__':
    main()
