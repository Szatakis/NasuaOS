# Development - Build System

NasuaOS uses a hierarchical GNU Make-based build system.

## Build Toolchain

| Requirement | Version | Notes |
|-------------|---------|-------|
| GCC | C++20 capable | Cross-compiled (`x86_64-elf-gcc`) recommended |
| Binutils | Any modern | `objcopy`, `ld` for linking |
| Make | 4.x+ | GNU Make |
| Python 3 | 3.8+ | For config generation |
| QEMU | 7.0+ | For testing (`make run`) |
| mtools | Any | For rootfs image creation (`mformat`, `mcopy`) |
| xorriso | Any | For ISO image creation |
| Limine | Latest | Bootloader (included in repo or submodule) |

## Top-Level Build System

The top-level `GNUmakefile` orchestrates the entire build:

### Key Variables

| Variable | Description |
|----------|-------------|
| `ARCH` | Target architecture (default: `x86_64`) |
| `SUB_ARCH` | Sub-architecture |
| `KERNEL_DIR` | Kernel source directory |
| `UTIL_DIR` | Utilities directory |
| `ISO_DIR` | ISO output directory |
| `QEMU` | QEMU executable path |
| `QEMUFLAGS` | QEMU flags (memory, drives, etc.) |

### Build Targets

| Command | Description |
|---------|-------------|
| `make all` | Build everything (kernel + rootfs + ISO) |
| `make run` | Build and run in QEMU (ISO mode) |
| `make run-hdd` | Build and run in QEMU (HDD mode) |
| `make all-hdd` | Build ISO and HDD images |
| `make clean` | Remove build artifacts (keeps downloads) |
| `make clean-all` | Full clean (removes all generated files) |
| `make all-bios` | Build for BIOS boot mode |
| `make run-bios` | Run in QEMU with BIOS mode |

### Build Order

```
make all
├── kernel (kernel_64bit/)        ← Compiled first
├── rootfs.img (utilities/rootfs/)
│   ├── bin/ (NAPP applications)
│   ├── sbin/ (NAPP command flat binaries)
│   └── ...
├── rootfs.img packaging (FAT16)
├── limine.cfg (from defaults.txt)
└── iso/ (final ISO image)
```

## Kernel Build

Source: `kernel_64bit/GNUmakefile`

The kernel is compiled as a freestanding C++20 binary:

```makefile
CXXFLAGS = -std=gnu++20 -ffreestanding -nostdlib -fno-exceptions -fno-rtti
ASFLAGS  = --64
LDFLAGS  = -T linker.ld -nostdlib
```

Output: An ELF64 binary loaded by the Limine bootloader.

### Kernel Debug Builds

Debug builds are enabled with:

```bash
make all DEBUG_BUILD=true
```

Or platform-specific:

```bash
make all DEBUG_WSL=true    # WSL-specific debug flags
make all DEBUG_LINUX=true  # Linux-specific debug flags
```

## NAPP Application Build

Source: `utilities/applications/app.mk`

Each NAPP application is built using the shared `app.mk` makefile:

```makefile
APP_NAME = calculator
include ../../../applications/app.mk
```

### NAPP Build Flags

```makefile
CXXFLAGS = -std=gnu++20 -ffreestanding -nostdlib -fPIC \
           -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
           -fno-stack-protector -fno-stack-check -Wall -Wextra -O2
LDFLAGS  = -T ../napp.lds -nostdlib
```

Output: A flat binary (`*.napp` or raw for `/sbin`) via:

```bash
objcopy -O binary app.elf app.napp
```

## Rootfs Build

Source: `utilities/rootfs/GNUmakefile`

Builds the FAT16 rootfs image:

```bash
mformat -F -f 8192 -v "NASUA_ROOTFS" :: -i rootfs.img
mcopy -i rootfs.img -s bin/ ::
mcopy -i rootfs.img -s sbin/ ::
mcopy -i rootfs.img -s home/ ::
```

Output: `utilities/rootfs/rootfs.img` (8MB FAT16 filesystem)

## ISO Build

The ISO is assembled by the top-level Makefile:

1. Copies the kernel ELF to `iso/`
2. Copies `rootfs.img` to `iso/`
3. Copies Limine bootloader files to `iso/`
4. Copies the generated `limine.conf` to `iso/`
5. Builds the ISO with `xorriso`:

```bash
xorriso -as mkisofs -b limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 \
    -el-torito-alt-boot \
    -e limine-uefi-cd.bin \
    -no-emul-boot \
    -isohybrid-gpt-basdat \
    -o NasuaOS-x86_64.iso iso/
```

## HDD Image Build

```bash
make all-hdd
```

Creates `NasuaOS-x86_64.hdd` — an HDD image with:
- MBR boot sector
- GPT partition table
- Partitions for kernel + rootfs
- Bootable via QEMU `-drive file=...,format=raw`

## Configuration Generation

The build system generates `limine.conf` from `defaults.txt`:

```bash
python3 .config/config_generator/generate_conf.py \
    .config/defaults.txt \
    .config/config_generator/limine.conf.template \
    > iso/limine.conf
```

## Continuous Integration

Source: `.github/workflows/build.yml`

CI performs:

1. Check out the repository
2. Install build dependencies (gcc, binutils, make, python3, qemu, mtools, xorriso)
3. Run `make all`
4. Run `make all-hdd`
5. Upload `NasuaOS-x86_64.iso` and `NasuaOS-x86_64.hdd` as release assets

## Build Artifacts

| Artifact | Location | Description |
|----------|----------|-------------|
| `kernel_64bit/kernel.elf` | `build/` | Kernel ELF binary |
| `utilities/rootfs/rootfs.img` | `utilities/rootfs/` | FAT16 rootfs image |
| `iso/limine.conf` | `iso/` | Generated boot config |
| `NasuaOS-x86_64.iso` | Root | Final bootable ISO |
| `NasuaOS-x86_64.hdd` | Root | Bootable HDD image (with `make all-hdd`) |

## Testing

After building, test with:

```bash
make run              # QEMU with ISO
make run-hdd          # QEMU with HDD
make run-bios         # QEMU with BIOS mode
```

See [Testing](testing.md) for more details on the testing workflow.
