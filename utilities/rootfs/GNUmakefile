IMAGE = rootfs.img
DIR_FOLDER = dir

SIZE_MB = 8

.PHONY: all
all:
	$(MAKE) -C ../applications

	dd if=/dev/zero of=$(IMAGE) bs=1M count=$(SIZE_MB)
	mformat -i $(IMAGE) -F ::
	
	mcopy -i $(IMAGE) -s $(DIR_FOLDER)/* ::/

.PHONY: clean
clean:
	rm -f $(IMAGE)

	$(MAKE) -C ../applications clean

.PHONY: distclean
distclean:
	rm -f *.img

	$(MAKE) -C ../applications distclean