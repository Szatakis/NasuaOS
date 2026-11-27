IMAGE = rootfs.img
DIR_FOLDER = dir

SIZE_MB = 40
WALLPAPER_SOURCE = ../../documentation/images
USER_IMAGES_DIR = $(DIR_FOLDER)/home/user/images

.PHONY: all
all:
	$(MAKE) -C ../applications
	$(MAKE) -C ../system_functions
	mkdir -p $(USER_IMAGES_DIR)
	cp -v $(WALLPAPER_SOURCE)/background.png $(WALLPAPER_SOURCE)/background_dark.png $(USER_IMAGES_DIR)/

	dd if=/dev/zero of=$(IMAGE) bs=1M count=$(SIZE_MB)
	mformat -i $(IMAGE) ::
	
	mcopy -i $(IMAGE) -s $(DIR_FOLDER)/* ::/

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -f $(IMAGE)
	rm -f $(USER_IMAGES_DIR)/background.png $(USER_IMAGES_DIR)/background_dark.png
	$(MAKE) -C ../applications clean
	$(MAKE) -C ../system_functions clean

# Remove everything built and generated including downloaded dependencies.
.PHONY: distclean
distclean:
	rm -f $(IMAGE) *.img
	rm -f $(USER_IMAGES_DIR)/background.png $(USER_IMAGES_DIR)/background_dark.png
	rm -f .app-payload-manifest
	$(MAKE) -C ../applications distclean
	$(MAKE) -C ../system_functions distclean
