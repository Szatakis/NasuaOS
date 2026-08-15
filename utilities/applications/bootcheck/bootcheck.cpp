extern "C" void _start()
{
    // tutaj później dodamy syscall do GUI

    while (true)
    {
        asm volatile("hlt");
    }
}