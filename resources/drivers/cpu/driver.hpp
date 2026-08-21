#ifndef CPU_H
#define CPU_H

#pragma once
#include <stdint.h>

class Cpu
{
public:
    static void init_cores();          // Check if mp is available
    static void get_brand(char* brand); // Return CPU name
    static const char* get_architecture();    // Returns CPU architecture
};

#endif