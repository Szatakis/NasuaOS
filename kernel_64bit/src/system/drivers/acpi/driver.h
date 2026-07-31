#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>
#include <stdbool.h>


bool acpi_init(); // Inits ACPI
void acpi_shutdown(); //Shutdowns PC
void acpi_reboot(); // Reboots PC


#endif