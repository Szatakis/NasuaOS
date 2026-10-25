IMAGE = rootfs.img
DIR_FOLDER = dir

SIZE_MB = 40

.PHONY: all
all:
	$(MAKE) -C ../applications
	$(MAKE) -C ../system_functions

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
