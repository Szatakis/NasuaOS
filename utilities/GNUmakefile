.SUFFIXES:

ARCH ?= x86_64
SUB_ARCH ?= x86_32

.PHONY: clean
clean:
	$(MAKE) -C hdt clean
	$(MAKE) -C hd_test clean
	$(MAKE) -C rootfs clean

.PHONY: distclean
distclean:
	$(MAKE) -C hdt distclean
	$(MAKE) -C hd_test distclean
	$(MAKE) -C rootfs distclean