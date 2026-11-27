# Nuke built-in rules.
.SUFFIXES:

LLVM_TRIPLE := aarch64-unknown-none-elf
UART_BASE := 0x09000000
TEXT_ADDR := 0x40080000
ARCH_NAME := \"aarch64\"
BITS := 64

CC := clang
LD := ld.lld

CFLAGS := -target $(LLVM_TRIPLE) -ffreestanding -fno-builtin -nostdinc \
	-ffunction-sections -fdata-sections -fno-stack-protector -O1 -Wall -Wextra 	-DARCH_NAME=$(ARCH_NAME) -DBITS=$(BITS) -DUART_BASE=$(UART_BASE) -DUSE_FW_CFG_RAMFB -DFW_CFG_BASE=0x09020000UL -DFB_PHYS=0x48000000UL
LDFLAGS := -nostdlib -static --gc-sections -z noseparate-code -T link.ld

SRCS := $(shell find src -type f \( -name '*.c' -o -name '*.S' \) | LC_ALL=C sort)
OBJS := $(patsubst src/%.c,obj/%.o,$(filter %.c,$(SRCS))) \
	$(patsubst src/%.S,obj/%.o,$(filter %.S,$(SRCS)))

OUTPUT := bin/kernel

.PHONY: all clean distclean
all: $(OUTPUT)

obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT): $(OBJS)
	@mkdir -p bin
	$(LD) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -rf obj bin

distclean: clean
