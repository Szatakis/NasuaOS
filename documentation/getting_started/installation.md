# Installation

NasuaOS ships as prebuilt ISO and HDD images and can also be built from source.

## Prebuilt Images

Prebuilt images are available in the `iso` branch of the repository:

- **NasuaOS-x86_64.iso** — Bootable ISO for QEMU, VirtualBox, and real hardware (UEFI + BIOS)
- **NasuaOS-x86_64.hdd** — Virtual hard disk image for virtual machines

Download the latest images from the [iso branch](https://github.com/Szatakis/NasuaOS/tree/iso).

## System Requirements

### Minimum
| Component | Requirement |
|-----------|-------------|
| Architecture | x86_64 (recommended), or 32-bit x86 for console mode |
| RAM | 512 MB |
| Storage | 2 GB disk/HDD image |
| Graphics | Framebuffer-compatible GPU (1280×720) |
| Input | PS/2 keyboard, PS/2 or USB mouse |

### Recommended
| Component | Requirement |
|-----------|-------------|
| Architecture | x86_64 |
| RAM | 2 GB |
| Storage | 8 GB+ ClawFS-formatted disk |

## Build Prerequisites (Linux)

To build NasuaOS from source, install:

```bash
sudo apt install git curl gcc g++ cmake xorriso clang lld nasm mtools python3 qemu-system-x86
```

See [Build Instructions](../build_instructions.md) for the full guide and Windows (WSL) setup.

## Building from Source

```bash
git clone https://github.com/Szatakis/NasuaOS.git
cd NasuaOS
make all        # Build ISO image
# or
make all-hdd    # Build HDD image
```

See [Build System](../development/build_system.md) for detailed build target reference.
