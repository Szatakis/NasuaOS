# Generic build rules for NasuaOS applications (.napp).
#
# Every folder inside utilities/applications is built with this makefile
# unless it provides its own GNUmakefile. All .cpp files in the folder (and
# in an optional src/ subfolder) are compiled and linked into <folder>.napp.

.SUFFIXES:

APP ?= $(notdir $(CURDIR))

TARGET := $(APP).napp
ELF := $(APP).elf

SHARED_DIR := ..
LINKER_SCRIPT := $(SHARED_DIR)/napp.lds
INCLUDE_DIR := $(SHARED_DIR)/include

CXX := g++
LD := ld
OBJCOPY := objcopy

# Directories
OBJ_DIR := obj

# Sources and Object files placed inside obj/
SOURCES := $(wildcard *.cpp) $(wildcard src/*.cpp)
OBJECTS := $(addprefix $(OBJ_DIR)/, $(notdir $(SOURCES:.cpp=.o)))

CXXFLAGS := \
    -std=gnu++20 \
    -O2 \
    -Wall \
    -Wextra \
    -ffreestanding \
    -fno-exceptions \
    -fno-rtti \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fno-plt \
    -fPIC \
    -fvisibility=hidden \
    -m64 \
    -mno-red-zone \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-80387 \
    -mcmodel=small \
    -I $(INCLUDE_DIR)

LDFLAGS := \
    -m elf_x86_64 \
    -static \
    -nostdlib \
    --no-dynamic-linker \
    -T $(LINKER_SCRIPT)

.PHONY: all
all: $(OBJ_DIR) $(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJ_DIR)/$(ELF)
	$(OBJCOPY) -O binary $< $@

$(OBJ_DIR)/$(ELF): $(OBJECTS) $(LINKER_SCRIPT)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# Pattern rule to compile .cpp files into obj/
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Support for optional src/ subfolder source files
$(OBJ_DIR)/%.o: src/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: distclean
distclean: 
	rm -rf $(OBJ_DIR) $(TARGET)