.SUFFIXES:

ARCH ?= x86_64
SUB_ARCH ?= x86_32

.PHONY: clean
clean:
	$(MAKE) -C hdt clean
	$(MAKE) -C ram_test clean
	$(MAKE) -C uefi_setup clean
	$(MAKE) -C uefi_shell clean
	$(MAKE) -C rootfs clean

.PHONY: distclean
distclean:
	$(MAKE) -C hdt distclean
	$(MAKE) -C ram_test distclean
	$(MAKE) -C uefi_setup distclean
	$(MAKE) -C uefi_shell distclean
	$(MAKE) -C rootfs distclean