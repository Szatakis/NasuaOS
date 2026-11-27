#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>
#include <stdbool.h>

class Acpi
{
public:
    static bool init();       // Inits ACPI
    static void shutdown();   // Shutdowns PC
    static void reboot();     // Reboots PC
};

#endif