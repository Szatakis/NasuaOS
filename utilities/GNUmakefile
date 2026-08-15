.SUFFIXES:

ARCH ?= x86_64
SUB_ARCH ?= x86_32

BOOT_OPTIONS := $(wildcard boot_options/*/)
BOOT_OPTIONS := $(patsubst %/,%,$(BOOT_OPTIONS))

.PHONY: clean
clean:
	@for dir in $(BOOT_OPTIONS); do \
		$(MAKE) -C $$dir clean; \
	done
	$(MAKE) -C rootfs clean

.PHONY: distclean
distclean:
	@for dir in $(BOOT_OPTIONS); do \
		$(MAKE) -C $$dir distclean; \
	done
	$(MAKE) -C rootfs distclean