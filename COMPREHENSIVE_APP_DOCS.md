# Notux OS - Complete App Execution System Documentation

## Overview

This documentation explains how to develop, build, and execute applications in Notux OS. The system supports running user-space ELF binaries through a complete process execution framework.

## Getting Started

### Prerequisites

Before building applications, ensure you have the required toolchain:
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install gcc clang lld nasm parted mtools \
                 dosfstools ntfs-3g qemu-system-x86 ovmf
```

## Building Applications

### 1. Application Structure

Create your application in `userspace/yourapp/yourapp.c`:

```c
#include <notux/libc.h>

int main(int argc, char **argv) {
    nx_cprintf(nx_color(255, 255, 255), 0, "Hello from Notux!\n");
    
    // Your application code here
    
    return 0;
}
```

### 2. Compilation

Applications are compiled using the standard toolchain with proper flags:

```bash
# From the notux4 directory
gcc -Ilibnotux/include -Llibnotux -lnotux -nostdlib -static userspace/yourapp/yourapp.c -o build/bin/yourapp
```

Or use the provided build script:
```bash
./build_apps.sh
```

### 3. Example Applications

Two example applications are included:
- `hello` - Simple hello world demonstrating basic functionality
- `testapp` - More comprehensive test showing file I/O and system calls

## Filesystem Structure

Applications should be placed in the NTFS root filesystem at:
- `#/bin/` - User executables (e.g., `#/bin/hello`)
- `#/usr/` - User home directories
- `#/etc/` - Configuration files

## Execution System

### Process Creation

User processes are created through:
```c
Process *proc = proc_create_user("myapp", "#/bin/myapp", argv, envp, uid);
```

### ELF Loading

The system automatically loads ELF binaries using `proc_load_elf()` which:
1. Validates ELF64 header format
2. Checks architecture (x86-64)
3. Extracts entry point address
4. Prepares process for execution

### Syscall Interface

The system supports the following syscalls for application management:

| Syscall | Number | Description |
|---------|--------|-------------|
| `SYS_EXEC` | 59 | Execute a program (equivalent to execve) |
| `SYS_FORK` | 57 | Create a new process |
| `SYS_EXIT` | 60 | Terminate current process |

## Shell Integration

The nsh shell automatically executes programs from `#/bin/`:
```bash
# From nsh shell
hello                    # Executes #/bin/hello
./build/bin/myapp        # Execute from specific path
myapp arg1 arg2          # With arguments
```

## API Reference

### System Calls

```c
// Execute a program
int sys_exec(const char *path, const char **argv, const char **envp);

// Fork current process
pid_t sys_fork(void);

// Exit current process
void sys_exit(int status);
```

### Process Management

```c
// Create user process from ELF binary
Process* proc_create_user(const char *name, const char *path,
                          const char **argv, const char **envp, uint32_t uid);

// Load ELF into process address space
int proc_load_elf(Process *p, const char *path, uint64_t *entry_out);
```

## Error Handling

The system returns standard error codes:
- `-ENOENT` - File not found
- `-EINVAL` - Invalid argument  
- `-EBADF` - Bad file descriptor
- `-ENOEXEC` - Executable format error
- `-ENOMEM` - Out of memory

## Troubleshooting

### Common Issues

1. **"Command not found"** - Check that the binary exists in `#/bin/`
2. **"Permission denied"** - Verify filesystem permissions and user privileges  
3. **"Invalid ELF header"** - Binary may be corrupted or wrong architecture
4. **"Out of memory"** - Process exceeded memory limits

### Debugging Tips

Use nsh's built-in commands to diagnose:
```bash
dir                    # List directory contents
ls                     # Alternative listing
cat <file>             # View file contents  
```

## Best Practices

1. **Use proper headers**: Include `<notux/libc.h>` for system calls
2. **Handle errors gracefully**: Check return values of system calls
3. **Manage memory**: Be aware of process memory limits (256 MiB default)
4. **Follow file permissions**: Respect user and admin privileges
5. **Use appropriate syscalls**: Prefer Notux-specific APIs over POSIX compatibility

## Development Workflow

1. Create application source in `userspace/`
2. Compile with proper flags using the toolchain
3. Install binary to `build/bin/` 
4. Test execution from nsh shell
5. Iterate and improve

## Future Improvements

The current implementation provides a solid foundation. Future enhancements include:
- Full ELF segment loading and memory mapping
- Dynamic linking support  
- Enhanced security features
- Advanced debugging capabilities
- Package management integration

## Contributing

Contributions to the app execution system are welcome! Please:
1. Follow existing code patterns and conventions
2. Add comprehensive error handling
3. Include proper documentation
4. Test with various application types
5. Ensure compatibility with existing system calls

## License

Notux OS is released under the MIT license. See LICENSE for details.