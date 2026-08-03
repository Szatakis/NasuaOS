.SUFFIXES:

ARCH ?= x86_32

AS := nasm
LD := ld

OBJ_DIR := obj-$(ARCH)
BIN_DIR := bin-$(ARCH)

OBJ := $(OBJ_DIR)/boot.o
BIN := $(BIN_DIR)/fwsetup

.PHONY: all
all: $(BIN)

$(BIN): $(OBJ) linker.lds
	@mkdir -p $(BIN_DIR)
	$(LD) -m elf_i386 -T linker.lds -o $@ $(OBJ)

$(OBJ_DIR)/boot.o: src/boot.asm
	@mkdir -p $(OBJ_DIR)
	$(AS) -f elf32 $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: distclean
distclean:
	rm -rf obj-* bin-*