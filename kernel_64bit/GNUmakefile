# Nuke built-in rules.
.SUFFIXES:

# Architecture directories, each with its own kernel build.
ARCH_DIRS := x86 arm risc-v

.PHONY: all clean distclean
all:
	@for d in $(ARCH_DIRS); do \
		$(MAKE) -C $$d || exit 1; \
	done

clean:
	@for d in $(ARCH_DIRS); do \
		$(MAKE) -C $$d clean || exit 1; \
	done

distclean:
	@for d in $(ARCH_DIRS); do \
		$(MAKE) -C $$d distclean || exit 1; \
	done
