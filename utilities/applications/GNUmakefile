# Builds every application folder into a .napp image and installs it into the
# rootfs at /bin/<app>/<app>.napp. New applications are picked up automatically,
# a folder with C++ sources is enough.

APPS := $(wildcard */)
APPS := $(patsubst %/,%,$(APPS))
APPS := $(filter-out include,$(APPS))
APP_BIN_DIR := ../rootfs/dir/bin
APP_BINARIES := $(addprefix $(APP_BIN_DIR)/,$(APPS))
OBSOLETE_APP_BINARIES := $(filter-out $(APP_BINARIES),$(wildcard $(APP_BIN_DIR)/*))
DOOM_LICENSE := doom/third_party/doomgeneric-LICENSE
FREEDOOM_LICENSE := doom/third_party/freedoom/COPYING.txt
FREEDOOM_CREDITS := doom/third_party/freedoom/CREDITS.txt
DOOM_IWAD := doom/data/freedoom2.wad
DOOM_ROOTFS_DIR := ../rootfs/dir
DOOM_LICENSE_DIR := $(DOOM_ROOTFS_DIR)/share/licenses/doom

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
	test -f $(DOOM_IWAD) || { echo "Missing $(DOOM_IWAD); provide Freedoom 2 IWAD before building the system."; exit 1; }
	mkdir -p $(DOOM_ROOTFS_DIR)/home $(DOOM_LICENSE_DIR)
	cp -v $(DOOM_IWAD) $(DOOM_ROOTFS_DIR)/home/freedoom2.wad
	cp -v $(DOOM_LICENSE) $(DOOM_LICENSE_DIR)/GPL-2.0.txt
	cp -v $(FREEDOOM_LICENSE) $(DOOM_LICENSE_DIR)/FREEDOOM-COPYING.txt
	cp -v $(FREEDOOM_CREDITS) $(DOOM_LICENSE_DIR)/FREEDOOM-CREDITS.txt

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -f $(OBSOLETE_APP_BINARIES)
	rm -f $(DOOM_ROOTFS_DIR)/home/freedoom2.wad
	rm -f $(DOOM_LICENSE_DIR)/GPL-2.0.txt $(DOOM_LICENSE_DIR)/FREEDOOM-COPYING.txt $(DOOM_LICENSE_DIR)/FREEDOOM-CREDITS.txt
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
	rm -f $(DOOM_ROOTFS_DIR)/home/freedoom2.wad
	rm -f $(DOOM_LICENSE_DIR)/GPL-2.0.txt $(DOOM_LICENSE_DIR)/FREEDOOM-COPYING.txt $(DOOM_LICENSE_DIR)/FREEDOOM-CREDITS.txt
	@for app in $(APPS); do \
		if [ -f $$app/GNUmakefile ]; then \
			$(MAKE) -C $$app distclean; \
		else \
			$(MAKE) -C $$app -f ../app.mk APP=$$app distclean; \
		fi; \
		rm -f $(APP_BIN_DIR)/$$app; \
	done
