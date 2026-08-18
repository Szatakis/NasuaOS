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

SOURCES := $(wildcard *.cpp) $(wildcard src/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

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
all: $(TARGET)

$(TARGET): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(ELF): $(OBJECTS) $(LINKER_SCRIPT)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(ELF) $(TARGET)

.PHONY: distclean
distclean: 
	rm -f $(OBJECTS) $(ELF) $(TARGET)
