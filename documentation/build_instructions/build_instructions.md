# Building NasuaOS

This guide explains how to build NasuaOS from source.

## Requirements

Install the following tools before building:

- Git
- Curl
- GCC
- G++
- CMake
- Xorriso
- Clang
- LLD
- NASM
- Mtools
- Python 3
- QEMU *(optional, recommended for testing (best 10.2.50))*

## Clone the Repository

```bash
git clone https://github.com/Szatakis/NasuaOS.git
cd NasuaOS
```

## Build

Compile the operating system:

```bash
make all
```

## Build and Run

If QEMU is installed, you can build and launch NasuaOS with:

```bash
make run
```

# Platform Setup

## Ubuntu / Debian

Install the required packages:

```bash
sudo apt update

sudo apt install git curl gcc g++ cmake xorriso clang lld nasm mtools python3 qemu-system-x86
```

Clone the repository:

```bash
git clone https://github.com/Szatakis/NasuaOS.git
cd NasuaOS
```

Build the project:

```bash
make all
```

Run it:

```bash
make run
```

## Windows (WSL / WSL2)

### 1. Install WSL

Install WSL with either:

- Ubuntu
- Debian

### 2. Install Dependencies

### 3. Instal QEMU on windows (optional)

Inside your Linux distribution, follow the **Ubuntu / Debian** instructions above.

---

# Build Targets

| Command | Description |
|---------|-------------|
| `make all` | Build NasuaOS |
| `make run` | Build and run in QEMU |
| `make clean` | Remove build artifacts |

# Troubleshooting

### `make: command not found`

Install Make:

```bash
sudo apt install make
```

### `nasm: command not found`

```bash
sudo apt install nasm
```

### `clang: command not found`

```bash
sudo apt install clang lld
```

### `xorriso: command not found`

```bash
sudo apt install xorriso
```