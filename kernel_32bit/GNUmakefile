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
	-mno-sse2 \
	-I src \
	-I ../resources \
	-I ../utilities/applications/include

LDFLAGS := \
	-m32 \
	-nostdlib \
	-no-pie


TARGET := $(BIN_DIR)/kernel_32bit

RESOURCES_DIR := ../resources
RESOURCES_CPP_SRC := $(shell find $(RESOURCES_DIR)/libs $(RESOURCES_DIR)/drivers/keyboard $(RESOURCES_DIR)/drivers/gpu -type f -name "*.cpp" 2>/dev/null)
RESOURCES_OBJ := $(patsubst $(RESOURCES_DIR)/%.cpp,$(OBJ_DIR)/resources/%.o,$(RESOURCES_CPP_SRC))


# wszystkie pliki źródłowe
CPP_SRC := $(shell find src -type f -name "*.cpp")
C_SRC   := $(shell find src -type f -name "*.c")
ASM_SRC := $(shell find src -type f -name "*.asm")


SRC := $(CPP_SRC) $(C_SRC) $(ASM_SRC)


# zamiana src/x.cpp -> obj-x86_32/x.o
OBJ := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CPP_SRC)) \
       $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRC)) \
       $(patsubst %.asm,$(OBJ_DIR)/%.o,$(ASM_SRC)) \
       $(RESOURCES_OBJ)



.PHONY: all
all: $(TARGET)



ifeq ($(filter $(ARCH),aarch32 loongarch32 riscv32 x86_32),)
	$(error Architecture $(ARCH) not supported)
endif

$(TARGET): $(OBJ) linker-scripts/$(ARCH).lds
	@mkdir -p $(BIN_DIR)
	$(LD) $(LDFLAGS) \
		-T linker-scripts/$(ARCH).lds \
		-o $@ \
		$(OBJ)

# C++
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# C
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ASM
$(OBJ_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

# Shared resources
$(OBJ_DIR)/resources/%.o: $(RESOURCES_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: distclean
distclean:
	rm -rf obj-* bin-* .deps-obtained