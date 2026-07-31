#include "kernel_panic.hpp"

#include "libs/libc/libc.h"
#include "libs/asm/asm.h"

#include "system/drivers/video/driver.hpp"
#include "system/drivers/cpu/driver.h"
#include "system/drivers/timer/driver.h"

#define PANIC_WIDTH 45
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 13

#define PANIC_LINES 12


uint32_t panic_start_x() 
{
    uint32_t char_size = 8 + FONT_SPACING_W;
    uint32_t dialog_width = PANIC_WIDTH * char_size;
    return (fb->width - dialog_width) / 2;
}

void panic_set_position() 
{
    uint32_t height = PANIC_LINES * CHAR_HEIGHT;

    cursor_x = panic_start_x();
    cursor_y = (fb->height - height) / 2;
}

void panic_print_border() 
{
    cursor_x = panic_start_x();

    print("+");

    for (int i = 0; i < PANIC_WIDTH - 2; i++) 
    {
        print("-");
    }

    print("+");

    cursor_y += CHAR_HEIGHT;
    cursor_x = panic_start_x();
}

void panic_print_empty() 
{
    cursor_x = panic_start_x();

    print("|");

    for (int i = 0; i < PANIC_WIDTH - 2; i++) 
    {
        print(" ");
    }

    print("|");

    cursor_y += CHAR_HEIGHT;
    cursor_x = panic_start_x();
}

void panic_print_center_title(const char* text)
{
    cursor_x = panic_start_x();

    size_t len = strlen(text);
    uint32_t spaces = 0;

    if (len < PANIC_WIDTH - 2)
        spaces = (PANIC_WIDTH - 2 - len) / 2;

    print("|");

    for (uint32_t i = 0; i < spaces; i++)
        print(" ");

    print(text);

    uint32_t remaining =
        PANIC_WIDTH - 2 - spaces - len;

    for (uint32_t i = 0; i < remaining; i++)
        print(" ");

    print("|");

    cursor_y += CHAR_HEIGHT;
    cursor_x = panic_start_x();
}

void panic_print_line(const char* text)
{
    cursor_x = panic_start_x();

    size_t len = strlen(text);


    print("| ");
    print(text);

    uint32_t spaces =
        PANIC_WIDTH - 3 - len;

    for (uint32_t i = 0; i < spaces; i++)
    {
        print(" ");
    }

    print("|");

    cursor_y += CHAR_HEIGHT;
    cursor_x = panic_start_x();
}

void panic_line(const char* prefix, const char* value)
{
    char buffer[256];

    buffer[0] = '\0';

    strcat(buffer, prefix);
    strcat(buffer, value);

    panic_print_line(buffer);
}


void kernel_panic(const char* message, const char* error_code, const char* rip, const char* rsp, const char* fault_address, const char* pid)
{
    struct PanicInfo
    {
        const char* error_code = "";
        const char* rip = "";
        const char* rsp = "";
        const char* fault_address = "";
        const char* cpu = "";
        const char* pid = "";
    };
    
    char cpu_name[49];
    cpu_get_brand(cpu_name);

    PanicInfo info;

    info.error_code = error_code;
    info.rip = rip;
    info.rsp = rsp;
    info.fault_address = fault_address;
    info.cpu = cpu_name;
    info.pid = pid;

    clear_screen();

    panic_set_position();
    panic_print_border();
    panic_print_center_title("KERNEL PANIC");
    panic_print_border();

    panic_print_empty();

    panic_line("Reason: ", message);
    panic_line("Error code: ", info.error_code);
    panic_line("RIP: ", info.rip);

    panic_line("RSP: ", info.rsp);

    panic_line("Fault Address: ", info.fault_address);

    panic_line("CPU: ", info.cpu);

    panic_line("PID: ", info.pid);

    panic_print_empty();
    panic_print_border();

    render_frame();

    sleep(2000);

    disable_interrupts();

    while(true)
    {
        halt_cpu();
    }
}