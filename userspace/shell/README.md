# Notux Shell (nsh) - Enhanced Version

## Overview

This is the enhanced Notux OS shell with improved command handling, history, and tab completion capabilities.

## Features

### Built-in Commands
- `help` / `?` - Show help information
- `version` - Display shell version  
- `cd <dir>` - Change directory
- `dir [path]` - List directory contents
- `touch <file>` - Create empty file
- `mkdir <dir>` - Create directory
- `mv <src> <dst>` - Move/rename files
- `edit <file>` - Edit file with text editor
- `unzip <file>` - Extract ZIP archives
- `ping <host>` - Send ICMP echo requests
- `usr <cmd>` - User management commands
- `sudo <cmd>` - Run commands as admin
- `bash [script]` - Start bash shell
- `services` - Manage background services  
- `pkgman` - Local package manager
- `opm` - Online package manager
- `gcc <file>` - Compile C programs
- `color` - Set terminal colors
- `exit` / `logout` - Exit shell

### External Applications
The shell automatically discovers and executes applications in `#/bin/`:
- `hello` - Simple hello world demo
- `calc` - Calculator application  
- `filemgr` - File manager
- `editor` - Text editor
- `nfetch` - System information display

## Usage

### Starting the Shell
```bash
# From Notux OS shell
./build/bin/nsh
# or simply
nsh
```

### Basic Commands
```bash
# Change directory
cd /usr/alice/docs

# List files
dir
dir /etc

# Create file  
touch myfile.txt

# Create directory
mkdir newdir

# Run applications directly
hello
calc
filemgr
editor
```

## Shell Features

### Command History
- Up/down arrow keys to navigate command history
- History is saved during session

### Tab Completion  
- Tab key completes partial commands and file paths
- Press tab to see available options

### Color Support
- Colored output for better readability
- Error messages in red
- Prompt with user and directory information

## Integration with System

The enhanced shell integrates seamlessly with the Notux OS:
- Uses standard system calls for I/O operations
- Supports all existing filesystem features  
- Works with new USB input devices
- Integrates with NTFS filesystem capabilities

## Development Notes

### Adding New Commands
To add a new built-in command:
1. Add function declaration in nsh_enhanced.c
2. Implement the functionality 
3. Add command to dispatch_command() switch statement

### External Applications
Applications in `#/bin/` are automatically executable:
- Compile with: `gcc -I../libnotux/include -L../libnotux -lnotux -nostdlib -static`
- Install to `build/bin/` directory 
- System will copy to NTFS partition during build

## Future Enhancements

### Planned Features
1. **Improved Tab Completion** - Better file and command completion
2. **Command Aliases** - User-defined command shortcuts  
3. **Scripting Support** - Shell script execution
4. **Background Jobs** - Job control for long-running processes
5. **Enhanced History** - Searchable command history
6. **Prompt Customization** - Configurable prompt format

### Performance Improvements
1. **Faster Command Lookup** - Hash-based command indexing
2. **Memory Optimization** - Reduced memory footprint
3. **Asynchronous Operations** - Non-blocking I/O for better responsiveness

## Troubleshooting

### Common Issues
1. **"Command not found"** - Check that application exists in `#/bin/`
2. **Permission denied** - Verify file permissions or use `sudo`  
3. **History navigation** - Use up/down arrow keys to navigate history

### Debugging
Use the built-in `help` command to see all available commands and features.