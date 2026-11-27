-include .config/kernel_config.mk

# Nuke built-in rules.
.SUFFIXES:

BOOTLOADER_REPO=https://raw.githubusercontent.com/Szatakis/NasuaOS-Bootloader/main

LIMINE_TEMPLATE := .config/config_generator/limine.conf.template
DEFAULTS_CONFIG := .config/defaults.txt
LIMINE_GENERATOR := .config/config_generator/generate_conf.py
GENERATED_LIMINE := .config/limine.conf

# UEFI shell fallback script baked into the ISO.  EDK2 only reaches it on
# targets where the El Torito image is not bootable (e.g. RISC-V), and a
# script aborts on the first wrong-architecture EFI, so each such target
# needs its own variant.
UEFI_STARTUP_NSH ?= .config/uefi_startup.nsh

# Application modules may provide an iso/ payload directory whose contents
# mirror the iso_root layout (see utilities/applications/GNUmakefile).
APP_ISO_PAYLOADS := $(wildcard utilities/applications/*/iso)

# Kernel architectures built into every image.
KERNEL_ARCHS := x86 arm risc-v

# QEMU binaries for experimental ARM and RISC-V targets.
QEMU_AARCH64 ?= qemu-system-aarch64
QEMU_ARM ?= qemu-system-arm
QEMU_RISCV64 ?= qemu-system-riscv64
QEMU_RISCV32 ?= qemu-system-riscv32

# Default user QEMU flags.
QEMU_X86_64 ?= qemu-system-x86_64
QEMUFLAGS ?=
QEMU_NETDEV ?=
QEMU_CPU_X86_64 ?= max

# The ISO is always the x86_64 bootable image; other architecture kernels
# are shipped inside it under /boot/kernel_32bit/<arch> and
# /boot/kernel_64bit/<arch>.
override IMAGE_NAME := NasuaOS-x86_64
override FS_NAME := clawfs_disk
FS_DISK_SIZE ?= 4G

BOOT_OPTIONS := $(wildcard utilities/boot_options/*/)
BOOT_OPTIONS := $(patsubst %/,%,$(BOOT_OPTIONS))

# Toolchain for building the 'limine' executable for the host.
HOST_CC := cc
HOST_CFLAGS := -g -O2 -pipe
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS :=

.PHONY: generate-limine
generate-limine: $(GENERATED_LIMINE)

$(GENERATED_LIMINE): $(DEFAULTS_CONFIG) $(LIMINE_TEMPLATE) $(LIMINE_GENERATOR)
	rm -f $(GENERATED_LIMINE)
	python3 $(LIMINE_GENERATOR)

.PHONY: all
all: $(IMAGE_NAME).iso

.PHONY: test-host
test-host:
	@bash ./tests/run_host_tests.sh

.PHONY: all-hdd
all-hdd: $(IMAGE_NAME).hdd

.PHONY: run
run: run-x86_64

.PHONY: run-hdd
run-hdd: run-hdd-x86_64

# ---------------------------------------------------------------------------
# x86 kernels (full NasuaOS, booted through Limine from the ISO)
# ---------------------------------------------------------------------------

kernel_64bit/x86/.deps-obtained:
	./kernel_64bit/x86/get-deps

kernel_32bit/x86/.deps-obtained:
	./kernel_32bit/x86/get-deps

.PHONY: kernels
kernels: kernel_64bit/x86/.deps-obtained kernel_32bit/x86/.deps-obtained
	$(MAKE) -C kernel_64bit
	$(MAKE) -C kernel_32bit

.PHONY: kernel_64bit kernel_32bit
kernel_64bit: kernel_64bit/x86/.deps-obtained
	$(MAKE) -C kernel_64bit

kernel_32bit: kernel_32bit/x86/.deps-obtained
	$(MAKE) -C kernel_32bit

# ---------------------------------------------------------------------------
# QEMU run targets
# ---------------------------------------------------------------------------

# x86_64 and x86_32 kernels are booted through Limine from the ISO;
.PHONY: run-x86_64 run-x86_32
run-x86_64 run-x86_32: edk2-bins $(IMAGE_NAME).iso $(FS_NAME).img
	$(QEMU_X86_64) \
		-cpu $(QEMU_CPU_X86_64) \
		-M pc,i8042=on,pcspk-audiodev=snd0 \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-x86_64.fd,readonly=on \
		-drive if=pflash,unit=1,format=raw,file=edk2-bins/vars-x86_64.fd \
		-cdrom $(IMAGE_NAME).iso \
		-drive id=$(FS_NAME),file=$(FS_NAME).img,format=raw,if=none \
		-device ide-hd,drive=$(FS_NAME),bus=ide.0,unit=0 \
		-display sdl,gl=on \
		-audiodev sdl,id=snd0 \
		-machine pcspk-audiodev=snd0 \
		-serial stdio \
		-device piix3-usb-uhci \
		-device ich9-usb-ehci1 \
		-device qemu-xhci \
		$(QEMU_NETDEV) \
		$(QEMUFLAGS)

.PHONY: run-hdd-x86_64 run-hdd-x86_32
run-hdd-x86_64 run-hdd-x86_32: edk2-bins $(IMAGE_NAME).hdd $(FS_NAME).img
	$(QEMU_X86_64) \
		-cpu $(QEMU_CPU_X86_64) \
		-M pc,i8042=on \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-x86_64.fd,readonly=on \
		-drive if=pflash,unit=1,format=raw,file=edk2-bins/vars-x86_64.fd \
		-hda $(IMAGE_NAME).hdd \
		-drive id=$(FS_NAME),file=$(FS_NAME).img,format=raw,if=none \
		-device ide-hd,drive=$(FS_NAME),bus=ide.0,unit=1 \
		-display sdl,gl=on \
		-audiodev sdl,id=snd0 \
		-machine pcspk-audiodev=snd0 \
		-serial stdio \
		-device piix3-usb-uhci \
		-device ich9-usb-ehci1 \
		-device qemu-xhci \
		$(QEMU_NETDEV) \
		$(QEMUFLAGS)

# ARM and RISC-V boot the ISO through Limine. 64-bit targets use EDK2
# firmware shipped in edk2-bins; 32-bit QEMU still loads the kernel ELF
# directly because the current firmware set has no 32-bit ARM/RISC-V UEFI.

# ARM
.PHONY: run-aarch64
run-aarch64: edk2-bins $(IMAGE_NAME).iso
	$(QEMU_AARCH64) \
		-M virt \
		-cpu cortex-a72 \
		-m 256 \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-aarch64.fd,readonly=on \
		-drive if=pflash,unit=1,format=raw,file=edk2-bins/vars-aarch64.fd \
		-cdrom $(IMAGE_NAME).iso \
		-boot order=d \
		-device ramfb \
		-display gtk,show-tabs=on \
		-serial vc:80Cx24C \
		$(QEMUFLAGS)

.PHONY: run-aarch32
run-aarch32: kernel_32bit/arm/bin/kernel
	$(QEMU_ARM) \
		-M virt \
		-cpu cortex-a15 \
		-m 256 \
		-kernel $< \
		-device ramfb \
		-display gtk,show-tabs=on \
		-serial vc:80Cx24C \
		$(QEMUFLAGS)

# RISC-V
.PHONY: run-riscv64
run-riscv64: edk2-bins
	$(MAKE) $(IMAGE_NAME).iso UEFI_STARTUP_NSH=.config/uefi_startup_riscv64.nsh
	$(QEMU_RISCV64) \
		-M virt,acpi=off \
		-cpu rv64 \
		-m 256 \
		-drive if=pflash,format=raw,unit=0,file=edk2-bins/code-riscv64.fd,readonly=on \
		-drive if=pflash,format=raw,unit=1,file=edk2-bins/vars-riscv64.fd \
		-drive file=$(IMAGE_NAME).iso,format=raw,if=virtio,readonly=on \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-display gtk,show-tabs=on \
		-serial vc:80Cx24C \
		$(QEMUFLAGS)

.PHONY: run-riscv32
run-riscv32: kernel_32bit/risc-v/bin/kernel
	$(QEMU_RISCV32) \
		-M virt \
		-cpu rv32 \
		-m 256 \
		-kernel $< \
		-device ramfb \
		-display gtk,show-tabs=on \
		-serial vc:80Cx24C \
		$(QEMUFLAGS)

.PHONY: run-bios
run-bios: $(IMAGE_NAME).iso $(FS_NAME).img
	$(QEMU_X86_64) \
		-cpu $(QEMU_CPU_X86_64) \
		-M pc,i8042=on \
		-cdrom $(IMAGE_NAME).iso \
		-boot d \
		-drive id=$(FS_NAME),file=$(FS_NAME).img,format=raw,if=none \
		-device ide-hd,drive=$(FS_NAME),bus=ide.0,unit=0 \
		-display sdl,gl=on \
		$(QEMUFLAGS)

.PHONY: run-hdd-bios
run-hdd-bios: $(IMAGE_NAME).hdd $(FS_NAME).img
	$(QEMU_X86_64) \
		-cpu $(QEMU_CPU_X86_64) \
		-M pc,i8042=on \
		-hda $(IMAGE_NAME).hdd \
		-drive id=$(FS_NAME),file=$(FS_NAME).img,format=raw,if=none \
		-device ide-hd,drive=$(FS_NAME),bus=ide.0,unit=1 \
		-display sdl,gl=on \
		$(QEMUFLAGS)

$(FS_NAME).img:
	@if [ ! -e "$@" ] || [ "$$(stat -c%s "$@")" -lt 4294967296 ]; then \
		truncate -s $(FS_DISK_SIZE) "$@"; \
	fi

edk2-bins:
	curl -fsSL $(BOOTLOADER_REPO)/edk2-bins.tar.gz | tar -xz

limine-binary/limine:
	rm -rf limine-binary
	curl -fsSL $(BOOTLOADER_REPO)/limine-binary.tar.gz | tar -xz
	$(MAKE) -C limine-binary \
		CC="$(HOST_CC)" \
		CFLAGS="$(HOST_CFLAGS)" \
		CPPFLAGS="$(HOST_CPPFLAGS)" \
		LDFLAGS="$(HOST_LDFLAGS)" \
		LIBS="$(HOST_LIBS)"

.PHONY: utilities

utilities:
	@for dir in $(BOOT_OPTIONS); do \
		$(MAKE) -C $$dir ARCH=x86_32; \
	done

.PHONY: fs_sys
fs_sys:
	$(MAKE) -C utilities/rootfs

$(IMAGE_NAME).iso: limine-binary/limine kernels utilities fs_sys $(GENERATED_LIMINE)
	rm -rf iso_root

	mkdir -p iso_root/EFI/
	mkdir -p iso_root/EFI/BOOT
	mkdir -p iso_root/boot
	mkdir -p iso_root/boot/utils
	mkdir -p iso_root/boot/limine
	mkdir -p iso_root/boot/kernel_64bit
	mkdir -p iso_root/boot/kernel_32bit
	mkdir -p iso_root/config
	mkdir -p iso_root/system
	mkdir -p iso_root/system/assets
	mkdir -p iso_root/system/assets/images
	mkdir -p iso_root/system/assets/fonts
	@for payload in $(APP_ISO_PAYLOADS); do \
		utilities/applications/install_payload.sh $$payload iso_root || exit 1; \
	done

	@for arch in $(KERNEL_ARCHS); do \
		mkdir -p iso_root/boot/kernel_64bit/$$arch iso_root/boot/kernel_32bit/$$arch; \
	done
	cp -v kernel_64bit/x86/bin-x86_64/kernel_64bit iso_root/boot/kernel_64bit/x86/kernel
	cp -v kernel_32bit/x86/bin-x86_32/kernel_32bit iso_root/boot/kernel_32bit/x86/kernel
	@for arch in arm risc-v; do \
		cp -v kernel_64bit/$$arch/bin/kernel iso_root/boot/kernel_64bit/$$arch/kernel; \
		cp -v kernel_32bit/$$arch/bin/kernel iso_root/boot/kernel_32bit/$$arch/kernel; \
	done

	cp -v utilities/boot_options/boot_mgr_x86/bin-x86_32/boot_mgr iso_root/boot/utils/boot_mgr_x86
	cp -v utilities/boot_options/boot_mgr_arm/bin-aarch64/boot_mgr iso_root/boot/utils/boot_mgr_arm
	cp -v utilities/boot_options/boot_mgr_riscv/bin-riscv64/boot_mgr iso_root/boot/utils/boot_mgr_riscv
	cp -v utilities/boot_options/clawfs_explorer/bin-x86_32/clawfs_explorer iso_root/boot/utils
	cp -v utilities/boot_options/hd_test/bin-x86_32/hdtest iso_root/boot/utils
	cp -v utilities/boot_options/hdt/bin-x86_32/hdt iso_root/boot/utils
	cp -v utilities/boot_options/kernel_debugger/bin-x86_32/kdebug iso_root/boot/utils
	cp -v utilities/boot_options/recovery_tool/bin-x86_32/recovery_tool iso_root/boot/utils
	cp -v utilities/boot_options/installer/bin-x86_32/installer iso_root/boot/utils

	cp -v utilities/rootfs/rootfs.img iso_root/system
	cp -v .config/limine.conf iso_root/boot/limine/
	cp -v documentation/images/background.png iso_root/system/assets/images/
	cp -v documentation/images/background_dark.png iso_root/system/assets/images/

	cp -v .config/boot_config.txt iso_root/config
	cp -v .config/boot_config_sf.txt iso_root/config
	cp -v .config/boot_config_rc.txt iso_root/config
	cp -v .config/defaults.txt iso_root/config
	cp -v limine-binary/limine-bios.sys limine-binary/limine-bios-cd.bin limine-binary/limine-uefi-cd.bin iso_root/boot/limine/
	mcopy -i iso_root/boot/limine/limine-uefi-cd.bin $(UEFI_STARTUP_NSH) ::/startup.nsh
	@for efi in limine-binary/BOOT*.EFI; do \
		cp -v $$efi iso_root/EFI/BOOT/; \
	done
	cp -v $(UEFI_STARTUP_NSH) iso_root/startup.nsh
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	./limine-binary/limine bios-install $(IMAGE_NAME).iso

$(IMAGE_NAME).hdd: limine-binary/limine kernels utilities fs_sys $(GENERATED_LIMINE)
	rm -f $(IMAGE_NAME).hdd
	ROOTFS_SIZE_BYTES=$$(stat -c%s utilities/rootfs/rootfs.img); \
	ROOTFS_SIZE_MB=$$(( (ROOTFS_SIZE_BYTES + 1048575) / 1048576 )); \
	HDD_SIZE_MB=$$((ROOTFS_SIZE_MB + 56)); \
	dd if=/dev/zero bs=1M count=0 seek=$$HDD_SIZE_MB of=$(IMAGE_NAME).hdd
	PATH=$$PATH:/usr/sbin:/sbin sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00 -m 1
	./limine-binary/limine bios-install $(IMAGE_NAME).hdd
	mformat -F -h 2048 -i $(IMAGE_NAME).hdd@@1M

	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT ::/EFI/utils ::/boot ::/boot/utils ::/boot/limine ::/boot/kernel_64bit ::/boot/kernel_32bit ::/config ::/system ::/system/assets ::/system/assets/images ::/system/assets/fonts
	@for arch in $(KERNEL_ARCHS); do \
		mmd -i $(IMAGE_NAME).hdd@@1M ::/boot/kernel_64bit/$$arch ::/boot/kernel_32bit/$$arch; \
	done
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel_64bit/x86/bin-x86_64/kernel_64bit ::/boot/kernel_64bit/x86/kernel
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel_32bit/x86/bin-x86_32/kernel_32bit ::/boot/kernel_32bit/x86/kernel
	@for arch in arm risc-v; do \
		mcopy -i $(IMAGE_NAME).hdd@@1M kernel_64bit/$$arch/bin/kernel ::/boot/kernel_64bit/$$arch/kernel; \
		mcopy -i $(IMAGE_NAME).hdd@@1M kernel_32bit/$$arch/bin/kernel ::/boot/kernel_32bit/$$arch/kernel; \
	done

	mcopy -i $(IMAGE_NAME).hdd@@1M .config/boot_config.txt ::/config
	mcopy -i $(IMAGE_NAME).hdd@@1M .config/boot_config_sf.txt ::/config
	mcopy -i $(IMAGE_NAME).hdd@@1M .config/boot_config_rc.txt ::/config
	mcopy -i $(IMAGE_NAME).hdd@@1M .config/defaults.txt ::/config

	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/boot_options/boot_mgr_x86/bin-x86_32/boot_mgr ::/boot/utils/boot_mgr_x86
	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/boot_options/clawfs_explorer/bin-x86_32/clawfs_explorer ::/boot/utils
	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/boot_options/hd_test/bin-x86_32/hdtest ::/boot/utils
	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/boot_options/hdt/bin-x86_32/hdt ::/boot/utils
	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/boot_options/kernel_debugger/bin-x86_32/kdebug ::/boot/utils
	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/boot_options/recovery_tool/bin-x86_32/recovery_tool ::/boot/utils
	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/boot_options/installer/bin-x86_32/installer ::/boot/utils

	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/rootfs/rootfs.img ::/system
	mcopy -i $(IMAGE_NAME).hdd@@1M documentation/images/background.png ::/system/assets/images
	mcopy -i $(IMAGE_NAME).hdd@@1M documentation/images/background_dark.png ::/system/assets/images
	mcopy -i $(IMAGE_NAME).hdd@@1M .config/limine.conf ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-binary/limine-bios.sys ::/boot/limine
	@for efi in limine-binary/BOOT*.EFI; do \
		mcopy -i $(IMAGE_NAME).hdd@@1M $$efi ::/EFI/BOOT; \
	done

.PHONY: clean
clean:
	$(MAKE) -C kernel_32bit clean
	$(MAKE) -C kernel_64bit clean
	$(MAKE) -C utilities clean
	rm -rf iso_root $(IMAGE_NAME).iso $(IMAGE_NAME).hdd

.PHONY: distclean
distclean:
	$(MAKE) -C kernel_32bit distclean
	$(MAKE) -C kernel_64bit distclean
	$(MAKE) -C utilities distclean
	rm -rf iso_root *.iso *.hdd limine-binary edk2-bins *.img $(GENERATED_LIMINE)
