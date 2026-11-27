#pragma once
#include <stdint.h>

struct Registers;

void page_fault_handler(Registers* regs);