APP := doom
TARGET := $(APP).napp
ELF := $(APP).elf

ENGINE_DIR := third_party/doomgeneric
OBJ_DIR := obj
LINKER_SCRIPT := napp.lds
INCLUDE_DIR := include

CXX := g++
CC := gcc

DOOM_OBJECTS := $(shell sed -n 's/^SRC_DOOM = //p' $(ENGINE_DIR)/Makefile.soso)
DOOM_OBJECTS := $(filter-out w_file_stdc.o doomgeneric_soso.o,$(DOOM_OBJECTS))
ENGINE_OBJECTS := $(addprefix $(OBJ_DIR)/engine/,$(DOOM_OBJECTS))
APP_OBJECTS := $(OBJ_DIR)/app/doom.o $(OBJ_DIR)/app/doom_runtime.o
PORT_OBJECTS := $(OBJ_DIR)/app/doomgeneric_nasua.o $(OBJ_DIR)/app/w_file_nasua.o
OBJECTS := $(ENGINE_OBJECTS) $(APP_OBJECTS) $(PORT_OBJECTS)

COMMON_FLAGS := \
	-O2 \
	-Wall \
	-Wextra \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-lto \
	-fno-plt \
	-fPIC \
	-m64 \
	-mno-red-zone \
	-mno-mmx \
	-mcmodel=small \
	-DNORMALUNIX \
	-DLINUX \
	-DSNDSERV \
	-D_DEFAULT_SOURCE \
	-U_FORTIFY_SOURCE \
	-D_FORTIFY_SOURCE=0 \
	-DNASUA_NAPP \
	-DDOOMGENERIC_RESX=320 \
	-DDOOMGENERIC_RESY=200 \
	-I$(INCLUDE_DIR) \
	-I$(ENGINE_DIR)

CFLAGS := $(COMMON_FLAGS) -std=gnu11
CXXFLAGS := $(COMMON_FLAGS) -std=gnu++20 -fno-exceptions -fno-rtti \
	-mno-sse -mno-sse2 -mno-80387
UPSTREAM_CFLAGS := $(CFLAGS) \
	-Wno-dangling-pointer \
	-Wno-enum-conversion \
	-Wno-implicit-fallthrough \
	-Wno-missing-field-initializers \
	-Wno-pointer-to-int-cast \
	-Wno-sign-compare \
	-Wno-type-limits \
	-Wno-unterminated-string-initialization \
	-Wno-unused-but-set-parameter \
	-Wno-unused-but-set-variable \
	-Wno-unused-const-variable \
	-Wno-unused-parameter \
	-Wno-unused-variable

LDFLAGS := \
	-nostdlib \
	-static \
	-Wl,--no-dynamic-linker \
	-Wl,--build-id=none \
	-Wl,--no-relax \
	-Wl,-T,$(LINKER_SCRIPT) \
	-Wl,--gc-sections \
	-Wl,--emit-relocs

.PHONY: all clean distclean
all: $(TARGET)

$(OBJ_DIR)/engine/%.o: $(ENGINE_DIR)/%.c
	mkdir -p $(@D)
	$(CC) $(UPSTREAM_CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/%.o: src/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/app/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) $(LINKER_SCRIPT) tools/pack_relocations.py
	$(CXX) $(LDFLAGS) -o $(OBJ_DIR)/$(ELF) $(OBJECTS) -lgcc
	objcopy -O binary $(OBJ_DIR)/$(ELF) $@
	python3 tools/pack_relocations.py $(OBJ_DIR)/$(ELF) $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

distclean: clean
