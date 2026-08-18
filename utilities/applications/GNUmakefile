# Builds every application folder into a .napp image and installs it into the
# rootfs at /bin/<app>/<app>.napp. New applications are picked up automatically,
# a folder with C++ sources is enough.

APPS := $(wildcard */)
APPS := $(patsubst %/,%,$(APPS))
APPS := $(filter-out include calculator,$(APPS))

.PHONY: all

all:
	@for app in $(APPS); do \
		if [ -f $$app/GNUmakefile ]; then \
			$(MAKE) -C $$app || exit 1; \
		else \
			$(MAKE) -C $$app -f ../app.mk APP=$$app || exit 1; \
		fi; \
		cp -v $$app/$$app.napp ../rootfs/dir/bin/$$app; \
	done

.PHONY: clean
clean:
	@for app in $(APPS); do \
		if [ -f $$app/GNUmakefile ]; then \
			$(MAKE) -C $$app clean; \
		else \
			$(MAKE) -C $$app -f ../app.mk APP=$$app clean; \
		fi; \
		rm -f ../rootfs/dir/bin/$$app; \
	done

.PHONY: distclean
distclean:
	@for app in $(APPS); do \
		if [ -f $$app/GNUmakefile ]; then \
			$(MAKE) -C $$app distclean; \
		else \
			$(MAKE) -C $$app -f ../app.mk APP=$$app distclean; \
		fi; \
		rm -f ../rootfs/dir/bin/$$app; \
	done
