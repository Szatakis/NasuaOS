.SUFFIXES:

ARCH ?= x86_32

AS := nasm
CC := g++
LD := g++

OBJ_DIR := obj-$(ARCH)
BIN_DIR := bin-$(ARCH)

CFLAGS := \
	-m32 \
	-ffreestanding \
	-fno-exceptions \
	-fno-rtti \
	-fno-stack-protector \
	-fno-pic \
	-fno-pie \
	-O2 \
	-Wall \
	-Wextra \
	-mno-sse \
	-mno-sse2

LDFLAGS := \
	-m32 \
	-nostdlib \
	-no-pie

OBJ := \
	$(OBJ_DIR)/boot.o \
	$(OBJ_DIR)/kernel.o

BIN := $(BIN_DIR)/kernel_32bit

.PHONY: all
all: $(BIN)
$(BIN): $(OBJ) linker.lds
	@mkdir -p $(BIN_DIR)
	$(LD) $(LDFLAGS) -T linker.lds -o $@ $(OBJ)

$(OBJ_DIR)/boot.o: src/boot.asm
	@mkdir -p $(OBJ_DIR)
	$(AS) -f elf32 $< -o $@

$(OBJ_DIR)/kernel.o: src/kernel.cpp
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: distclean
distclean:
	rm -rf obj-* bin-*