IMAGE = rootfs.img
DIR_FOLDER = dir

SIZE_MB = 40
WALLPAPER_SOURCE = ../../documentation/images
USER_IMAGES_DIR = $(DIR_FOLDER)/home/user/images

.PHONY: all
all:
	$(MAKE) -C ../applications
	$(MAKE) -C ../system_functions
	rm -f "$(DIR_FOLDER)/share/models/nai/SmolLM2-135M-Instruct-IQ4_XS.gguf" \
		"$(DIR_FOLDER)/share/models/nai/LICENSE-2.0.txt" \
		"$(DIR_FOLDER)/share/models/nai/NOTICE.md" \
		"$(DIR_FOLDER)/share/models/nai/LLAMA-CPP-MIT.txt"
	rm -f "$(DIR_FOLDER)/share/licenses/nai/LICENSE-2.0.txt" \
		"$(DIR_FOLDER)/share/licenses/nai/NOTICE.md" \
		"$(DIR_FOLDER)/share/licenses/nai/LLAMA-CPP-MIT.txt"

	mkdir -p $(USER_IMAGES_DIR)
	cp -v $(WALLPAPER_SOURCE)/background.png $(WALLPAPER_SOURCE)/background_dark.png $(USER_IMAGES_DIR)/

	dd if=/dev/zero of=$(IMAGE) bs=1M count=$(SIZE_MB)
	mformat -i $(IMAGE) ::
	
	mcopy -i $(IMAGE) -s $(DIR_FOLDER)/* ::/

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -f $(IMAGE)

	$(MAKE) -C ../applications clean
	$(MAKE) -C ../system_functions clean

# Remove everything built and generated including downloaded dependencies.
.PHONY: distclean
distclean:
	rm -f *.img

	$(MAKE) -C ../applications distclean
	$(MAKE) -C ../system_functions distclean
