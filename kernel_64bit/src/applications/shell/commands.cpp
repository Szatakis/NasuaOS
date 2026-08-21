#include <cstddef>
#include <cstdint>
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#include "commands.hpp"

#include "system/drivers/drivers.hpp"

#include "kernel/kernel_panic/kernel_panic.hpp"
#include "system/sysfunc/logger/logger.hpp"
#include "system/sysfunc/rand/rand.hpp"

#include "applications/applications.hpp"
#include "applications/settings/settings.hpp"
#include "applications/terminal/terminal.hpp"
#include "applications/suaedit/suaedit.hpp"
#include "applications/task_manager/task_manager.hpp"
#include "drivers/gpu/driver.hpp"
#include "functions/functions.hpp"

#include "system/vars/info_vars/info_vars.hpp"

#include "system/filesystem/clawfs/clawfs.hpp"
#include "system/filesystem/file_resolver/file_resolver.hpp"

#include "system/applications/napp/napp.hpp"

#include "libs/libc/libc.hpp"
#include "libs/asm/asm.hpp"

extern uint32_t Gpu::current_text_color;
extern bool debug_mode;
extern bool safe_mode;

static constexpr uint32_t HELP_COMMANDS_PER_PAGE = 13;
static constexpr uint32_t HELP_STATIC_PAGES = 10;

// Build argv array from shell args string (splits by spaces, strips quotes)
static int build_argv(const char* args, const char** argv, int max_args)
{
    if (args == nullptr || *args == '\0' || max_args <= 0)
    {
        return 0;
    }

    static char buf[256];
    strcpy(buf, args);

    int argc = 0;
    int i = 0;
    
    while (buf[i] != '\0' && argc < max_args)
    {
        // Skip leading spaces
        while (buf[i] == ' ') 
        {
            i++;
            if (buf[i] == '\0') break;
        }
        
        if (buf[i] == '\0')
        {
            break;
        }
        
        // Start of argument
        argv[argc] = &buf[i];
        argc++;
        
        if (argc >= max_args)
        {
            break;
        }
        
        // Find end of argument
        while (buf[i] != '\0' && buf[i] != ' ')
        {
            i++;
        }
        
        // Null-terminate
        if (buf[i] == ' ')
        {
            buf[i] = '\0';
            i++;
        }
    }

    return argc;
}

// Main shell function
void execute_command(const char *cmd) 
{
    Gpu::print("\n");
    
    if (cmd == nullptr || *cmd == '\0') 
    {
        Gpu::print_cmd();
        return;
    }

    const char* space_ptr = strchr(cmd, ' ');
    size_t cmd_name_len = space_ptr ? (space_ptr - cmd) : strlen(cmd);
    const char* args = space_ptr ? (space_ptr + 1) : "";

    char single_char_buf[2] = {0, 0};

    Gpu::print(CMD_TEXT_WHITE);
    // Command: help
    if (cmd_name_len == 4 && Memory::memcmp(cmd, "help", 4) == 0) 
    {
        const char* arg_check = args;

        uint32_t page = 1; // default page

        static char sbin_names[NAPP_MAX_APPLICATIONS][NAPP_MAX_NAME];
        static char sbin_descs[NAPP_MAX_APPLICATIONS][NAPP_MAX_NAME];
        uint32_t sbin_count = napp_list_sbin(sbin_names, sbin_descs, NAPP_MAX_APPLICATIONS);
        uint32_t sbin_pages = (sbin_count + HELP_COMMANDS_PER_PAGE - 1) / HELP_COMMANDS_PER_PAGE;


        if (sbin_count > NAPP_MAX_APPLICATIONS)
        {
            sbin_count = NAPP_MAX_APPLICATIONS;
        }

        if (sbin_pages == 0)
        {
            sbin_pages = 1;
        }

        uint32_t total_pages = HELP_STATIC_PAGES + sbin_pages;

        while (*arg_check == ' ') 
        {
            arg_check++;
        }

        if (*arg_check != '\0') 
        {
            if (Memory::memcmp(arg_check, "--page ", 7) != 0) 
            {
                Gpu::print_error("Syntax error!\n");
                Gpu::print_info("Usage: help --page [1-");
                char total_buf[16];
                itoa(total_pages, total_buf);
                Gpu::print(total_buf);
                Gpu::print("]:\n");
                Gpu::print_cmd();
                return;
            }
        }

        const char* page_flag = strstr(args, "--page ");

        if (page_flag) 
        {
            const char* page_num = page_flag + 7;

            page = parse_number(page_num);
        }

        if (page < 1 || page > total_pages)
        {
            Gpu::print_error("Invalid page number!\n");
            Gpu::print_info("Valid pages: 1-");
            Gpu::print_num8(total_pages);
            Gpu::print("\n");
            Gpu::print_cmd();
            return;
        }

        if (page == 1) 
        {
            Gpu::print_info("Available commands (Page 1/");
            char total_buf[16];
            itoa(total_pages, total_buf);
            Gpu::print(total_buf);
            Gpu::print("):\n");
            Gpu::print(" See page 11 and above to view commands loaded from /sbin.\n");
            Gpu::print(" -help                               - Show the first page of help\n");
            Gpu::print("   --page [1-10]                     - Show specific help page\n");
            Gpu::print(" -info                               - Display OS version and hardware info\n");
            Gpu::print(" -source                             - Show link to the OS source code\n");
            Gpu::print(" -shutdown                           - Power off the system safely\n");
            Gpu::print(" -reboot                             - Restart the computer\n");
            Gpu::print(" -echo                               - Print text to the screen or write to file\n");
            Gpu::print("   --text <text>                     - (Required) Specify the text to Gpu::print\n");
            Gpu::print("   --color [0xRRGGBB]                - (Optional) Set custom HEX text color\n");
            Gpu::print("   --file <file_name.file_ext>       - (Optional) Save the text output into a file\n");
            Gpu::print(" -asciiart                           - Convert text into large ASCII banner\n");
            Gpu::print("    --text <string>                  - (Required) Text to transform\n");
        }

        else if (page == 2) 
        {
            Gpu::print_info("Available commands (Page 2/");
            char total_buf[16];
            itoa(total_pages, total_buf);
            Gpu::print(total_buf);
            Gpu::print("):\n");
            Gpu::print(" -format                             - Format storage drive\n");
            Gpu::print("   --commands                        - Copy system commands from ISO to ClawFS\n");
            Gpu::print("   --clear                           - Completely wipe the disk without creating the base file structure\n");
            Gpu::print(" -ls                                 - List files and directories in current path\n");
            Gpu::print("    [path]                           - (Optional) List contents of specified path\n");
            Gpu::print(" -touch                              - Create a new empty file\n");
            Gpu::print("   --file <file_name.file_ext>       - (Required) Name of the file to create\n");
            Gpu::print(" -mount                              - Mount ClawFS overlay for command override\n");
            Gpu::print(" -unmount                            - Unmount ClawFS overlay\n");
            Gpu::print(" -time                               - System clock utility\n");
            Gpu::print("   --get                             - (Required 1 of 3) Display current date and time\n");
            Gpu::print("   --set <DD.MM.YYYY-HH:MM:SS>       - (Required 1 of 3) Set new system date and time\n");
            Gpu::print("   --info                            - (Required 1 of 3) Display CMOS storage and RTC battery status\n");
        }

        else if (page == 3) 
        {
            Gpu::print_info("Available commands (Page 3/");
            char total_buf[16];
            itoa(total_pages, total_buf);
            Gpu::print(total_buf);
            Gpu::print("):\n");
            Gpu::print(" -uptime                             - Display system uptime since boot\n");
            Gpu::print(" -panic                              - Trigger kernel panic for debugging\n");
            Gpu::print(" -resolution                         - Display current screen resolution and video mode information\n");
            Gpu::print(" -logs                               - Kernel log management utility\n");
            Gpu::print("    --show                           - (Required 1 of 5) Display stored kernel logs from memory\n");
            Gpu::print("    --clear                          - (Required 1 of 5) Clear kernel log buffer\n");
            Gpu::print("    --level <INFO|WARN|ERROR|DEBUG>  - (Required 1 of 5) Display only selected level messages\n");
            Gpu::print("    --subsystem <subsystem_name>     - (Required 1 of 5) Display logs from selected subsystem\n");
            Gpu::print("    --put <text>                     - (Required 1 of 5) Add custom log message with INFO level\n");
            Gpu::print(" -bootapp                            - Application manager command\n");
            Gpu::print("    --app <application_name>         - (Required) Load and execute selected application\n");
            Gpu::print("    --list                           - List all available applications\n");
            Gpu::print(" -clear                              - Clear the terminal screen\n");
        }

        else if (page == 4)
        {
            Gpu::print_info("Available commands (Page 4/");
            char total_buf[16];
            itoa(total_pages, total_buf);
            Gpu::print(total_buf);
            Gpu::print("):\n");
            Gpu::print(" -mkdir                              - Create a new directory\n");
            Gpu::print("    --dir_name <name>                - (Required) Name of the new directory\n");
            Gpu::print(" -rm                                 - Remove file or directory\n");
            Gpu::print("    --name <name>                    - (Required) Name of the item to remove\n");
            Gpu::print("    --type <file|dir>                - (Required) Specify if it is a file or directory\n");
            Gpu::print(" -cd                                 - Change current directory\n");
            Gpu::print("    [path]                           - (Optional) Path to change to (default: /home)\n");
            Gpu::print(" -uart                               - Send data over serial port\n");
            Gpu::print("   --text <text>                     - (Required) Text string to transmit via UART\n");
            Gpu::print(" -pwd                                - Display the current working directory\n");
            Gpu::print(" -safe_mode                          - Enable safe mode for system debugging\n");
            Gpu::print(" -inb                                - Read a byte from an I/O port\n");
            Gpu::print("    --port [0xHEX]                   - (Required) Specify I/O port address\n");
        }

        else if (page == 5)
        {
            Gpu::print_info("Available commands (Page 5/");
            char total_buf[16];
            itoa(total_pages, total_buf);
            Gpu::print(total_buf);
            Gpu::print("):\n");
            Gpu::print(" -outb                               - Write a byte to an I/O port\n");
            Gpu::print("    --port [0xHEX]                   - (Required) Specify I/O port address\n");
            Gpu::print("    --val [0xHEX]                    - (Required) Value byte to write\n");
            Gpu::print(" -mv                                 - Move a file or directory\n");
            Gpu::print("    --source <source>                - (Required) Source path\n");
            Gpu::print("    --destination <destination>      - (Required) Destination path\n");
            Gpu::print(" -cp                                 - Copy a file or directory\n");
            Gpu::print("    --source <source>                - (Required) Source path\n");
            Gpu::print("    --destination <destination>      - (Required) Destination path\n");
            Gpu::print(" -cat                                - Display the contents of one or more files\n");
            Gpu::print("    --file <filename>                - (Required) First file to display\n");
            Gpu::print("    --file_n <filename>              - (Optional) Additional file to display\n");
            Gpu::print(" -fetch                              - Display system summary and ASCII logo\n");
        }

        else if (page == 6)
        {
            Gpu::print_info("Available commands (Page 6/");
            char total_buf[16];
            itoa(total_pages, total_buf);
            Gpu::print(total_buf);
            Gpu::print("):\n");
            Gpu::print(" -debug                              - Enable or disable debug mode\n");
            Gpu::print("   --on                              - (Required 1 of 2) Enable debug mode\n");
            Gpu::print("   --off                             - (Required 1 of 2) Disable debug mode\n");
            Gpu::print(" -beep                               - Play a sound through the PC speaker");
            Gpu::print("   --freq [frequency]                - (Optional) Set beep frequency in Hz");
            Gpu::print("   --dur [time]                      - (Optional) Set beep duration in milliseconds");
            Gpu::print(" -calc                               - Perform a basic arithmetic calculation");
            Gpu::print("   --op <add|sub|mul|div>            - (Required) Select the arithmetic operation");
            Gpu::print("   --num1 [value]                    - (Required) First number");
            Gpu::print("   --num2 [value]                    - (Required) Second number");
            Gpu::print(" -rand                               - Generate a random number within a range");
            Gpu::print("   --min [value]                     - (Required) Minimum value of the range");
            Gpu::print("   --max [value]                     - (Required) Maximum value of the range");
        }

        else if (page == 7)
        {
            Gpu::print_info("Available commands (Page 7/");
            char total_buf[16];
            itoa(total_pages, total_buf);
            Gpu::print(total_buf);
            Gpu::print("):\n");
        }

        else if (page == 8)
        {
            Gpu::print_info("Available commands (Page 8/");
            char total_buf[16];
            itoa(total_pages, total_buf);
            Gpu::print(total_buf);
            Gpu::print("):\n");
        }

        else if (page == 9)
        {
            Gpu::print_info("Available commands (Page 9/");
            char total_buf[16];
            itoa(total_pages, total_buf);
            Gpu::print(total_buf);
            Gpu::print("):\n");
        }

        else if (page == 10)
        {
            Gpu::print_info("Available commands (Page 10/");
            char total_buf[16];
            itoa(total_pages, total_buf);
            Gpu::print(total_buf);
            Gpu::print("):\n");
        }

        else if (page > HELP_STATIC_PAGES)
        {
            if (page > total_pages)
            {
                Gpu::print_error("Invalid page number!\n");
                Gpu::print_info("Total pages: ");
                Gpu::print_num8(total_pages);
                Gpu::print("\n");
                Gpu::print_cmd();
                return;
            }

            Gpu::print_info("System commands from /sbin (Page ");
            Gpu::print_num8(page);
            Gpu::print("/");
            Gpu::print_num8(total_pages);
            Gpu::print("):\n");

            if (sbin_count == 0)
            {
                Gpu::print(" (no commands found in /sbin — rootfs may not be mounted)\n");
            }
            else
            {
                uint32_t start = (page - HELP_STATIC_PAGES - 1) * HELP_COMMANDS_PER_PAGE;
                uint32_t end = start + HELP_COMMANDS_PER_PAGE;

                if (end > sbin_count)
                {
                    end = sbin_count;
                }

                for (uint32_t i = start; i < end; i++)
                {
                    if (sbin_names[i][0] == '.')
                    {
                        continue;
                    }

                    // Format: " -<name>" padded to 37 chars, then "- <description>"
                    char line_buf[256];
                    uint32_t pos = 0;

                    line_buf[pos++] = ' ';
                    line_buf[pos++] = '-';

                    uint32_t name_len = strlen(sbin_names[i]);
                    for (uint32_t c = 0; c < name_len; c++)
                    {
                        line_buf[pos++] = sbin_names[i][c];
                    }

                    while (pos < 37)
                    {
                        line_buf[pos++] = ' ';
                    }

                    line_buf[pos++] = '-';
                    line_buf[pos++] = ' ';

                    uint32_t desc_len = strlen(sbin_descs[i]);
                    for (uint32_t c = 0; c < desc_len && pos < sizeof(line_buf) - 2; c++)
                    {
                        line_buf[pos++] = sbin_descs[i][c];
                    }

                    line_buf[pos++] = '\n';
                    line_buf[pos] = '\0';

                    Gpu::print(line_buf);
                }
            }
        }
    }

    // 2. Command: clear
    else if (cmd_name_len == 5 && Memory::memcmp(cmd, "clear", 5) == 0)
    {
        if (active_terminal_redirect)
        {
            terminal_clear_output();
        }
        else
        {
            Gpu::init_text_buffer();
        }
        return;
    }

    // 3. Command: echo
    else if (cmd_name_len == 4 && Memory::memcmp(cmd, "echo", 4) == 0)
    {
        const char* text_flag = strstr(args, "--text ");
        const char* color_flag = strstr(args, "--color ");
        const char* file_flag = strstr(args, "--file ");

        if (text_flag)
        {
            const char* text_ptr = text_flag + 7;
            char text_buf[256];
            int i = 0;

            if (*text_ptr == '"')
            {
                text_ptr++;
                while (*text_ptr && *text_ptr != '"' && i < 255)
                {
                    text_buf[i++] = *text_ptr++;
                }
            }
            else
            {
                while (*text_ptr && *text_ptr != ' ' && i < 255)
                {
                    text_buf[i++] = *text_ptr++;
                }
            }
            text_buf[i] = '\0';

            uint32_t old_color = Gpu::current_text_color;

            if (color_flag)
            {
                const char* color_ptr = color_flag + 8;
                Gpu::current_text_color = parse_hex_color(color_ptr);
            }

            Gpu::print(text_buf);

            if (file_flag)
            {
                const char* file_ptr = file_flag + 7;
                char file_name[64];
                int j = 0;

                if (*file_ptr == '"')
                {
                    file_ptr++;
                    while (*file_ptr && *file_ptr != '"' && j < 63)
                    {
                        file_name[j++] = *file_ptr++;
                    }
                }
                else
                {
                    while (*file_ptr && *file_ptr != ' ' && j < 63)
                    {
                        file_name[j++] = *file_ptr++;
                    }
                }
                file_name[j] = '\0';

                // Write to file using ClawFS
                clawfs_create_file_in(current_path, file_name);
            }

            Gpu::print("\n");
            Gpu::current_text_color = old_color;
        }
        else
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage: echo --text <text> [--color [0xRRGGBB]] [--file <filename>]\n");
        }
    }

    // 4. Command: Gpu::fetch
    else if (cmd_name_len == 5 && Memory::memcmp(cmd, "fetch", 5) == 0)
    {
        Gpu::fetch();
    }

    // 5. Command: time
    else if (cmd_name_len == 4 && Memory::memcmp(cmd, "time", 4) == 0)
    {
        const char* set_flag = strstr(args, "--set ");
        const char* get_flag = strstr(args, "--get");
        const char* status_flag = strstr(args, "--info");

        if (set_flag)
        {
            // Format:
            // --set DD.MM.YYYY-HH:MM:SS
            const char* date_ptr = set_flag + 6;

            // Validate format: DD.MM.YYYY-HH:MM:SS
            if (strlen(date_ptr) < 19 || date_ptr[2] != '.' || date_ptr[5] != '.' || date_ptr[10] != '-' || date_ptr[13] != ':' || date_ptr[16] != ':')
            {
                Gpu::print_error("Syntax error!\n");
                Gpu::print_info("Usage: time --set <DD.MM.YYYY-HH:MM:SS>\n");
            }
            else
            {
                Rtc::RtcTime new_time;

                new_time.day    = parse_digits(date_ptr, 2);
                new_time.month  = parse_digits(date_ptr + 3, 2);
                new_time.year   = parse_digits(date_ptr + 6, 4);

                new_time.hour   = parse_digits(date_ptr + 11, 2);
                new_time.minute = parse_digits(date_ptr + 14, 2);
                new_time.second = parse_digits(date_ptr + 17, 2);

                Rtc::set_time(new_time);

                Gpu::print_info("System time updated successfully!\n");
            }
        }
        else if (get_flag)
        {
            Rtc::RtcTime time = Rtc::get_time();
            
            // Display date: DD.MM.YYYY
            Gpu::print_num_padded(time.day);
            Gpu::print(".");
            Gpu::print_num_padded(time.month);
            Gpu::print(".");
            Gpu::print_year(time.year);

            Gpu::print(" ");

            // Display time: HH:MM:SS
            Gpu::print_num_padded(time.hour);
            Gpu::print(":");
            Gpu::print_num_padded(time.minute);
            Gpu::print(":");
            Gpu::print_num_padded(time.second);

            Gpu::print("\n");
        }
        else if (status_flag)
        {
            bool battery_ok = Rtc::is_battery_ok();

            Gpu::print_info("RTC battery: ");
            Gpu::print(battery_ok ? "OK" : "low");
            Gpu::print("\n");
        }
        else
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage: time --get | --set <DD.MM.YYYY-HH:MM:SS> | --info\n");
        }
    }

    // 6. Command: reboot
    else if (cmd_name_len == 6 && Memory::memcmp(cmd, "reboot", 6) == 0)
    {
        Acpi::reboot();
    }

    // 7. Command: shutdown
    else if (cmd_name_len == 8 && Memory::memcmp(cmd, "shutdown", 8) == 0)
    {
        Acpi::shutdown();
    }

    // 8. Command: format
    else if(cmd_name_len == 6 && Memory::memcmp(cmd,"format", 6) == 0)
    {
        while (*args == ' ')
        {
            args++;
        }

        if (*args != '\0')
        {
            if (strcmp(args, "--commands") == 0)
            {
                Gpu::print_info("Setting up persistent command storage...\n");
                if (format_commands())
                {
                    Gpu::print_info("Commands copied successfully to ClawFS.\n");
                    Gpu::print_info("Run 'mount' to enable the overlay.\n");
                }
                else
                {
                    Gpu::print_error("Failed to format commands.\n");
                }
            }
            else if (strcmp(args, "--clear") == 0)
            {
                // Clear option
                Gpu::print_info("Clearing ClawFS...\n");
                clawfs_format_clr();
                Gpu::print_info("Done.\n");
            }
            else
            {
                Gpu::print_error("Syntax error!\n");
                Gpu::print_info("Usage: format [--commands | --clear]\n");
            }
        }
        else
        {
            Gpu::print_warn("Formatting CLAWFS...\n");
            clawfs_format();
            Gpu::print_info("Done.\n");
            Gpu::print_info("Run 'format --commands' to copy system commands.\n");
        }
    }

    // 9. Command: mount
    else if(cmd_name_len == 5 && Memory::memcmp(cmd, "mount", 5) == 0)
    {
        if (clawfs_exists())
        {
            file_resolver_mount(true);
        }
        else
        {
            Gpu::print_error("Disk is not formatted as CLAWFS.\n");
            Gpu::print_info("Run 'format' first to create the filesystem.\n");
        }
    }

    // 10. Command: unmount
    else if(cmd_name_len == 7 && Memory::memcmp(cmd, "unmount", 7) == 0)
    {
        file_resolver_mount(false);
    }

    // 11. Command: touch
    else if (cmd_name_len == 5 && Memory::memcmp(cmd, "touch", 5) == 0) 
    {
        const char* filename_flag = strstr(args, "--file ");

        if (filename_flag) 
        {
            const char* filename_ptr = filename_flag + 7;
            char name_buf[64]; // Bufor for file name
            int i = 0;

            if (*filename_ptr == '"') 
            {
                filename_ptr++;

                while (*filename_ptr && *filename_ptr != '"' && i < 63) 
                {
                    name_buf[i++] = *filename_ptr++;
                }
            }
            else 
            {
                while (*filename_ptr && *filename_ptr != ' ' && i < 63) 
                {
                    name_buf[i++] = *filename_ptr++;
                }
            }
            name_buf[i] = '\0';

            clawfs_create_file_in(current_path, name_buf);
        }
        else 
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage: touch --file <filename>\n");
        }
    }

    // 12. Command: info
    else if (cmd_name_len == 4 && Memory::memcmp(cmd, "info", 4) == 0)
    {
        char cpu_name[48];
        Cpu::get_brand(cpu_name);

        auto print_line = [](const char* label, const char* value)
        {
            Gpu::print(CMD_TEXT_GRAY);
            Gpu::print(label);
            Gpu::print(CMD_TEXT_WHITE);
            Gpu::print(value);
            Gpu::print("\n");
        };

        // Section: Software
        Gpu::print_info("Software information\n");
        print_line("System Version: ", "NasuaOS 0.8.0");
        print_line("Kernel Version: ", "0.3.0\n\n");

        // Section: Hardware
        Gpu::print_info("Hardware information\n");

        print_line("CPU:            ", cpu_name);
        Gpu::print(CMD_TEXT_GRAY);
        Gpu::print("Total RAM:      ");
        Gpu::print(CMD_TEXT_WHITE);
        Gpu::print(Memory::total());
        Gpu::print("MB\n");
        Gpu::print(CMD_TEXT_GRAY);
        Gpu::print("Used RAM:       ");
        Gpu::print(CMD_TEXT_WHITE);
        Gpu::print_num8(Memory::used() / (1024 * 1024));
        Gpu::print("MB\n");
        Gpu::print("\n");

        Gpu::print_info("Storage information\n");
        if (storage_uses_ata())
        {
            print_line("Storage type:   ", "ATA Disk");
        }
        else
        {
            print_line("Storage type:   ", "RAM Disk");
        }

        Gpu::print(CMD_TEXT_GRAY);
        Gpu::print("Storage Total:  ");
        Gpu::print(CMD_TEXT_WHITE);
        Gpu::print_num8(Disk::total() / (1024 * 1024));
        Gpu::print("MB\n");
        Gpu::print(CMD_TEXT_GRAY);
        Gpu::print("Storage Used:   ");
        Gpu::print(CMD_TEXT_WHITE);
        Gpu::print_num8(Disk::used() / (1024 * 1024));
        Gpu::print("MB\n");
        Gpu::print("\n");

        Gpu::print_info("ClawFS status\n");
        if (file_resolver_is_mounted())
        {
            print_line("Overlay:        ", "Mounted (commands from disk)");
        }
        else
        {
            print_line("Overlay:        ", "Unmounted (commands from ISO)");
        }
    }

    // 13. Command: source
    else if (cmd_name_len == 6 && Memory::memcmp(cmd, "source", 6) == 0)
    {
        Gpu::print_info("NasuaOS Source Code: https://github.com/szatakis/NasuaOS\n");
    }

    // 14. Command: debug
    else if (cmd_name_len == 5 && Memory::memcmp(cmd, "debug", 5) == 0)
    {
        const char* on_flag = strstr(args, "--on");
        const char* off_flag = strstr(args, "--off");

        if (on_flag)
        {
            debug_mode = true;
            Gpu::print_info("Debug mode enabled.\n");
        }
        else if (off_flag)
        {
            debug_mode = false;
            Gpu::print_info("Debug mode disabled.\n");
        }
        else
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage: debug --on | --off\n");
        }
    }

    // 15. Command: uptime
    else if (cmd_name_len == 6 && Memory::memcmp(cmd, "uptime", 6) == 0)
    {
        Rtc::print_uptime();
    }

    // 16. Command: panic
    else if (cmd_name_len == 5 && Memory::memcmp(cmd, "panic", 5) == 0)
    {
        kernel_panic("User-triggered panic for debugging");
    }

    // 17. Command: resolution
    else if (cmd_name_len == 10 && Memory::memcmp(cmd, "resolution", 10) == 0)
    {
        Gpu::print_resolution();
    }

    // 18. Command: logs
    else if (cmd_name_len == 4 && Memory::memcmp(cmd, "logs", 4) == 0)
    {
        const char* show_flag = strstr(args, "--show");
        const char* clear_flag = strstr(args, "--clear");
        const char* level_flag = strstr(args, "--level ");
        const char* subsystem_flag = strstr(args, "--subsystem ");
        const char* put_flag = strstr(args, "--put ");

        if (show_flag)
        {
            print_logs();
        }
        else if (clear_flag)
        {
            clear_logs();
            Gpu::print_info("Log buffer cleared.\n");
        }
        else if (level_flag)
        {
            const char* level_arg = level_flag + 8;
            LogLevel level = parse_log_level(level_arg);
            print_logs_level(level);
        }
        else if (subsystem_flag)
        {
            const char* subsystem_arg = subsystem_flag + 12;
            print_logs_subsystem(subsystem_arg);
        }
        else if (put_flag)
        {
            const char* text_arg = put_flag + 6;
            char text[256];
            int i = 0;

            if (*text_arg == '"')
            {
                text_arg++;
                while (*text_arg && *text_arg != '"' && i < 255)
                {
                    text[i++] = *text_arg++;
                }
            }
            else
            {
                while (*text_arg && *text_arg != ' ' && i < 255)
                {
                    text[i++] = *text_arg++;
                }
            }
            text[i] = '\0';

            log(INFO, "USER", text);
            Gpu::print_info("Log added successfully\n");
        }
        else
        {
            Gpu::print_error("Invalid --put format! Use: logs --put <your message>\n");
        }
    }

    // 19. Command: bootapp
    else if (cmd_name_len == 7 && Memory::memcmp(cmd, "bootapp", 7) == 0)
    {
        const char* app_flag = strstr(args, "--app ");
        const char* list_flag = strstr(args, "--list");

        if (app_flag)
        {
            const char* app_ptr = app_flag + 6;
            char app_name_buf[64];
            int i = 0;

            if (*app_ptr == '"')
            {
                app_ptr++;
                while (*app_ptr && *app_ptr != '"' && i < 63)
                {
                    app_name_buf[i++] = *app_ptr++;
                }
            }
            else
            {
                while (*app_ptr && *app_ptr != ' ' && i < 63)
                {
                    app_name_buf[i++] = *app_ptr++;
                }
            }
            app_name_buf[i] = '\0';

            // Check for built-in kernel applications first
            bool app_launched = false;

            if (strcmp(app_name_buf, "settings") == 0)
            {
                Gpu::Window_Manager::register_window(&settings);
                app_launched = true;
            }
            else if (strcmp(app_name_buf, "terminal") == 0)
            {
                Gpu::Window_Manager::register_window(&terminal);
                app_launched = true;
            }
            else if (strcmp(app_name_buf, "suaedit") == 0)
            {
                Gpu::Window_Manager::register_window(&suaedit);
                app_launched = true;
            }
            else if (strcmp(app_name_buf, "task_manager") == 0)
            {
                Gpu::Window_Manager::register_window(&task_manager);
                app_launched = true;
            }
            
            if (app_launched)
            {
                Gpu::print_info("Built-in application launched: ");
                Gpu::print(app_name_buf);
                Gpu::print("\n");
            }
            else
            {
                // Fall back to /bin applications
                int exit_code = 0;
                if (napp_run(app_name_buf, &exit_code))
                {
                    Gpu::print_info("Application exited with code: ");
                    Gpu::print_num8(exit_code);
                    Gpu::print("\n");
                }
                else
                {
                    Gpu::print_error("Failed to run application\n");
                }
            }
        }
        else if (list_flag)
        {
            Gpu::print_info("Built-in applications:\n");
            Gpu::print(" - settings\n");
            Gpu::print(" - terminal\n");
            Gpu::print(" - suaedit\n");
            Gpu::print(" - task_manager\n");
            
            Gpu::print_info("/bin applications:\n");
            static char app_names[NAPP_MAX_APPLICATIONS][NAPP_MAX_NAME];
            uint32_t count = napp_list(app_names, NAPP_MAX_APPLICATIONS);

            for (uint32_t i = 0; i < count; i++)
            {
                Gpu::print(" - ");
                Gpu::print(app_names[i]);
                Gpu::print("\n");
            }
        }
        else
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage:\n");
            Gpu::print("  bootapp --list\n");
            Gpu::print("  bootapp --app <app_name>\n");
        }
    }

    // 20. Command: beep
    else if (cmd_name_len == 4 && Memory::memcmp(cmd, "beep", 4) == 0) 
    {
        int freq = 1000; // Default freqency in Hz
        int dur = 200;   // Default time in ms
        
        const char* freq_flag = strstr(args, "--freq ");
        const char* dur_flag = strstr(args, "--dur ");
        
        if (freq_flag) 
        {
            const char* freq_arg = freq_flag + 7;
            char freq_buf[16];
            int i = 0;
            
            if (freq_arg[0] == '"') 
            {
                freq_arg++;
                while (freq_arg[i] != '"' && freq_arg[i] != '\0' && i < 15) 
                {
                    freq_buf[i] = freq_arg[i];
                    i++;
                }
            }
            else 
            {
                while (freq_arg[i] != ' ' && freq_arg[i] != '\0' && i < 15) 
                {
                    freq_buf[i] = freq_arg[i];
                    i++;
                }
            }
            freq_buf[i] = '\0';
            
            if (strlen(freq_buf) > 0) 
            {
                freq = atoi(freq_buf);
            }
        }
        if (dur_flag) 
        {
            const char* dur_arg = dur_flag + 6;
            char dur_buf[16];
            int i = 0;
            
            if (dur_arg[0] == '"') 
            {
                dur_arg++;
                while (dur_arg[i] != '"' && dur_arg[i] != '\0' && i < 15) 
                {
                    dur_buf[i] = dur_arg[i];
                    i++;
                }
            }
            else 
            {
                while (dur_arg[i] != ' ' && dur_arg[i] != '\0' && i < 15) 
                {
                    dur_buf[i] = dur_arg[i];
                    i++;
                }
            }
            dur_buf[i] = '\0';
            
            if (strlen(dur_buf) > 0) 
            {
                dur = atoi(dur_buf);
            }
        }
        
        if (freq > 0 && dur > 0) 
        {
            Gpu::print_info("Beeping... Freq: "); 
            Gpu::print_num8(freq);
            Gpu::print(" Hz, Duration: ");
            Gpu::print_num8(dur);
            Gpu::print(" ms\n");

            Timer::sleep(10);

            Audio::beep(freq, dur);
        }
        else 
        {
            Gpu::print_error("Invalid arguments!\n");
            Gpu::print_info("Usage:\n");
            Gpu::print("  beep\n");
            Gpu::print("  beep --freq [frequency] --dur [time]\n");
        }
    }

    // 21. Command: calc
    else if (cmd_name_len == 4 && Memory::memcmp(cmd, "calc", 4) == 0) 
    {
        const char* op_flag  = strstr(args, "--op ");
        const char* num1_flag = strstr(args, "--num1 ");
        const char* num2_flag = strstr(args, "--num2 ");

        if (op_flag && num1_flag && num2_flag) 
        {
            char op_buf[8] = {0};
            char num1_buf[16] = {0};
            char num2_buf[16] = {0};
            int i = 0;

            // --op
            const char* op_arg = op_flag + 5;
            i = 0;
            if (op_arg[0] == '"') 
            {
                op_arg++;
                while (op_arg[i] != '"' && op_arg[i] != '\0' && i < 7) { op_buf[i] = op_arg[i]; i++; }
            }
            else 
            {
                while (op_arg[i] != ' ' && op_arg[i] != '\0' && i < 7) { op_buf[i] = op_arg[i]; i++; }
            }
            op_buf[i] = '\0';

            // --num1
            const char* num1_arg = num1_flag + 7;
            i = 0;
            if (num1_arg[0] == '"') 
            {
                num1_arg++;
                while (num1_arg[i] != '"' && num1_arg[i] != '\0' && i < 15) 
                { 
                    num1_buf[i] = num1_arg[i]; 
                    i++; 
                }
            }
            else 
            {
                while (num1_arg[i] != ' ' && num1_arg[i] != '\0' && i < 15) 
                { 
                    num1_buf[i] = num1_arg[i];
                    i++; 
                }
            }
            num1_buf[i] = '\0';

            // --num2
            const char* num2_arg = num2_flag + 7;
            i = 0;
            if (num2_arg[0] == '"') 
            {
                num2_arg++;
                while (num2_arg[i] != '"' && num2_arg[i] != '\0' && i < 15) 
                { 
                    num2_buf[i] = num2_arg[i]; 
                    i++; 
                }
            }
            else 
            {
                while (num2_arg[i] != ' ' && num2_arg[i] != '\0' && i < 15) 
                { 
                    num2_buf[i] = num2_arg[i]; 
                    i++; 
                }
            }

            num2_buf[i] = '\0';

            int n1 = atoi(num1_buf);
            int n2 = atoi(num2_buf);
            int result = 0;

            // Calculations
            if (strcmp(op_buf, "add") == 0) 
            {
                result = n1 + n2;
                Gpu::print_info("Result: ");
                Gpu::print_num8(result);
                Gpu::print("\n");
            }
            else if (strcmp(op_buf, "sub") == 0) 
            {
                result = n1 - n2;
                Gpu::print_info("Result: ");
                Gpu::print_num8(result);
                Gpu::print("\n");
            }
            else if (strcmp(op_buf, "mul") == 0) 
            {
                result = n1 * n2;
                Gpu::print_info("Result: ");
                Gpu::print_num8(result);
                Gpu::print("\n");
            }
            else if (strcmp(op_buf, "div") == 0) 
            {
                if (n2 == 0) 
                {
                    Gpu::print_error("Division by zero error!\n");
                }
                else 
                {
                    result = n1 / n2;
                    Gpu::print_info("Result: ");
                    Gpu::print_num8(result);
                    Gpu::print("\n");
                }
            }
            else 
            {
                Gpu::print_error("Unknown operation! Use: add, sub, mul, div\n");
            }
        }
        else 
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage: calc --op <add|sub|mul|div> --num1 [val] --num2 [val]\n");
        }
    }

    // 22. Command: rand
    else if (cmd_name_len == 4 && Memory::memcmp(cmd, "rand", 4) == 0) 
    {
        const char* min_flag = strstr(args, "--min ");
        const char* max_flag = strstr(args, "--max ");

        int min_val = 0;
        int max_val = 0;
        int has_max = 0;

        if (min_flag) 
        {
            const char* min_arg = min_flag + 6;
            char min_buf[16] = {0};
            int i = 0;

            if (min_arg[0] == '"') 
            {
                min_arg++;

                while (min_arg[i] != '"' && min_arg[i] != '\0' && i < 15) 
                {
                    min_buf[i] = min_arg[i];
                    i++;
                }
            }
            else 
            {
                while (min_arg[i] != ' ' && min_arg[i] != '\0' && i < 15) 
                {
                    min_buf[i] = min_arg[i];
                    i++;
                }
            }
            min_buf[i] = '\0';
            min_val = atoi(min_buf);
        }

        if (max_flag) 
        {
            const char* max_arg = max_flag + 6;
            char max_buf[16] = {0};
            int i = 0;

            if (max_arg[0] == '"') 
            {
                max_arg++;

                while (max_arg[i] != '"' && max_arg[i] != '\0' && i < 15) 
                {
                    max_buf[i] = max_arg[i];
                    i++;
                }
            }
            else 
            {
                while (max_arg[i] != ' ' && max_arg[i] != '\0' && i < 15) 
                {
                    max_buf[i] = max_arg[i];
                    i++;
                }
            }
            max_buf[i] = '\0';
            max_val = atoi(max_buf);
            has_max = 1;
        }

        if (has_max) 
        {
            if (min_val > max_val) 
            {
                Gpu::print_error("Invalid range! Min cannot be greater than Max.\n");
            }
            else 
            {
                int64_t range = (int64_t)max_val - (int64_t)min_val + 1;
                
                int random_num = min_val + ((system_rand() & 0x7fffffff) % range);
                
                Gpu::print_info("Random number: ");
                Gpu::print_num8(random_num);
                Gpu::print("\n");
            }
        }
        else 
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage: rand --min [val] --max [val]\n");
        }
    }

    // 23. Command: inb
    else if (cmd_name_len == 3 && Memory::memcmp(cmd, "inb", 3) == 0)
    {
        const char* port_flag = strstr(args, "--port ");

        if (!port_flag)
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage: inb --port [0xHEX]\n");
        }
        else {
            const char* port_str = port_flag + 7;

            uint16_t port = (uint16_t)parse_hex(port_str);
            uint8_t value = inb(port);

            Gpu::print("Port ");
            Gpu::print_hex(port);
            Gpu::print(" = ");
            Gpu::print_hex(value);
            Gpu::print("\n");
        }
    }

    // 24. Command: outb
    else if (cmd_name_len == 4 && Memory::memcmp(cmd, "outb", 4) == 0)
    {
        const char* port_flag = strstr(args, "--port ");
        const char* val_flag  = strstr(args, "--val ");

        if (!port_flag || !val_flag)
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage: outb --port [0xHEX] --val [0xHEX]\n");
        }
        else
        {
            uint16_t port = (uint16_t)parse_hex(port_flag + 7);
            uint8_t value = (uint8_t)parse_hex(val_flag + 6);

            outb(port, value);

            Gpu::print("Written ");
            Gpu::print_hex(value);
            Gpu::print(" -> ");
            Gpu::print_hex(port);
            Gpu::print("\n");
        }
    }

    // 25. Command: asciiart
    else if(cmd_name_len == 8 && Memory::memcmp(cmd, "asciiart", 8) == 0)
    {
        const char* text_flag = strstr(args, "--text ");

        if(!text_flag)
        {
            Gpu::print("Usage: asciiart --text <text>\n");
        }
        else
        {
            text_flag += 7;


            if(text_flag[0] == '"')
            {
                text_flag++;
            }


            char buffer[128];

            int i = 0;

            while(text_flag[i] && text_flag[i] != '"' && i < 127)
            {
                buffer[i] = text_flag[i];
                i++;
            }

            buffer[i] = 0;


            print_ascii_art(buffer);
        }
    }

    // 26. Command: safe_mode
    else if(cmd_name_len == 9 && Memory::memcmp(cmd, "safe_mode", 9) == 0)
    {
        Gpu::print_info("Safe mode ON\n");
        safe_mode = true;
    }

    // 27. Command: uart
    else if (cmd_name_len == 4 && Memory::memcmp(cmd, "uart", 4) == 0)
    {
        const char* text_flag = strstr(args, "--text ");

        if (!text_flag)
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage: uart --text <text>\n");
        }
        else
        {
            const char* text_ptr = text_flag + 7;
            char text_buf[256];
            int i = 0;

            if (*text_ptr == '"')
            {
                text_ptr++;

                while (*text_ptr && *text_ptr != '"' && i < 255)
                {
                    text_buf[i++] = *text_ptr++;
                }
            }
            else
            {
                while (*text_ptr && *text_ptr != ' ' && i < 255)
                {
                    text_buf[i++] = *text_ptr++;
                }
            }

            text_buf[i] = '\0';

            Uart::puts(text_buf);
            Uart::puts("\r\n");

            Gpu::print_info("Text sent over UART.\n");
        }
    }



    // Dynamic /sbin commands
    else 
    {
        char cmd_name[64];
        size_t i = 0;

        while (i < cmd_name_len && i < 63)
        {
            cmd_name[i] = cmd[i];
            i++;
        }

        cmd_name[i] = '\0';

        char sbin_path[256];
        strcpy(sbin_path, "/sbin/");
        strcat(sbin_path, cmd_name);

        if (system_file_exists(sbin_path))
        {
            napp_set_current_path(current_path);

            const char* argv[17];
            argv[0] = cmd_name;
            int argc = build_argv(args, argv + 1, 16) + 1;

            int exit_code = 0;
            if (napp_run_path(sbin_path, argc, argv, &exit_code))
            {
                strcpy(current_path, napp_get_current_path());
                Gpu::print_cmd();
                return;
            }

            strcpy(current_path, napp_get_current_path());
        }

        Gpu::print_error("Unknown command: ");
        
        for (size_t i = 0; i < cmd_name_len; i++) 
        {
            single_char_buf[0] = cmd[i];
            Gpu::print(single_char_buf);
        }

        Gpu::print("\n");
    }
    
    Gpu::print_cmd();
}
