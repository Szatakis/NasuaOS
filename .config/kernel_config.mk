# Target architecture to build for.
ARCH := x86_64
SUB_ARCH := x86_32

# Default user QEMU flags.
QEMUFLAGS ?= -m 2G
# Flags for internet in QEMU.
QEMU_NETDEV ?= -netdev user,id=net0 -device virtio-net-pci,netdev=net0

# Virtual disk size.
FS_DISK_SIZE := 4G