# Builds every system command module into a .napp image and installs it into
# the rootfs at /sbin/<cmd>. Commands are discovered automatically: any
# subdirectory that contains its own GNUmakefile, or at least one C++ source
# file (top-level or in src/), is treated as a command module.
#
# Shared build infrastructure lives in utilities/applications/ (sibling of
# this dir).

ALL_DIRS := $(wildcard */)
MODULES_WITH_MAKEFILE := $(foreach d,$(ALL_DIRS),\
	$(if $(wildcard $(d)GNUmakefile),$(d)))
MODULES_WITH_SOURCES := $(foreach d,$(ALL_DIRS),\
	$(if $(or $(wildcard $(d)*.cpp),$(wildcard $(d)src/*.cpp)),$(d)))
CMDS := $(sort $(patsubst %/,%,$(MODULES_WITH_MAKEFILE) $(MODULES_WITH_SOURCES)))

APPS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../applications)
SBIN_DIR := $(APPS_DIR)/../rootfs/dir/sbin

CMD_BINARIES := $(addprefix $(SBIN_DIR)/,$(CMDS))
OBSOLETE_CMD_BINARIES := $(filter-out $(CMD_BINARIES),$(wildcard $(SBIN_DIR)/*))

.PHONY: all clean distclean

all:
	@rm -f $(OBSOLETE_CMD_BINARIES)
	@mkdir -p $(SBIN_DIR)
	@for cmd in $(CMDS); do \
		printf '[%d/%d] Building %s\n' 1 1 "$$cmd"; \
		$(MAKE) -C $$cmd -f $(APPS_DIR)/app.mk APP=$$cmd \
			SHARED_DIR=$(APPS_DIR) || exit 1; \
		cp -v $$cmd/$$cmd.napp $(SBIN_DIR)/$$cmd || exit 1; \
	done

# Remove object files and the final executable.
clean:
	@rm -f $(OBSOLETE_CMD_BINARIES)
	@for cmd in $(CMDS); do \
		$(MAKE) -C $$cmd -f $(APPS_DIR)/app.mk APP=$$cmd \
			SHARED_DIR=$(APPS_DIR) clean || exit 1; \
		rm -f $(SBIN_DIR)/$$cmd; \
	done

# Remove everything built and generated including downloaded dependencies.
distclean:
	@rm -f $(OBSOLETE_CMD_BINARIES)
	@for cmd in $(CMDS); do \
		$(MAKE) -C $$cmd -f $(APPS_DIR)/app.mk APP=$$cmd \
			SHARED_DIR=$(APPS_DIR) distclean || exit 1; \
		rm -f $(SBIN_DIR)/$$cmd; \
	done
