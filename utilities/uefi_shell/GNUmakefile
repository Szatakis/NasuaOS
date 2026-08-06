.SUFFIXES:

ARCH ?= x86_64

AS := nasm
LD := lld-link

OBJ_DIR := obj-$(ARCH)
BIN_DIR := bin-$(ARCH)

OBJ := $(OBJ_DIR)/boot.obj
EFI := $(BIN_DIR)/uefi_shell.efi


.PHONY: all
all: $(EFI)

$(EFI): $(OBJ)
	@mkdir -p $(BIN_DIR)

	$(LD) \
		/subsystem:efi_application \
		/entry:_start \
		/out:$@ \
		$<


$(OBJ): src/boot.asm
	@mkdir -p $(OBJ_DIR)

	$(AS) \
		-f win64 \
		$< \
		-o $@


.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: distclean
distclean:
	rm -rf obj-* bin-*