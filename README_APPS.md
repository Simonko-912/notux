# Notux OS - App Execution System

## Overview

This document explains how to add and execute applications in Notux OS. The system supports running user-space ELF binaries through the standard process execution mechanism.

## How It Works

1. **Process Creation**: User processes are created via `proc_create_user()` which sets up virtual memory, stacks, and initial state
2. **ELF Loading**: The `proc_load_elf()` function loads an ELF binary from the filesystem into the process's address space
3. **Execution**: The system uses `sys_exec` syscall to execute binaries through the shell or directly

## Adding New Apps

To add a new application:

1. Create your C source file in `userspace/yourapp/yourapp.c`
2. Include the proper headers:
   ```c
   #include <notux/libc.h>
   ```
3. Compile with:
   ```bash
   gcc -I../libnotux/include -L../libnotux -lnotux -nostdlib -static yourapp.c -o yourapp
   ```
4. Place the binary in `build/bin/` for testing

## Example Applications

Two example applications are included:
- `hello` - Simple hello world demonstrating basic functionality
- `testapp` - More comprehensive test showing file I/O and system calls

## Running Apps

After building, you can run apps from the shell:
```bash
# From nsh shell
./build/bin/hello
./build/bin/testapp arg1 arg2
```

## API Integration

The system integrates with the existing syscall interface:
- `SYS_EXEC` (syscall 59) - Execute a program
- `proc_exec_path()` - Internal function that handles execution
- `proc_load_elf()` - Loads ELF binary into process address space

## Future Improvements

The current implementation provides the framework. Full implementation would include:
1. Complete ELF segment loading and mapping
2. Dynamic linking support 
3. Proper error handling for malformed binaries
4. Memory protection and security features