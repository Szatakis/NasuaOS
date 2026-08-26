# Contributing to NasuaOS

Thank you for your interest in contributing to NasuaOS! This guide covers everything you need to get started.

## Quick Start

1. **Fork** the repository
2. **Clone** your fork:
   ```bash
   git clone https://github.com/<your-username>/NasuaOS.git
   ```
3. **Create a branch**:
   ```bash
   git checkout -b my-feature
   ```
4. **Make your changes**
5. **Test** (`make all`, `make run`)
6. **Commit** with a clear message
7. **Push** and open a **Pull Request**

## Code Style

### Language

- **C++20** for kernel and NAPP applications (`-std=gnu++20`)
- **Python 3** for build/config scripts
- **Makefile** syntax for build files

### Formatting

- Use **4-space indentation** (no tabs in most files)
- Brace style: **K&R** (opening brace on same line)
- Pointer/reference asterisks: `int* ptr` (asterisk with type, not name)
- Header guards use `#pragma once` (not `#ifndef`)

### Naming

| Convention | Example                             |
|------------|-------------------------------------|
| Functions  | `snake_case`                        |
| Variables  | `snake_case`                        |
| Constants  | `UPPER_CASE`                        |
| Classes    | `PascalCase`                        |
| Files      | `snake_case.cpp` / `snake_case.hpp` |

```cpp
// Good
void pmm_init(uint64_t mem_map_addr);
bool is_window_visible = false;
uint64_t KERNEL_HEAP_SIZE = 16 * 1024 * 1024;

// Avoid
int functionName();    // camelCase
int i;                 // too short
```

### Comments

Comments should explain *why*, not *what*. Use `//` for inline comments and `/* */` for block comments at file tops.

```cpp
// The red zone is disabled because the kernel uses the stack for interrupt handling
// and the red zone could be clobbered by interrupt handlers.
__attribute__((target("no-red-zone")))
```

## Pull Request Process

1. Ensure `make all` succeeds
2. Ensure `make all-hdd` succeeds
3. Write or update documentation in `documentation/`
4. Follow the commit message format below
5. Reference any relevant issues in your PR description

### Commit Message Format

```
area: brief description

More detailed explanation if needed.

Reviewed-by: <reviewer>  # if applicable
```

Examples:
```
kernel: fix null pointer dereference in page_fault_handler

The page fault handler was not checking for a null fault address
before dereferencing it, causing a triple fault on certain memory
access patterns.

filesystem/clawfs: add tombstone support for deleted files
```

## What to Contribute

### Bugs

- Fix crashes, panics, or incorrect behavior
- Add error handling for edge cases
- Ensure memory is properly freed

### Features

- New shell commands
- New NAPP applications
- New driver support
- GUI improvements
- Filesystem enhancements

### Documentation

- Update or add pages in `documentation/`
- Fix typos or unclear descriptions
- Add examples

### Testing

- Add QEMU test scripts
- Improve CI coverage
- Write test applications

## Development Workflow

### Building

```bash
make all           # Build everything
make run           # Test in QEMU
make clean         # Remove build artifacts
make clean-all     # Full clean
```

### Debugging

```bash
make run DEBUG_BUILD=true     # With debug symbols
# Or:
# 1. Run qemu with -s -S
# 2. Attach GDB: gdb-multiarch
```

See [Debug Instructions](../debug_instructions/debug_instructions.md) for detailed debugging procedures.

## Architecture Support

| Architecture  | Status         | Maintainer |
|---------------|----------------|------------|
| x86_64        | **Primary**    | Active     |
| i686 (32-bit) | Experimental   | —          |
| ARM64         | Boot stub only | —          |
| RISC-V 64     | Boot stub only | —          |
| LoongArch64   | Boot stub only | —          |

When adding features, they should be added to x86_64 first. Ports to other architectures are welcome as follow-up PRs.

## Reporting Issues

Use the GitHub issue template for bug reports. Include:

1. **NasuaOS version** and commit hash
2. **Hardware** or emulator used (QEMU, VirtualBox, real hardware)
3. **Steps to reproduce**
4. **Expected behavior**
5. **Actual behavior**
6. **Serial log** output (from `-serial file:serial.log`)

## Security Policy

See [SECURITY.md](../../SECURITY.md) for security vulnerability reporting.

## License

By contributing, you agree that your contributions will be licensed under the same license as NasuaOS. See the repository for license details.
