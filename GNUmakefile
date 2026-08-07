-include config.mk

# Nuke built-in rules.
.SUFFIXES:

BOOTLOADER_REPO=https://github.com/Szatakis/NasuaOS-Bootloader/raw/main

# Target architecture to build for. Default to x86_64.
ARCH ?= x86_64
SUB_ARCH ?= x86_32

# Default user QEMU flags. These are appended to the QEMU command calls.
QEMUFLAGS := -m 2G

override IMAGE_NAME := NasuaOS-$(ARCH)
override FS_NAME := clawfs_disk

# Detect WSL
DEBUG_WSL ?= true

ifeq ($(DEBUG_WSL),true)
	IS_WSL := 1
else
	IS_WSL := $(shell grep -qi microsoft /proc/version 2>/dev/null && echo 1 || echo 0)
endif

ifeq ($(IS_WSL),1)
QEMU_X86_64 = /mnt/c/Program\ Files/qemu/qemu-system-x86_64.exe
QEMU_AARCH64 = /mnt/c/Program\ Files/qemu/qemu-system-aarch64.exe
QEMU_RISCV64 = /mnt/c/Program\ Files/qemu/qemu-system-riscv64.exe
QEMU_LOONGARCH64 = /mnt/c/Program\ Files/qemu/qemu-system-loongarch64.exe

define PREPARE_WSL
	@powershell.exe -Command "New-Item -ItemType Directory -Force -Path C:\wsl_target" > /dev/null

	@if [ ! -f /mnt/c/wsl_target/$(FS_NAME).img ]; then \
		dd if=/dev/zero of=/mnt/c/wsl_target/$(FS_NAME).img bs=1M count=2048 2>/dev/null; \
		echo "-> Created virtual disk"; \
	fi

	@cp $(IMAGE_NAME).iso /mnt/c/wsl_target/$(IMAGE_NAME).iso
	@rm -rf /mnt/c/wsl_target/edk2-bins
	@cp -r edk2-bins /mnt/c/wsl_target/
endef

else

QEMU_X86_64 = qemu-system-x86_64
QEMU_AARCH64 = qemu-system-aarch64
QEMU_RISCV64 = qemu-system-riscv64
QEMU_LOONGARCH64 = qemu-system-loongarch64

define PREPARE_WSL
endef

endif

# Toolchain for building the 'limine' executable for the host.
HOST_CC := cc
HOST_CFLAGS := -g -O2 -pipe
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS :=

.PHONY: all
all: $(IMAGE_NAME).iso

.PHONY: all-hdd
all-hdd: $(IMAGE_NAME).hdd

.PHONY: run
run: run-$(ARCH)

.PHONY: run-hdd
run-hdd: run-hdd-$(ARCH)

.PHONY: run-x86_64
run-x86_64: edk2-bins $(IMAGE_NAME).iso
ifeq ($(IS_WSL),1)
	$(PREPARE_WSL)
	$(QEMU_X86_64) \
		-M pc \
		-drive if=pflash,unit=0,format=raw,file=C:\\wsl_target\\edk2-bins\\code-x86_64.fd,readonly=on \
		-drive if=pflash,unit=1,format=raw,file=C:\\wsl_target\\edk2-bins\\vars-x86_64.fd \
		-cdrom C:\\wsl_target\\$(IMAGE_NAME).iso \
		-drive id=$(FS_NAME),file=C:\\wsl_target\\$(FS_NAME).img,format=raw,if=none \
		-device ide-hd,drive=$(FS_NAME),bus=ide.0,unit=0 \
		-display sdl,gl=on \
		-device virtio-vga \
		-serial stdio \
		-audiodev sdl,id=snd0 \
		-machine pcspk-audiodev=snd0 \
		$(QEMUFLAGS)
else
	$(QEMU_X86_64) \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-x86_64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		-audiodev sdl,id=snd0 \
		-machine pcspk-audiodev=snd0 \
		$(QEMUFLAGS)
endif

.PHONY: run-hdd-x86_64
run-hdd-x86_64: edk2-bins $(IMAGE_NAME).hdd
ifeq ($(IS_WSL),1)
	@powershell.exe -Command "New-Item -ItemType Directory -Force -Path C:\wsl_target" > /dev/null

	@cp $(IMAGE_NAME).hdd /mnt/c/wsl_target/$(IMAGE_NAME).hdd
	@rm -rf /mnt/c/wsl_target/edk2-bins
	@cp -r edk2-bins /mnt/c/wsl_target/

	$(QEMU_X86_64) \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=C:\\wsl_target\\edk2-bins\\code-x86_64.fd,readonly=on \
		-hda C:\\wsl_target\\$(IMAGE_NAME).hdd \
		$(QEMUFLAGS) \
		-display sdl,gl=on \
		-serial stdio
else
	$(QEMU_X86_64) \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-x86_64.fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)
endif

.PHONY: run-aarch64
run-aarch64: edk2-bins $(IMAGE_NAME).iso
ifeq ($(IS_WSL),1)
	$(PREPARE_WSL)
	$(QEMU_AARCH64) \
		-M virt \
		-cpu cortex-a72 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=C:\\wsl_target\\edk2-bins\\code-aarch64.fd,readonly=on \
		-cdrom C:\\wsl_target\\$(IMAGE_NAME).iso \
		$(QEMUFLAGS)
else
	$(QEMU_AARCH64) \
		-M virt \
		-cpu cortex-a72 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-aarch64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)
endif

.PHONY: run-hdd-aarch64
run-hdd-aarch64: edk2-bins $(IMAGE_NAME).hdd
ifeq ($(IS_WSL),1)
	@cp $(IMAGE_NAME).hdd /mnt/c/wsl_target/$(IMAGE_NAME).hdd
	@rm -rf /mnt/c/wsl_target/edk2-bins
	@cp -r edk2-bins /mnt/c/wsl_target/

	$(QEMU_AARCH64) \
		-M virt \
		-cpu cortex-a72 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=C:\\wsl_target\\edk2-bins\\code-aarch64.fd,readonly=on \
		-hda C:\\wsl_target\\$(IMAGE_NAME).hdd \
		$(QEMUFLAGS)
else
	$(QEMU_AARCH64) \
		-M virt \
		-cpu cortex-a72 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-aarch64.fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)
endif

.PHONY: run-riscv64
run-riscv64: edk2-bins $(IMAGE_NAME).iso
ifeq ($(IS_WSL),1)
	$(PREPARE_WSL)
	$(QEMU_RISCV64) \
		-M virt \
		-cpu rv64 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=C:\\wsl_target\\edk2-bins\\code-riscv64.fd,readonly=on \
		-cdrom C:\\wsl_target\\$(IMAGE_NAME).iso \
		$(QEMUFLAGS)
else
	$(QEMU_RISCV64) \
		-M virt \
		-cpu rv64 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-riscv64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)
endif

.PHONY: run-hdd-riscv64
run-hdd-riscv64: edk2-bins $(IMAGE_NAME).hdd
ifeq ($(IS_WSL),1)
	@cp $(IMAGE_NAME).hdd /mnt/c/wsl_target/$(IMAGE_NAME).hdd
	@rm -rf /mnt/c/wsl_target/edk2-bins
	@cp -r edk2-bins /mnt/c/wsl_target/

	$(QEMU_RISCV64) \
		-M virt \
		-cpu rv64 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=C:\\wsl_target\\edk2-bins\\code-riscv64.fd,readonly=on \
		-hda C:\\wsl_target\\$(IMAGE_NAME).hdd \
		$(QEMUFLAGS)
else
	$(QEMU_RISCV64) \
		-M virt \
		-cpu rv64 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-riscv64.fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)
endif

.PHONY: run-loongarch64
run-loongarch64: edk2-bins $(IMAGE_NAME).iso
ifeq ($(IS_WSL),1)
	$(PREPARE_WSL)
	$(QEMU_LOONGARCH64) \
		-M virt \
		-cpu la464 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=C:\\wsl_target\\edk2-bins\\code-loongarch64.fd,readonly=on \
		-cdrom C:\\wsl_target\\$(IMAGE_NAME).iso \
		$(QEMUFLAGS)
else
	$(QEMU_LOONGARCH64) \
		-M virt \
		-cpu la464 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-loongarch64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)
endif

.PHONY: run-hdd-loongarch64
run-hdd-loongarch64: edk2-bins $(IMAGE_NAME).hdd
ifeq ($(IS_WSL),1)
	@cp $(IMAGE_NAME).hdd /mnt/c/wsl_target/$(IMAGE_NAME).hdd
	@rm -rf /mnt/c/wsl_target/edk2-bins
	@cp -r edk2-bins /mnt/c/wsl_target/

	$(QEMU_LOONGARCH64) \
		-M virt \
		-cpu la464 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=C:\\wsl_target\\edk2-bins\\code-loongarch64.fd,readonly=on \
		-hda C:\\wsl_target\\$(IMAGE_NAME).hdd \
		$(QEMUFLAGS)
else
	$(QEMU_LOONGARCH64) \
		-M virt \
		-cpu la464 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-tablet \
		-drive if=pflash,unit=0,format=raw,file=edk2-bins/code-loongarch64.fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)
endif


.PHONY: run-bios
run-bios: $(IMAGE_NAME).iso
ifeq ($(IS_WSL),1)
	@cp $(IMAGE_NAME).iso /mnt/c/wsl_target/$(IMAGE_NAME).iso

	$(QEMU_X86_64) \
		-M q35 \
		-cdrom C:\\wsl_target\\$(IMAGE_NAME).iso \
		-boot d \
		$(QEMUFLAGS) \
		-display sdl,gl=on \
		-serial stdio
else
	$(QEMU_X86_64) \
		-M q35 \
		-cdrom $(IMAGE_NAME).iso \
		-boot d \
		$(QEMUFLAGS)
endif

.PHONY: run-hdd-bios
run-hdd-bios: $(IMAGE_NAME).hdd
ifeq ($(IS_WSL),1)
	@cp $(IMAGE_NAME).hdd /mnt/c/wsl_target/$(IMAGE_NAME).hdd

	$(QEMU_X86_64) \
		-M q35 \
		-hda C:\\wsl_target\\$(IMAGE_NAME).hdd \
		$(QEMUFLAGS) \
		-display sdl,gl=on \
		-serial stdio
else
	$(QEMU_X86_64) \
		-M q35 \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)
endif

edk2-bins:
	curl -L $(BOOTLOADER_REPO)/edk2-bins.tar.gz | tar -xz

limine-binary/limine:
	rm -rf limine-binary
	curl -L $(BOOTLOADER_REPO)/limine-binary.tar.gz | tar -xz
	$(MAKE) -C limine-binary \
		CC="$(HOST_CC)" \
		CFLAGS="$(HOST_CFLAGS)" \
		CPPFLAGS="$(HOST_CPPFLAGS)" \
		LDFLAGS="$(HOST_LDFLAGS)" \
		LIBS="$(HOST_LIBS)"

kernel_64bit/.deps-obtained:
	./kernel_64bit/get-deps

.PHONY: kernel_64bit
kernel_64bit: kernel_64bit/.deps-obtained
	$(MAKE) -C kernel_64bit ARCH=$(ARCH)

.PHONY: kernel_32bit
kernel_32bit:
	$(MAKE) -C kernel_32bit ARCH=$(SUB_ARCH)

.PHONY: utilities
utilities:
	$(MAKE) -C utilities/hdt ARCH=$(SUB_ARCH)
	$(MAKE) -C utilities/ram_test ARCH=$(SUB_ARCH)
	$(MAKE) -C utilities/uefi_setup ARCH=$(ARCH)
	$(MAKE) -C utilities/uefi_shell ARCH=$(ARCH)

.PHONY: fs_sys
fs_sys:
	$(MAKE) -C utilities/rootfs

$(IMAGE_NAME).iso: limine-binary/limine kernel_64bit kernel_32bit utilities fs_sys
	rm -rf iso_root

	mkdir -p iso_root/EFI/
	mkdir -p iso_root/EFI/BOOT
	mkdir -p iso_root/boot
	mkdir -p iso_root/boot/utils
	mkdir -p iso_root/boot/limine
	mkdir -p iso_root/system
	mkdir -p iso_root/system/assets
	mkdir -p iso_root/system/assets/images
	mkdir -p iso_root/system/assets/fonts

	cp -v kernel_64bit/bin-$(ARCH)/kernel_64bit iso_root/boot/
	cp -v kernel_32bit/bin-$(SUB_ARCH)/kernel_32bit iso_root/boot/
	cp -v utilities/hdt/bin-$(SUB_ARCH)/hdt iso_root/boot/utils
	cp -v utilities/ram_test/bin-$(SUB_ARCH)/memtest iso_root/boot/utils
	cp -v utilities/uefi_setup/bin-$(ARCH)/fwsetup.efi iso_root/boot/utils
	cp -v utilities/uefi_shell/bin-$(ARCH)/uefi_shell.efi iso_root/boot/utils
	cp -v utilities/rootfs/rootfs.img iso_root/system
	cp -v limine.conf iso_root/boot/limine/
	cp -v documentation/images/background.png iso_root/system/assets/images/
	cp -v boot_config.txt iso_root/boot
	cp -v boot_config_sf.txt iso_root/boot
ifeq ($(ARCH),x86_64)
	cp -v limine-binary/limine-bios.sys limine-binary/limine-bios-cd.bin limine-binary/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine-binary/BOOTX64.EFI iso_root/EFI/BOOT/
	cp -v limine-binary/BOOTIA32.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	./limine-binary/limine bios-install $(IMAGE_NAME).iso
endif
ifeq ($(ARCH),aarch64)
	cp -v limine-binary/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine-binary/BOOTAA64.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
endif
ifeq ($(ARCH),riscv64)
	cp -v limine-binary/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine-binary/BOOTRISCV64.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
endif
ifeq ($(ARCH),loongarch64)
	cp -v limine-binary/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine-binary/BOOTLOONGARCH64.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
endif
	rm -rf iso_root

$(IMAGE_NAME).hdd: limine-binary/limine kernel_64bit kernel_32bit utilities fs_sys
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=96 of=$(IMAGE_NAME).hdd
ifeq ($(ARCH),x86_64)
	PATH=$$PATH:/usr/sbin:/sbin sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00 -m 1
	./limine-binary/limine bios-install $(IMAGE_NAME).hdd
else
	PATH=$$PATH:/usr/sbin:/sbin sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00
endif
	mformat -F -h 2048 -i $(IMAGE_NAME).hdd@@1M
	
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT ::/EFI/utils ::/boot ::/boot/utils ::/boot/limine ::/system ::/system/assets ::/system/assets/images ::/system/assets/fonts
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel_64bit/bin-$(ARCH)/kernel_64bit ::/boot
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel_32bit/bin-$(SUB_ARCH)/kernel_32bit ::/boot

	mcopy -i $(IMAGE_NAME).hdd@@1M boot_config.txt ::/boot
	mcopy -i $(IMAGE_NAME).hdd@@1M boot_config_sf.txt ::/boot

	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/hdt/bin-$(SUB_ARCH)/hdt ::/boot/utils
	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/ram_test/bin-$(SUB_ARCH)/memtest ::/boot/utils
	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/uefi_setup/bin-$(ARCH)/fwsetup.efi ::/boot/utils
	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/uefi_shell/bin-$(ARCH)/uefi_shell.efi ::/boot/utils

	mcopy -i $(IMAGE_NAME).hdd@@1M utilities/rootfs/rootfs.img ::/system
	mcopy -i $(IMAGE_NAME).hdd@@1M documentation/images/background.png ::/system/assets/images
	mcopy -i $(IMAGE_NAME).hdd@@1M limine.conf ::/boot/limine
ifeq ($(ARCH),x86_64)
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-binary/limine-bios.sys ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-binary/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-binary/BOOTIA32.EFI ::/EFI/BOOT
endif
ifeq ($(ARCH),aarch64)
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-binary/BOOTAA64.EFI ::/EFI/BOOT
endif
ifeq ($(ARCH),riscv64)
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-binary/BOOTRISCV64.EFI ::/EFI/BOOT
endif
ifeq ($(ARCH),loongarch64)
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-binary/BOOTLOONGARCH64.EFI ::/EFI/BOOT
endif

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

	rm -rf iso_root *.iso *.hdd limine-binary edk2-bins