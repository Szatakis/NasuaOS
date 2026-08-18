# Builds every system command folder into a .napp image and installs it into
# the rootfs at /sbin/<cmd>. New commands are picked up automatically — a
# folder with a C++ source is enough.

CMDS := $(wildcard */)
CMDS := $(patsubst %/,%,$(CMDS))

# Shared build infrastructure lives in utilities/applications/ (sibling of this dir)
APPS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../applications)

.PHONY: all
all:
	@for cmd in $(CMDS); do \
		$(MAKE) -C $$cmd -f $(APPS_DIR)/app.mk APP=$$cmd \
			SHARED_DIR=$(APPS_DIR) || exit 1; \
		mkdir -p $(APPS_DIR)/../rootfs/dir/sbin; \
		cp -v $$cmd/$$cmd.napp $(APPS_DIR)/../rootfs/dir/sbin/$$cmd; \
	done

.PHONY: clean
clean:
	@for cmd in $(CMDS); do \
		$(MAKE) -C $$cmd -f $(APPS_DIR)/app.mk APP=$$cmd \
			SHARED_DIR=$(APPS_DIR) clean; \
		rm -f $(APPS_DIR)/../rootfs/dir/sbin/$$cmd; \
	done

.PHONY: distclean
distclean:
	@for cmd in $(CMDS); do \
		$(MAKE) -C $$cmd -f $(APPS_DIR)/app.mk APP=$$cmd \
			SHARED_DIR=$(APPS_DIR) distclean; \
		rm -f $(APPS_DIR)/../rootfs/dir/sbin/$$cmd; \
	done
