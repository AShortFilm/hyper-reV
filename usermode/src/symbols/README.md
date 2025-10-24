# HyperDbg Symbol System - Ported to hyper-reV

## Overview

This directory contains a complete port of the HyperDbg symbol parsing system, specifically adapted for the hyper-reV usermode CLI tool. The symbol system enables powerful debugging and analysis capabilities by allowing symbol resolution for both user-mode and kernel-mode processes.

## Purpose

The symbol system provides:

1. **Process Attachment**: Attach to any running process by PID
2. **Module Discovery**: List all loaded modules in the attached process
3. **Symbol Resolution**: Resolve function/variable addresses by symbol name (e.g., `ntdll!NtCreateFile`)
4. **Address Resolution**: Resolve addresses back to their symbolic names
5. **Memory Reading**: Read process memory using virtual addresses
6. **Symbol Downloads**: Automatic PDB symbol download from Microsoft symbol servers

## Architecture

### Core Components

- **symbol_manager.cpp/h**: Core symbol system using Windows DbgHelp API
  - Process attachment/detachment
  - Module enumeration
  - Symbol resolution (address ↔ name)
  - Memory reading from attached process

- **symbol_commands.cpp/h**: CLI command integration
  - Command definitions for symbol operations
  - User-facing interface for symbol system

- **symbol_types.h**: Type definitions for symbol structures

## Available Commands

### `sap <pid>` - Symbol Attach Process
Attach to a process for symbol parsing and inspection.

**Example:**
```
> sap 1234
attached to process 1234
```

### `sdp` - Symbol Detach Process
Detach from the currently attached process.

**Example:**
```
> sdp
detached from process
```

### `slm` - Symbol List Modules
List all modules loaded in the currently attached process, including base addresses and sizes.

**Example:**
```
> slm
ntdll.dll - base: 0x7FFE0000, size: 0x1F0000 - C:\Windows\System32\ntdll.dll
kernel32.dll - base: 0x7FF80000, size: 0x100000 - C:\Windows\System32\kernel32.dll
```

### `srs <symbol>` - Symbol Resolve Symbol
Resolve a symbol name to its address. Supports module!function syntax.

**Examples:**
```
> srs ntdll!NtCreateFile
ntdll!NtCreateFile = 0x7FFE12340000

> srs kernel32!CreateFileW
kernel32!CreateFileW = 0x7FF812450000
```

### `sra <address>` - Symbol Resolve Address
Resolve an address to its symbolic name (reverse lookup).

**Example:**
```
> sra 0x7FFE12340000
0x7FFE12340000 = ntdll!NtCreateFile
```

### `srm <address> <size>` - Symbol Read Memory
Read memory from the attached process at the specified virtual address.

**Example:**
```
> srm 0x7FFE12340000 64
memory at 0x7FFE12340000:
4C 8B D1 B8 55 00 00 00 F6 04 25 08 03 FE 7F 01
75 03 0F 05 C3 CD 2E C3 0F 1F 84 00 00 00 00 00
48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 48
89 78 20 41 56 48 83 EC 30 65 48 8B 04 25 60 00
```

## Integration with hyper-reV

### Symbol Resolution in Commands
The symbol system integrates seamlessly with the existing hyper-reV command infrastructure. Once a process is attached via `sap`, you can use resolved symbol addresses in other commands.

**Example Workflow:**
```
# 1. Attach to a process
> sap 5678
attached to process 5678

# 2. Resolve a function address
> srs ntdll!NtCreateFile
ntdll!NtCreateFile = 0x7FFE12340000

# 3. Use the address in other hyper-reV commands
> rgvm 0x7FFE12340000 <cr3> 8
value: 0x48895C8B48D18B4C
```

### Symbol Path Configuration
The system uses Microsoft's public symbol server by default:
- Default path: `srv*C:\Symbols*https://msdl.microsoft.com/download/symbols`
- Override with `_NT_SYMBOL_PATH` environment variable

## Technical Details

### DbgHelp API
The symbol system leverages Windows DbgHelp.lib for:
- Symbol loading and management
- PDB file parsing
- Symbol name/address resolution
- Type information extraction

### Thread Safety
All symbol operations are protected by a mutex to ensure thread-safe access.

### Memory Management
- Automatic cleanup on detach
- Resource cleanup on shutdown
- Module information caching for performance

## Differences from HyperDbg

This port differs from the original HyperDbg implementation in several key ways:

1. **Simplified Focus**: Removed HyperDbg-specific remote debugging features
2. **Modern C++**: Uses C++20/23 features (std::optional, std::ranges, std::print)
3. **CLI11 Integration**: Commands use CLI11 for argument parsing
4. **Self-Contained**: No external HyperDbg SDK dependencies

## Use Cases for Debugger Teaching

### 1. Understanding Process Memory Layout
```
> sap <pid>
> slm
# Observe how modules are loaded in memory
```

### 2. Symbol Resolution
```
> srs kernel32!CreateProcessW
# See how exported functions are mapped to addresses
```

### 3. Reverse Engineering
```
> sra <unknown_address>
# Identify what function an address belongs to
```

### 4. Memory Inspection
```
> srm <function_address> 256
# Examine raw bytes of a function
```

### 5. API Hooking Preparation
```
# Find the address of a function to hook
> srs ntdll!NtReadFile
ntdll!NtReadFile = 0x7FFE12380000

# Use with hyper-reV's kernel hooking (akh command)
> akh 0x7FFE12380000 --asmbytes 0x90 0x90 --monitor
```

## Future Enhancements

Potential improvements for educational purposes:

1. **Kernel Symbol Support**: Extend to resolve kernel module symbols (nt!*, ntoskrnl!*)
2. **Type Information**: Display structure definitions and field offsets
3. **Call Stack Walking**: Implement stack trace functionality
4. **Export/Import Analysis**: Show module dependencies
5. **Symbol Search**: Wildcard/regex symbol search

## References

- Original HyperDbg symbol-parser: [hyperdbg符号系统/symbol-parser](../../../hyperdbg符号系统/symbol-parser)
- Microsoft DbgHelp API: https://docs.microsoft.com/en-us/windows/win32/debug/debug-help-library
- Symbol Server Protocol: https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/symbol-servers-and-symbol-stores

## Credits

- Original implementation by HyperDbg team (Sina Karvandi, Alee Amini)
- Ported and adapted for hyper-reV's educational debugging purposes
