# Builds every application module into a .napp image and installs it into the
# rootfs at /bin/<app>/<app>.napp.
#
# Application modules are discovered automatically: any subdirectory that
# contains its own GNUmakefile, or at least one C++ source file (top-level or
# in src/), is treated as an application module. Directories that provide
# neither (shared headers, build output, ...) are ignored, so adding or
# removing an application never requires changes to this file.
#
# Optional per-application payloads:
#   <app>/payload/...  files copied into ../rootfs/dir preserving their
#   relative paths. Symlinks are dereferenced and broken links are skipped.
#   Every installed file is recorded in a manifest so payloads belonging to
#   removed applications are deleted automatically on the next build.

.DEFAULT_GOAL := all

ALL_DIRS := $(wildcard */)
MODULES_WITH_MAKEFILE := $(foreach d,$(ALL_DIRS),\
	$(if $(wildcard $(d)GNUmakefile),$(d)))
MODULES_WITH_SOURCES := $(foreach d,$(ALL_DIRS),\
	$(if $(or $(wildcard $(d)*.cpp),$(wildcard $(d)src/*.cpp)),$(d)))
APPS := $(sort $(patsubst %/,%,$(MODULES_WITH_MAKEFILE) $(MODULES_WITH_SOURCES)))

APP_BIN_DIR := ../rootfs/dir/bin
ROOTFS_DIR := ../rootfs/dir
PAYLOAD_MANIFEST := ../rootfs/.app-payload-manifest
PAYLOAD_INSTALL := ./install_payload.sh

APP_BINARIES := $(addprefix $(APP_BIN_DIR)/,$(APPS))
OBSOLETE_APP_BINARIES := $(filter-out $(APP_BINARIES),$(wildcard $(APP_BIN_DIR)/*))

.PHONY: all clean-payload clean distclean

all: clean-payload
	@mkdir -p $(APP_BIN_DIR)
	@rm -f $(OBSOLETE_APP_BINARIES)
	@for app in $(APPS); do \
		if [ -f $$app/GNUmakefile ]; then \
			$(MAKE) -C $$app || exit 1; \
		else \
			$(MAKE) -C $$app -f ../app.mk APP=$$app || exit 1; \
		fi; \
		cp -v $$app/$$app.napp $(APP_BIN_DIR)/$$app || exit 1; \
	done
	@for app in $(APPS); do \
		if [ -d $$app/payload ]; then \
			$(PAYLOAD_INSTALL) $$app/payload $(ROOTFS_DIR) >> $(PAYLOAD_MANIFEST) || exit 1; \
		fi; \
	done

# Remove every file a previous build installed through an application
# payload, then drop directories that are left empty.
clean-payload:
	@if [ -f $(PAYLOAD_MANIFEST) ]; then \
		while IFS= read -r f; do \
			rm -f "$(ROOTFS_DIR)/$$f"; \
		done < $(PAYLOAD_MANIFEST); \
	fi
	@rm -f $(PAYLOAD_MANIFEST)
	@find "$(ROOTFS_DIR)" -mindepth 1 -depth -type d -empty -delete

# Remove object files, napp images and installed payloads.
clean: clean-payload
	@rm -f $(OBSOLETE_APP_BINARIES)
	@for app in $(APPS); do \
		if [ -f $$app/GNUmakefile ]; then \
			$(MAKE) -C $$app clean || exit 1; \
		else \
			$(MAKE) -C $$app -f ../app.mk APP=$$app clean || exit 1; \
		fi; \
		rm -f $(APP_BIN_DIR)/$$app; \
	done

# Remove all built and generated application outputs.
distclean: clean-payload
	@rm -f $(OBSOLETE_APP_BINARIES)
	@for app in $(APPS); do \
		if [ -f $$app/GNUmakefile ]; then \
			$(MAKE) -C $$app distclean || exit 1; \
		else \
			$(MAKE) -C $$app -f ../app.mk APP=$$app distclean || exit 1; \
		fi; \
		rm -f $(APP_BIN_DIR)/$$app; \
	done
