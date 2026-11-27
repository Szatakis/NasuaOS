# Nuke built-in rules.
.SUFFIXES:

LLVM_TRIPLE := riscv64-unknown-none-elf

CC := clang
LD := ld.lld

CFLAGS := -target $(LLVM_TRIPLE) -ffreestanding -fno-builtin -nostdinc \
	-ffunction-sections -fdata-sections -fno-stack-protector -mcmodel=medany \
	-O2 -Wall -Wextra
LDFLAGS := -nostdlib -static --gc-sections -z noseparate-code -T link.ld

SRCS := $(shell find src -type f \( -name '*.c' -o -name '*.S' \) | LC_ALL=C sort)
OBJS := $(patsubst src/%.c,obj/%.o,$(filter %.c,$(SRCS))) \
	$(patsubst src/%.S,obj/%.o,$(filter %.S,$(SRCS)))

OUTPUT := bin-riscv64/boot_mgr

.PHONY: all clean distclean
all: $(OUTPUT)

obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -rf obj bin-riscv64

distclean: clean
