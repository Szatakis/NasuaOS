.SUFFIXES:

ARCH ?= x86_32

AS := nasm
CXX := g++
LD := ld

OBJ_DIR := obj-$(ARCH)
BIN_DIR := bin-$(ARCH)

BIN := $(BIN_DIR)/hdtest

OBJS := \
	$(OBJ_DIR)/boot.o \
	$(OBJ_DIR)/hdtest.o


CXXFLAGS := \
	-m32 \
	-ffreestanding \
	-fno-pie \
	-fno-pic \
	-fno-stack-protector \
	-fno-exceptions \
	-fno-rtti \
	-fno-use-cxa-atexit \
	-mno-red-zone \
	-O2 \
	-Wall \
	-Wextra \
	-I src


LDFLAGS := \
	-m elf_i386 \
	-T linker.lds


.PHONY: all
all: $(BIN)


$(BIN): $(OBJS) linker.lds
	@mkdir -p $(BIN_DIR)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(OBJ_DIR)/boot.o: src/boot.asm
	@mkdir -p $(OBJ_DIR)
	$(AS) -f elf32 $< -o $@

$(OBJ_DIR)/hdtest.o: src/hdtest.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@



.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)


.PHONY: distclean
distclean:
	rm -rf obj-* bin-*