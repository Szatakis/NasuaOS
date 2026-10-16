# Builds every application folder into a .napp image and installs it into the
# rootfs at /bin/<app>/<app>.napp. New applications are picked up automatically,
# a folder with C++ sources is enough.

APPS := $(wildcard */)
APPS := $(patsubst %/,%,$(APPS))
APPS := $(filter-out include,$(APPS))
APP_BIN_DIR := ../rootfs/dir/bin
APP_BINARIES := $(addprefix $(APP_BIN_DIR)/,$(APPS))
OBSOLETE_APP_BINARIES := $(filter-out $(APP_BINARIES),$(wildcard $(APP_BIN_DIR)/*))

.PHONY: all

all:
	rm -f $(OBSOLETE_APP_BINARIES)
	@for app in $(APPS); do \
		if [ -f $$app/GNUmakefile ]; then \
			$(MAKE) -C $$app || exit 1; \
		else \
			$(MAKE) -C $$app -f ../app.mk APP=$$app || exit 1; \
		fi; \
		cp -v $$app/$$app.napp $(APP_BIN_DIR)/$$app; \
	done

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -f $(OBSOLETE_APP_BINARIES)
	@for app in $(APPS); do \
		if [ -f $$app/GNUmakefile ]; then \
			$(MAKE) -C $$app clean; \
		else \
			$(MAKE) -C $$app -f ../app.mk APP=$$app clean; \
		fi; \
		rm -f $(APP_BIN_DIR)/$$app; \
	done

# Remove everything built and generated including downloaded dependencies.
.PHONY: distclean
distclean:
	rm -f $(OBSOLETE_APP_BINARIES)
	@for app in $(APPS); do \
		if [ -f $$app/GNUmakefile ]; then \
			$(MAKE) -C $$app distclean; \
		else \
			$(MAKE) -C $$app -f ../app.mk APP=$$app distclean; \
		fi; \
		rm -f $(APP_BIN_DIR)/$$app; \
	done
