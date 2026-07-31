#include "../driver.hpp"

void Uart::init() 
{
    outb(COM1_PORT + 1, 0x00);    // Wyłącz przerwania
    outb(COM1_PORT + 3, 0x80);    // Włącz DLAB
    outb(COM1_PORT + 0, 0x01);    // 115200 baud (low byte)
    outb(COM1_PORT + 1, 0x00);    // (high byte)
    outb(COM1_PORT + 3, 0x03);    // 8 bitów, brak parzystości, 1 bit stopu
    outb(COM1_PORT + 2, 0xC7);    // Włącz i wyczyść FIFO
    outb(COM1_PORT + 4, 0x0B);    // IRQs włączone, RTS/DSR ustawione
}

void Uart::putc(char c) 
{
    while (!is_transmit_empty());
    outb(COM1_PORT, c);
}

void Uart::puts(const char* str) 
{
    while (*str) 
    {
        if (*str == '\n') 
        {
            putc('\r');
        }
        putc(*str++);
    }
}

void Uart::puthex(uint64_t val) 
{
    const char* hex_digits = "0123456789ABCDEF";
    puts("0x");
    for (int i = 60; i >= 0; i -= 4) 
    {
        putc(hex_digits[(val >> i) & 0xF]);
    }
}

void Uart::putdec(uint64_t val)
{
    char buffer[32];
    int i = 0;

    if(val == 0)
    {
        putc('0');
        return;
    }

    while(val > 0)
    {
        buffer[i++] = '0' + (val % 10);

        val /= 10;
    }

    while(i > 0)
    {
        putc(buffer[--i]);
    }
}
