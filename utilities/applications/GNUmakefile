APPS := $(wildcard */)
APPS := $(patsubst %/,%,$(APPS))

.PHONY: all

all:
	@for app in $(APPS); do \
		$(MAKE) -C $$app; \
		mkdir -p ../rootfs/dir/bin/$$app; \
		cp -v $$app/$$app ../rootfs/dir/bin/$$app/$$app; \
	done

.PHONY: clean
clean:
	@for app in $(APPS); do \
		$(MAKE) -C $$app clean; \
		rm -rf ../rootfs/dir/bin/$$app; \
	done

.PHONY: distclean
distclean:
	@for app in $(APPS); do \
		$(MAKE) -C $$app distclean; \
		rm -rf ../rootfs/dir/bin/$$app; \
	done