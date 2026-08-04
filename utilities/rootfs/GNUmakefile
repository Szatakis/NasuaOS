IMAGE = rootfs.img
DIR_FOLDER = dir

SIZE_MB = 64

.PHONY: all
all: $(IMAGE)

$(IMAGE):
	@echo "Tworzenie pustego obrazu $(IMAGE)..."
	dd if=/dev/zero of=$(IMAGE) bs=1M count=$(SIZE_MB)
	
	@echo "Formatowanie obrazu jako FAT32..."
	mformat -i $(IMAGE) -F ::
	
	@echo "Kopiowanie zawartości folderu dir/ do obrazu..."
	mcopy -i $(IMAGE) -s $(DIR_FOLDER)/* ::/

.PHONY: clean
clean:
	rm -f $(IMAGE)

.PHONY: distclean
distclean:
	rm -f *.img