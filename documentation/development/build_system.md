# Development - Build System

NasuaOS uses a hierarchical GNU Make-based build system.

## Build Toolchain

| Requirement | Version       | Notes                                          |
|-------------|---------------|------------------------------------------------|
| GCC         | C++20 capable | Cross-compiled (`x86_64-elf-gcc`) recommended  |
| Binutils    | Any modern    | `objcopy`, `ld` for linking                    |
| Make        | 4.x+          | GNU Make                                       |
| Python 3    | 3.8+          | For config generation                          |
| QEMU        | 7.0+          | For testing (`make run`)                       |
| mtools      | Any           | For rootfs image creation (`mformat`, `mcopy`) |
| xorriso     | Any           | For ISO image creation                         |
| Limine      | Latest        | Bootloader                                     |

## Top-Level Build System

The top-level `GNUmakefile` orchestrates the entire build:

### Key Variables

| Variable     | Description                             |
|--------------|-----------------------------------------|
| `QEMUFLAGS`  | QEMU flags (memory, drives, etc.)       |
| `QEMU_NETDEV`| QEMU network device flags               |
| `FS_DISK_SIZE` | Virtual disk size                     |

There is no global `ARCH` variable anymore: every image contains all
architectures. Kernel sources live in `kernel_64bit/<arch>/` and
`kernel_32bit/<arch>/` with `<arch>` ∈ `x86`, `arm`, `risc-v`,
`longarch`.

### Build Targets

| Command             | Description                                      |
|---------------------|--------------------------------------------------|
| `make all`          | Build everything (all kernels + rootfs + ISO)    |
| `make run`          | Run x86_64 image in QEMU (alias: `make run-x86_64`) |
| `make run-x86_32`   | Run the same ISO; pick the 32-bit entry in Limine |
| `make run-aarch64`  | Boot AArch64 through Limine and EDK2 in QEMU     |
| `make run-aarch32`  | Boot the ARMv7 UART shell directly in QEMU       |
| `make run-riscv64`  | Boot RISC-V64 through Limine and EDK2 in QEMU    |
| `make run-riscv32`  | Boot the RISC-V32 UART shell directly in QEMU    |
| `make run-loongarch64` | Boot the loongarch64 UART shell in QEMU       |
| `make run-loongarch32` | Boot the loongarch32 UART shell in QEMU       |
| `make run-hdd`      | Build and run in QEMU (HDD mode)                 |
| `make all-hdd`      | Build ISO and HDD images                         |
| `make clean`        | Remove build artifacts (keeps downloads)         |
| `make distclean`    | Full clean (removes all generated files)         |
| `make run-bios`     | Run in QEMU with BIOS mode                       |

### Build Order

```
make all
├── kernels (kernel_64bit/, kernel_32bit/)  ← All architectures
├── rootfs.img (utilities/rootfs/)
│   ├── bin/ (NAPP applications)
│   ├── sbin/ (NAPP command flat binaries)
│   └── ...
├── rootfs.img packaging (FAT16)
├── limine.cfg (from defaults.txt)
└── iso/ (final ISO image)
```

## Kernel Build

Sources: `kernel_64bit/GNUmakefile`, `kernel_32bit/GNUmakefile`

Each top-level kernel makefile iterates over its architecture
directories (`x86`, `arm`, `risc-v`, `longarch`) and runs the
per-architecture `GNUmakefile` inside each.

- `kernel_64bit/x86/` and `kernel_32bit/x86/` build the full
  freestanding kernels (C++20) for x86_64 and x86_32.
- The other directories build minimal freestanding kernels with
  `clang`/`ld.lld`: an architecture-specific `src/start.S` entry
  point plus `src/kernel.c`, a simple UART shell.

Built kernels are installed into the ISO at
`/boot/kernel_64bit/<arch>/kernel` and
`/boot/kernel_32bit/<arch>/kernel`.

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

| Artifact                      | Location            | Description                              |
|-------------------------------|---------------------|------------------------------------------|
| `kernel_64bit/kernel.elf`     | `build/`            | Kernel ELF binary                        |
| `utilities/rootfs/rootfs.img` | `utilities/rootfs/` | FAT16 rootfs image                       |
| `iso/limine.conf`             | `iso/`              | Generated boot config                    |
| `NasuaOS-x86_64.iso`          | Root                | Final bootable ISO                       |
| `NasuaOS-x86_64.hdd`          | Root                | Bootable HDD image (with `make all-hdd`) |

## Testing

After building, test with:

```bash
make run              # QEMU with ISO
make run-hdd          # QEMU with HDD
make run-bios         # QEMU with BIOS mode
```

See [Testing](testing.md) for more details on the testing workflow.
