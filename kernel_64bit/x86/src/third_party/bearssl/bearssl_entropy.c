#include <bearssl.h>

br_prng_seeder br_prng_seeder_system(const char** name)
{
    if (name != 0)
    {
        *name = "kernel-provided entropy";
    }
    return 0;
}
