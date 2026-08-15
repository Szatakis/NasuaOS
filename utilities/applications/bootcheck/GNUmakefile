CXX = g++

CXXFLAGS = \
	-ffreestanding \
	-fno-exceptions \
	-fno-rtti \
	-fno-stack-protector \
	-fno-pic \
	-mno-red-zone \
	-mcmodel=small

LDFLAGS = \
	-nostdlib \
	-static \
	-no-pie

TARGET = bootcheck

.PHONY: all

all: $(TARGET)

$(TARGET): bootcheck.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) bootcheck.cpp -o $(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)

.PHONY: distclean
distclean:
	rm -f $(TARGET)