#include "../driver.hpp"

#include "system/drivers/keyboard/driver.hpp"
#include "system/drivers/uart/driver.hpp"

#include "system/fonts/fonts.hpp"
#include "system/gui/vars/colors.hpp"

#include "applications/shell/commands.hpp"
#include "applications/terminal/terminal.hpp"

#include "libs/libc/libc.h"

#define TEXT_COLS 160
#define TEXT_ROWS 80

typedef struct {
    char ch;
    uint32_t color;
} TermChar;

static TermChar text_buffer[TEXT_ROWS][TEXT_COLS];

static size_t text_col = 0;
static size_t text_row = 0;

uint32_t current_text_color = COLOR_WHITE;

size_t cursor_x = 0;
size_t cursor_y = 0;

const uint32_t FONT_SPACING_W = 1;
const uint32_t FONT_SPACING_H = 4;

extern char current_path[256];
extern char user_name[16];
extern char pc_name[16];

void init_text_buffer() 
{
    text_col = 0;
    text_row = 0;

    for (size_t r = 0; r < TEXT_ROWS; r++) 
    {
        for (size_t c = 0; c < TEXT_COLS; c++) 
        {
            text_buffer[r][c].ch = ' ';
            text_buffer[r][c].color = COLOR_WHITE;
        }
    }

    fetch();
    print(CMD_TEXT_WHITE);
    print("Enter Command\n");
    print_cmd();
}

static void scroll_text() 
{
    for (size_t r = 1; r < TEXT_ROWS; r++) 
    {
        for (size_t c = 0; c < TEXT_COLS; c++) 
        {
            text_buffer[r - 1][c] = text_buffer[r][c];
        }
    }

    for (size_t c = 0; c <TEXT_COLS; c++) 
    {
        text_buffer[TEXT_ROWS - 1][c].ch = ' ';
        text_buffer[TEXT_ROWS - 1][c].color = COLOR_WHITE;
    }
    if (text_row > 0) 
    {
        text_row--;
    }
}

void draw_text_buffer() 
{
    size_t start_y = 0;
    
    for (size_t r = 0; r < TEXT_ROWS; r++) 
    {
        size_t start_x = 0;
        
        if (start_y + 8 >= (fb->height - 45)) 
        {
            break;
        }

        for (size_t c = 0; c < TEXT_COLS; c++) 
        {
            if (start_x + 8 >= fb->width) 
            {
                break;
            }

            char ch = text_buffer[r][c].ch;
            if (ch != ' ' && ch != '\0') 
            {
                draw_char8(ch, start_x, start_y, text_buffer[r][c].color);
            }
            
            start_x += 8 + FONT_SPACING_W;
        }
        start_y += 8 + FONT_SPACING_H;
    }
    
    cursor_x = text_col * (8 + FONT_SPACING_W);
    cursor_y = text_row * (8 + FONT_SPACING_H);
}

void print_char8(char c) 
{
    if (active_terminal_redirect) 
    {
        terminal_write_char(c);
        return;
    }

    if (c == '\n') 
    {
        text_col = 0;
        text_row++;

        size_t max_rows = (fb->height - 45) / (8 + FONT_SPACING_H);
        if (text_row >= max_rows || text_row >= TEXT_ROWS) 
        {
            scroll_text();
        }
        return;
    }

    size_t max_cols = fb->width / (8 + FONT_SPACING_W);
    if (text_col >= max_cols || text_col >= TEXT_COLS) 
    {
        print_char8('\n');
    }

    text_buffer[text_row][text_col].ch = c;
    text_buffer[text_row][text_col].color = current_text_color;
    
    text_col++;
}

void print_num8(uint32_t num) 
{
    char buffer[12];
    int i = 0;

    if (num == 0) 
    {
        print("0");
        return;
    }

    while (num > 0) 
    {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i > 0) 
    {
        print_char8(buffer[--i]);
    }
}

void print(const char* str) 
{
    while (*str) 
    {
        if (*str == '\x1B' && *(str + 1) == '[') 
        {
            char code = *(str + 2);

            switch (code) 
            {
                case 'w': current_text_color = COLOR_WHITE; break;
                case 'r': current_text_color = COLOR_RED; break;
                case 'g': current_text_color = COLOR_GREEN; break;
                case 'b': current_text_color = COLOR_BLUE; break;
                case 'y': current_text_color = COLOR_YELLOW; break;
                case 'a': current_text_color = COLOR_GRAY; break;
                case 'e': current_text_color = COLOR_TERM_INFO; break;
                case 't': current_text_color = COLOR_TERM_WARN; break;
                case 'u': current_text_color = COLOR_TERM_ERROR; break;
            }

            str += 3;
            continue;
        }

        print_char8(*str++);
    }
}

void print_int(int value) 
{
    char buffer[32];
    int i = 0;

    if (value == 0) 
    {
        print("0");
        return;
    }

    if (value < 0) 
    {
        print("-");
        value = -value;
    }

    while (value > 0) 
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    for (int j = i - 1; j >= 0; j--) 
    {
        char c[2];
        c[0] = buffer[j];
        c[1] = '\0';
        print(c);
    }
}

void print_hex(uint32_t value)
{
    print("0x");

    bool started = false;

    for (int i = 7; i >= 0; i--)
    {
        uint8_t digit = (value >> (i * 4)) & 0xF;

        if (digit != 0 || started || i == 0)
        {
            started = true;

            char c;

            if (digit < 10)
                c = '0' + digit;
            else
                c = 'A' + (digit - 10);

            print_char8(c);
        }
    }
}

void print_at8(const char* str, size_t x, size_t y, uint32_t color) 
{
    size_t start_x = x;
    while (*str) 
    {
        if (*str == '\n') 
        {
            x = start_x;
            y += 8 + FONT_SPACING_H;
            str++;
            continue;
        }
        draw_char8(*str, x, y, color);
        x += 8 + FONT_SPACING_W;
        str++;
    }
}

void print_at10(const char* str, size_t x, size_t y, uint32_t color) 
{
    size_t start_x = x;
    while (*str) 
    {
        if (*str == '\n') 
        {
            x = start_x;
            y += 10 + FONT_SPACING_H;
            str++;
            continue;
        }
        draw_char10(*str, x, y, color);
        x += 10 + FONT_SPACING_W;
        str++;
    }
}

void print_at12(const char* str, size_t x, size_t y, uint32_t color) 
{
    size_t start_x = x;
    while (*str) 
    {
        if (*str == '\n') 
        {
            x = start_x;
            y += 12 + FONT_SPACING_H;
            str++;
            continue;
        }
        draw_char12(*str, x, y, color);
        x += 12 + FONT_SPACING_W;
        str++;
    }
}

void print_at16(const char* str, size_t x, size_t y, uint32_t color) 
{
    size_t start_x = x;
    while (*str) 
    {
        if (*str == '\n') 
        {
            x = start_x;
            y += 16 + FONT_SPACING_H;
            str++;
            continue;
        }
        draw_char16(*str, x, y, color);
        x += 16 + FONT_SPACING_W;
        str++;
    }
}

void delete_last_char(size_t font_size) 
{
    if (text_col > 0) 
    {
        text_col--;
        text_buffer[text_row][text_col].ch = ' ';
    } else if (text_row > 0) 
    {
        text_row--;

        size_t max_cols = fb->width / (font_size + FONT_SPACING_W);
        text_col = (max_cols < TEXT_COLS) ? max_cols - 1 : TEXT_COLS - 1;
        text_buffer[text_row][text_col].ch = ' ';
    }
}

void clear_line() 
{
    size_t start_col = text_col - cmd_idx;
    
    for (size_t c = start_col; c < text_col; c++) 
    {
        text_buffer[text_row][c].ch = ' ';
    }
    
    text_col = start_col;

    for (size_t i = 0; i < sizeof(command_buffer); i++) 
    {
        command_buffer[i] = '\0';
    }
    cmd_idx = 0;
}

void print_info(const char* msg) 
{
    print(CMD_TERM_INFO);
    print("[info]: ");
    print(CMD_TEXT_WHITE);
    print(msg);
}

void print_warn(const char* msg) 
{
    print(CMD_TERM_WARN);
    print("[warning]: ");
    print(CMD_TEXT_WHITE);
    print(msg);
}

void print_error(const char* msg) 
{
    print(CMD_TERM_ERROR);
    print("[error]: ");
    print(CMD_TEXT_WHITE);
    print(msg);
}

void fetch() 
{
    print(CMD_TEXT_WHITE);
    print("\n $$\\   $$\\                                          $$$$$$\\   $$$$$$\\  \n");
    print(" $$$\\  $$ |                                        $$  __$$\\ $$  __$$\\ \n");
    print(" $$$$\\ $$ | $$$$$$\\   $$$$$$$\\ $$\\   $$\\  $$$$$$\\  $$ /  $$ |$$ /  \\__|\n");
    print(" $$$$\\ $$ | $$$$$$\\   $$$$$$$\\ $$\\   $$\\  $$$$$$\\  $$ /  $$ |$$ /  \\__|\n");
    print(" $$ $$\\$$ | \\____$$\\ $$  _____|$$ |  $$ | \\____$$\\ $$ |  $$ |\\$$$$$$\\  \n");
    print(" $$ \\$$$$ | $$$$$$$ |\\$$$$$$\\  $$ |  $$ | $$$$$$$ |$$ |  $$ | \\____$$\\ \n");
    print(" $$ |\\$$$ |$$  __$$ | \\____$$\\ $$ |  $$ |$$  __$$ |$$ |  $$ |$$\\   $$ |\n");
    print(" $$ | \\$$ |\\$$$$$$$ |$$$$$$$  |\\$$$$$$  |\\$$$$$$$ | $$$$$$  |\\$$$$$$  |\n");
    print(" \\__|  \\__| \\_______|\\_______/  \\______/  \\_______| \\______/  \\______/ \n\n");
}

void print_cmd() 
{
    print(CMD_TEXT_BLUE);
    print(user_name);
    print("@");
    print(pc_name);
    print(CMD_TEXT_WHITE); 
    print(":");
    print(CMD_TEXT_YELLOW);
    print(current_path);
    print(CMD_TEXT_GRAY);
    print("> ");
    print(CMD_TEXT_WHITE); 
}

void print_num_padded(uint32_t num) 
{
    char buf[3];
    buf[0] = '0' + (num / 10);
    buf[1] = '0' + (num % 10);
    buf[2] = '\0';
    print(buf);
}

void print_year(uint32_t year) 
{
    char buf[5];
    buf[0] = '0' + (year / 1000);
    buf[1] = '0' + ((year % 1000) / 100);
    buf[2] = '0' + ((year % 100) / 10);
    buf[3] = '0' + (year % 10);
    buf[4] = '\0';
    print(buf);
}

void print_resolution() 
{
    if (!fb) 
    {
        Uart::puts("Framebuffer not available\n");
        return;
    }
    
    print_info("Screen resolution: ");
    print_int(fb->width);
    print("x");
    print_int(fb->height);
    print("\n");
}
