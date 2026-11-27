#include <stdint.h>

void halt();
void reboot();
void init_shutdown(uint32_t mbi_addr);
void shutdown();
void check_modules(uint32_t mbi_addr);