# HyperDbg Symbol System Migration to hyper-reV - Summary

## Overview

Successfully ported the HyperDbg symbol parsing system to the hyper-reV project, providing powerful symbol resolution capabilities for debugger teaching and analysis purposes.

## What Was Ported

### Core Functionality
- ✅ Process attachment by PID
- ✅ Module enumeration for attached processes
- ✅ Symbol name → address resolution (`ntdll!NtCreateFile`)
- ✅ Address → symbol name resolution (reverse lookup)
- ✅ Process memory reading via virtual addresses
- ✅ Automatic Microsoft symbol server integration
- ✅ Thread-safe symbol operations

### Technology Stack
- **API**: Windows DbgHelp library for PDB parsing and symbol resolution
- **Language**: Modern C++20/23 (std::optional, std::print, std::ranges)
- **Integration**: CLI11 command framework
- **Dependencies**: DbgHelp.lib (Windows SDK)

## Migration Strategy

### Simplified Architecture
Rather than porting the entire HyperDbg pdbex/symbol-parser stack, I implemented a streamlined symbol system that:

1. **Uses Native Windows APIs**: Direct DbgHelp API usage instead of custom PDB parsing
2. **Modern C++ Design**: Leverages C++20/23 features and STL
3. **Focused Scope**: User-mode process symbol resolution (not remote debugging)
4. **Zero External Dependencies**: No HyperDbg SDK required

### Why This Approach?

The original HyperDbg symbol system includes:
- `pdbex`: Custom PDB parsing library (DLL-based)
- `symbol-parser`: Symbol resolution layer with remote debugging features
- Complex HyperDbg SDK integration

For hyper-reV's educational debugging purposes, this full stack was unnecessary. The simplified approach provides:
- ✅ Easier maintenance
- ✅ Better performance (native API)
- ✅ Cleaner integration with existing code
- ✅ All required teaching/debugging capabilities

## New File Structure

```
usermode/src/symbols/
├── symbol_manager.h              # Core symbol system interface
├── symbol_manager.cpp            # DbgHelp API implementation
├── symbol_commands.h             # CLI command definitions
├── symbol_commands.cpp           # Command handlers
└── README.md                     # Comprehensive documentation
```

## Available Commands

| Command | Purpose | Example |
|---------|---------|---------|
| `sap <pid>` | Attach to process | `sap 1234` |
| `sdp` | Detach from process | `sdp` |
| `slm` | List modules | `slm` |
| `srs <symbol>` | Resolve symbol name | `srs ntdll!NtCreateFile` |
| `sra <address>` | Resolve address | `sra 0x7FFE12340000` |
| `srm <addr> <size>` | Read memory | `srm 0x7FFE12340000 64` |

## Integration Points

### 1. Main Application (main.cpp)
```cpp
#include "symbols/symbol_manager.h"

int main() {
    symbols::initialize();  // Initialize symbol system
    // ... existing code ...
    symbols::shutdown();    // Cleanup on exit
}
```

### 2. Command System (commands.cpp)
```cpp
#include "symbols/symbol_commands.h"

void commands::process(const std::string command) {
    // ... existing command setup ...
    symbol_commands::init_commands(app, aliases_transformer);
    // ... command parsing ...
}
```

### 3. Build Configuration (usermode.vcxproj)
- Added `DbgHelp.lib` to linker dependencies
- Added symbol source files to compilation
- Maintained static runtime linkage (`/MT`, `/MTd`)

## Use Cases for Teaching

### 1. Understanding Symbol Resolution
Students can see how debuggers map function names to addresses:
```
> sap 5678
> srs ntdll!NtCreateFile
ntdll!NtCreateFile = 0x7FFE12340000
```

### 2. Reverse Engineering Workflows
```
> sap 1234
> slm                          # See loaded modules
> srs kernel32!CreateFileW     # Find function address
> srm 0x7FF812450000 128       # Examine function bytes
```

### 3. API Hooking Integration
Combine with hyper-reV's existing hooking:
```
> srs ntdll!NtReadFile         # Get target address
> akh 0x7FFE12380000 --monitor # Hook using resolved address
```

### 4. Process Memory Analysis
```
> sap 9876
> slm                          # List all loaded DLLs
> srs user32!MessageBoxW       # Find API address
> srm 0x... 32                 # Read function prologue
```

## Technical Highlights

### Thread Safety
```cpp
namespace symbols {
    std::mutex g_guard;  // Protects all symbol operations
    
    std::optional<std::uint64_t> resolve_symbol_address(...) {
        std::scoped_lock lock(g_guard);
        // Safe concurrent access
    }
}
```

### Error Handling
```cpp
// All operations return std::optional for safe error handling
auto address = symbols::resolve_symbol_address("ntdll!NtCreateFile");
if (!address.has_value()) {
    std::println("Symbol not found");
}
```

### Resource Management
```cpp
namespace symbols {
    bool attach(std::uint32_t process_id) {
        // Automatic cleanup of previous session
        unload_modules_locked();
        // Open new process handle
        // Initialize DbgHelp
        // Load module symbols
    }
}
```

## Symbol Server Configuration

### Default Behavior
- Uses Microsoft public symbol server
- Downloads PDBs to `C:\Symbols\`
- Path: `srv*C:\Symbols*https://msdl.microsoft.com/download/symbols`

### Custom Configuration
Set `_NT_SYMBOL_PATH` environment variable:
```batch
set _NT_SYMBOL_PATH=srv*C:\MySymbols*https://msdl.microsoft.com/download/symbols;C:\LocalSymbols
```

## Comparison with Original HyperDbg

| Feature | HyperDbg | hyper-reV Port |
|---------|----------|----------------|
| PDB Parsing | Custom (pdbex) | DbgHelp API |
| Remote Debugging | Yes | No (local only) |
| Kernel Symbols | Yes | Future enhancement |
| User-Mode Symbols | Yes | Yes ✅ |
| Type Information | Yes | Future enhancement |
| Module Enumeration | Yes | Yes ✅ |
| Symbol Resolution | Yes | Yes ✅ |
| Memory Reading | Yes (remote) | Yes (local) ✅ |
| Dependencies | pdbex.dll, SDK | DbgHelp.lib only |

## Future Enhancements

### Short Term
1. **Kernel Module Symbols**: Extend to resolve `nt!*` and `ntoskrnl!*` symbols
2. **Symbol Caching**: Cache frequently used symbols for performance

### Medium Term
3. **Type Information**: Display structure definitions and field offsets
4. **Call Stack Walking**: Implement stack trace functionality
5. **Symbol Search**: Wildcard/regex search for symbol names

### Long Term
6. **Source Line Information**: Map addresses to source file lines
7. **Inline Function Support**: Handle inlined function symbols
8. **Multi-Process**: Manage symbols for multiple processes simultaneously

## Educational Value

This symbol system teaches students:

1. **Debugger Internals**: How debuggers resolve symbols using PDB files
2. **Process Memory**: Understanding module loading and memory layout
3. **API Resolution**: How function names map to executable addresses
4. **Symbol Formats**: Microsoft PDB format and symbol servers
5. **Debugging Workflows**: Professional debugging techniques

## Build Requirements

- **Toolset**: Visual Studio 2022 (v143)
- **C++ Standard**: C++latest (C++20/23)
- **Runtime**: Static multithreaded (`/MT` release, `/MTd` debug)
- **SDK**: Windows 10 SDK (for DbgHelp.lib)
- **Dependencies**: CLI11 (already in project)

## Testing Recommendations

### Basic Functionality
```
1. Launch usermode.exe
2. Find a running process PID (e.g., notepad.exe)
3. Test: sap <pid>
4. Test: slm
5. Test: srs ntdll!RtlInitUnicodeString
6. Test: sdp
```

### Advanced Testing
```
1. Attach to multiple processes sequentially
2. Resolve symbols from different modules
3. Read memory at resolved addresses
4. Test error cases (invalid PID, bad symbol names)
```

## Conclusion

The HyperDbg symbol system has been successfully adapted for hyper-reV, providing a modern, efficient, and educational symbol resolution framework. The implementation:

- ✅ Maintains core functionality for teaching purposes
- ✅ Simplifies architecture using native Windows APIs
- ✅ Integrates seamlessly with existing hyper-reV infrastructure
- ✅ Provides foundation for future enhancements
- ✅ Demonstrates professional debugging techniques

The ported system serves as both a practical tool and an educational resource for understanding debugger symbol resolution.
