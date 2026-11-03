#include <cstddef>
#include <cstdint>
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#include "shell.hpp"

#include "system/drivers/drivers.hpp"

#include "kernel/kernel_panic/kernel_panic.hpp"
#include "system/sysfunc/logger/logger.hpp"
#include "system/sysfunc/rand/rand.hpp"

#include "applications/applications.hpp"
#include "drivers/gpu/driver.hpp"
#include "functions/functions.hpp"

#include "system/vars/info_vars/info_vars.hpp"

#include "system/filesystem/clawfs/clawfs.hpp"
#include "system/filesystem/file_resolver/file_resolver.hpp"
#include "system/authentication/authentication.hpp"

#include "system/applications/napp/napp.hpp"

#include "libs/libc/libc.hpp"
#include "libs/asm/asm.hpp"

extern uint32_t Gpu::current_text_color;
extern bool debug_mode;
extern bool safe_mode;

static constexpr uint32_t HELP_COMMANDS_PER_PAGE = 13;

struct HelpCommand
{
    const char* name;
    const char* description;
    const char* details;
};

static const HelpCommand HELP_COMMANDS[] =
{
    {"help", "Show the first page of help", "   --page <number>                  - Show a specific help page\n"},
    {"info", "Display OS version and hardware info", ""},
    {"source", "Show link to the OS source code", ""},
    {"shutdown", "Power off the system safely", ""},
    {"reboot", "Restart the computer", ""},
    {"echo", "Print text to the screen or write to file",
     "   --text <text>                     - (Required) Specify the text to Gpu::print\n"
     "   --color [0xRRGGBB]                - (Optional) Set custom HEX text color\n"
     "   --file <file_name.file_ext>       - (Optional) Save the text output into a file\n"},
    {"asciiart", "Convert text into large ASCII banner",
     "   --text <string>                  - (Required) Text to transform\n"},
    {"rand", "Generate a random number within a range",
     "   --min [value]                     - (Required) Minimum value of the range\n"
     "   --max [value]                     - (Required) Maximum value of the range\n"},
    {"touch", "Create a new empty file",
     "   --file <file_name.file_ext>       - (Required) Name of the file to create\n"},
    {"mount", "Mount ClawFS overlay for command override", ""},
    {"unmount", "Unmount ClawFS overlay", ""},
    {"time", "System clock utility",
     "   --get                             - (Required 1 of 3) Display current date and time\n"
     "   --set <DD.MM.YYYY-HH:MM:SS>       - (Required 1 of 3) Set new system date and time\n"
     "   --info                            - (Required 1 of 3) Display CMOS storage and RTC battery status\n"},
    {"uptime", "Display system uptime since boot", ""},
    {"panic", "Trigger kernel panic for debugging", ""},
    {"resolution", "Display current screen resolution and video mode information", ""},
    {"logs", "Kernel log management utility",
     "   --show                           - (Required 1 of 5) Display stored kernel logs from memory\n"
     "   --clear                          - (Required 1 of 5) Clear kernel log buffer\n"
     "   --level <INFO|WARN|ERROR|DEBUG>  - (Required 1 of 5) Display only selected level messages\n"
     "   --subsystem <subsystem_name>     - (Required 1 of 5) Display logs from selected subsystem\n"
     "   --put <text>                     - (Required 1 of 5) Add custom log message with INFO level\n"},
    {"bootapp", "Application manager command",
     "   --app <application_name>         - (Required) Load and execute selected application\n"
     "   --list                           - List all available applications\n"},
    {"clear", "Clear the terminal screen", ""},
    {"uart", "Send data over serial port",
     "   --text <text>                     - (Required) Text string to transmit via UART\n"},
    {"format", "Format storage drive",
     "   --commands                        - Copy system commands from ISO to ClawFS\n"
     "   --clear                           - Completely wipe the disk without creating the base file structure\n"
     "   --distclear                       - Completely wipe ClawFS and remove all filesystem data\n"},
    {"outb", "Write a byte to an I/O port",
     "   --port [0xHEX]                   - (Required) Specify I/O port address\n"
     "   --val [0xHEX]                    - (Required) Value byte to write\n"},
    {"fetch", "Display system summary and ASCII logo", ""},
    {"debug", "Enable or disable debug mode",
     "   --on                              - (Required 1 of 2) Enable debug mode\n"
     "   --off                             - (Required 1 of 2) Disable debug mode\n"},
    {"beep", "Play a sound through the PC speaker",
     "   --freq [frequency]                - (Optional) Set beep frequency in Hz\n"
     "   --dur [time]                      - (Optional) Set beep duration in milliseconds\n"},
    {"calc", "Perform a basic arithmetic calculation",
     "   --op <add|sub|mul|div>            - (Required) Select the arithmetic operation\n"
     "   --num1 [value]                    - (Required) First number\n"
     "   --num2 [value]                    - (Required) Second number\n"},
    {"safe_mode", "Enable safe mode for system debugging", ""},
    {"inb", "Read a byte from an I/O port",
     "   --port [0xHEX]                   - (Required) Specify I/O port address\n"},
    {"ip", "Display the current IP address", ""},
    {"ping", "Send a ping request to an IP address or hostname", 
     "   --target <address>                - (Required) Specify target IP address or hostname\n"},
    {"users", "List available user accounts", ""},
    {"useradd", "Create a user account",
     "   <username>                        - Create account, then enter its password securely\n"},
    {"userdel", "Delete a user account",
     "   <username>                        - Delete a non-root account\n"},
    {"usermod", "Rename a user account",
     "   <old-name> <new-name>             - Change an account name\n"},
    {"passwd", "Change an account password",
     "   [username]                        - Enter and confirm a new password securely\n"},
    {"su", "Switch to another user",
     "   <username>                        - Authenticate as the selected account\n"},
    {"login", "Enable, disable, or open the login screen",
     "   --on | --off                      - Require or skip login on the next boot\n"},
};

static constexpr uint32_t HELP_COMMAND_COUNT = sizeof(HELP_COMMANDS) / sizeof(HELP_COMMANDS[0]);

static void print_help_command(const char* name, const char* description, const char* details)
{
    Gpu::print(" -");
    Gpu::print(name);

    size_t position = strlen(name) + 2;
    while (position < 37)
    {
        Gpu::print(" ");
        position++;
    }

    Gpu::print("- ");
    Gpu::print(description);
    Gpu::print("\n");
    Gpu::print(details);
}

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
            if (buf[i] == '\0') 
            {
                break;
            }
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
    char command_name[64];
    if (cmd_name_len >= sizeof(command_name))
    {
        Gpu::print_error("Command name is too long.\n");
        Gpu::print_cmd();
        return;
    }
    for (size_t i = 0; i < cmd_name_len; ++i)
    {
        command_name[i] = cmd[i];
    }
    command_name[cmd_name_len] = '\0';
    if (auth_execute_command(command_name, args))
    {
        Gpu::print_cmd();
        return;
    }

    // Command: help
    if (cmd_name_len == 4 && Memory::memcmp(cmd, "help", 4) == 0) 
    {
        uint32_t page = 1;
        static char sbin_names[NAPP_MAX_APPLICATIONS][NAPP_MAX_NAME];
        static char sbin_descs[NAPP_MAX_APPLICATIONS][NAPP_MAX_NAME];
        static char sbin_help_texts[NAPP_MAX_APPLICATIONS][NAPP_HELP_TEXT_LENGTH];
        uint32_t sbin_count = napp_list_sbin(sbin_names, sbin_descs, sbin_help_texts, NAPP_MAX_APPLICATIONS);
        uint32_t total_commands = HELP_COMMAND_COUNT + sbin_count;
        uint32_t total_pages = (total_commands + HELP_COMMANDS_PER_PAGE - 1) / HELP_COMMANDS_PER_PAGE;
        if (total_pages == 0)
        {
            total_pages = 1;
        }

        const char* arg_check = args;
        while (*arg_check == ' ')
        {
            arg_check++;
        }

        if (*arg_check != '\0' && Memory::memcmp(arg_check, "--page ", 7) != 0)
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

        const char* page_flag = strstr(args, "--page ");
        if (page_flag)
        {
            page = parse_number(page_flag + 7);
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

        Gpu::print_info("Available commands (Page ");
        Gpu::print_num8(page);
        Gpu::print("/");
        Gpu::print_num8(total_pages);
        Gpu::print("):\n");

        uint32_t start = (page - 1) * HELP_COMMANDS_PER_PAGE;
        uint32_t end = start + HELP_COMMANDS_PER_PAGE;
        if (end > total_commands)
        {
            end = total_commands;
        }

        for (uint32_t i = start; i < end; i++)
        {
            if (i < HELP_COMMAND_COUNT)
            {
                const HelpCommand& command = HELP_COMMANDS[i];
                print_help_command(command.name, command.description, command.details);
            }
            else
            {
                uint32_t sbin_index = i - HELP_COMMAND_COUNT;
                print_help_command(sbin_names[sbin_index], sbin_descs[sbin_index], sbin_help_texts[sbin_index]);
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

                char file_path[256];
                uint32_t path_length = 0;
                while (current_path[path_length] != '\0' && path_length < sizeof(file_path) - 1)
                {
                    ++path_length;
                }
                const bool needs_separator = path_length > 0 && current_path[path_length - 1] != '/';
                const uint32_t name_length = static_cast<uint32_t>(strlen(file_name));
                if (path_length >= sizeof(file_path) || path_length + (needs_separator ? 1 : 0) + name_length >= sizeof(file_path))
                {
                    Gpu::print_error("File path is too long!\n");
                }
                else
                {
                    uint32_t path_index = 0;
                    while (current_path[path_index] != '\0')
                    {
                        file_path[path_index] = current_path[path_index];
                        ++path_index;
                    }
                    if (needs_separator)
                    {
                        file_path[path_index++] = '/';
                    }
                    for (uint32_t name_index = 0; name_index < name_length; ++name_index)
                    {
                        file_path[path_index++] = file_name[name_index];
                    }
                    file_path[path_index] = '\0';

                    char file_contents[256];
                    for (int content_index = 0; content_index < i; ++content_index)
                    {
                        file_contents[content_index] = text_buf[content_index];
                    }
                    file_contents[i] = '\n';
                    Gpu::print("\n");
                    if (!clawfs_write_file(file_path, file_contents, static_cast<uint32_t>(i + 1)))
                    {
                        Gpu::print_error("Could not write file!\n");
                    }
                }
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
            else if (strcmp(args, "--distclear") == 0)
            {
                // Clear option
                Gpu::print_info("Clearing Disk...\n");
                clawfs_format_dclr();
                Gpu::print_info("Done.\n");
            }
            else
            {
                Gpu::print_error("Syntax error!\n");
                Gpu::print_info("Usage: format [--commands | --clear | --distclear]\n");
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
                while (op_arg[i] != '"' && op_arg[i] != '\0' && i < 7) 
                { 
                    op_buf[i] = op_arg[i]; i++; 
                }
            }
            else 
            {
                while (op_arg[i] != ' ' && op_arg[i] != '\0' && i < 7) 
                { 
                    op_buf[i] = op_arg[i]; i++; 
                }
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
        else 
        {
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

    // 28. Command: ip
    else if (cmd_name_len == 2 && Memory::memcmp(cmd, "ip", 2) == 0)
    {
        while (*args == ' ') 
        {
            args++;
        }

        if (*args == '\0')
        {
            Network::print_info();
        }
        else
        {
            Network::IPAddress new_ip;
            // Parse IP string
            int a=0, b=0, c=0, d=0;
            const char* ptr = args;

            while (*ptr >= '0' && *ptr <= '9') 
            { 
                a = a * 10 + (*ptr - '0'); ptr++; 
            }
            if (*ptr == '.') 
            {
                ptr++;
            }
            while (*ptr >= '0' && *ptr <= '9') 
            { 
                b = b * 10 + (*ptr - '0'); ptr++; 
            }
            if (*ptr == '.') 
            {
                ptr++;
            }
            while (*ptr >= '0' && *ptr <= '9') 
            { 
                c = c * 10 + (*ptr - '0'); ptr++; 
            }
            if (*ptr == '.') 
            {
                ptr++;
            }
            while (*ptr >= '0' && *ptr <= '9') 
            { 
                d = d * 10 + (*ptr - '0'); ptr++; 
            }
            if (a >= 0 && a <= 255 && b >= 0 && b <= 255 && c >= 0 && c <= 255 && d >= 0 && d <= 255)
            {
                new_ip.bytes[0] = (uint8_t)a;
                new_ip.bytes[1] = (uint8_t)b;
                new_ip.bytes[2] = (uint8_t)c;
                new_ip.bytes[3] = (uint8_t)d;
                Network::set_ip(new_ip);
                Gpu::print_info("Updated IP address to: ");
                Gpu::print(args);
                Gpu::print("\n");
            }
            else
            {
                Gpu::print_error("Invalid IP address format!\n");
                Gpu::print_info("Usage: ip [x.x.x.x]\n");
            }
        }
    }

    // 29. Command: ping
    else if (cmd_name_len == 4 && Memory::memcmp(cmd, "ping", 4) == 0)
    {
        const char* target_flag = strstr(args, "--target ");

        if (!target_flag)
        {
            Gpu::print_error("Syntax error!\n");
            Gpu::print_info("Usage: ping --target <ip_address|hostname>\n");
        }
        else if (!Network::is_available())
        {
            Gpu::print_error("Network interface is not available!\n");
        }
        else
        {
            const char* target_arg = target_flag + 9;
            while (*target_arg == ' ')
            {
                target_arg++;
            }

            char target_name[254];
            uint16_t target_length = 0;
            while (target_arg[target_length] != '\0' && target_arg[target_length] != ' ' && target_length < sizeof(target_name) - 1)
            {
                target_name[target_length] = target_arg[target_length];
                ++target_length;
            }
            target_name[target_length] = '\0';

            Network::IPAddress target_ip = {};
            bool is_ip = true;
            const char* ptr = target_name;
            for (uint8_t octet = 0; octet < 4; ++octet)
            {
                uint16_t value = 0;
                uint8_t digits = 0;
                while (*ptr >= '0' && *ptr <= '9')
                {
                    value = static_cast<uint16_t>(value * 10 + (*ptr - '0'));
                    ++digits;
                    ++ptr;
                    if (digits > 3 || value > 255)
                    {
                        is_ip = false;
                        break;
                    }
                }
                if (digits == 0 || !is_ip)
                {
                    is_ip = false;
                    break;
                }
                target_ip.bytes[octet] = static_cast<uint8_t>(value);
                if (octet < 3)
                {
                    if (*ptr != '.')
                    {
                        is_ip = false;
                        break;
                    }
                    ++ptr;
                }
            }
            if (*ptr != '\0')
            {
                is_ip = false;
            }

            if (target_length == 0 || (target_arg[target_length] != '\0' && target_arg[target_length] != ' '))
            {
                Gpu::print_error("Target is empty or too long!\n");
                Gpu::print_info("Usage: ping --target <ip_address|hostname>\n");
            }
            else
            {
                if (!is_ip && !Network::resolve_hostname(target_name, target_ip))
                {
                    Gpu::print_error("Unable to resolve hostname: ");
                    Gpu::print(target_name);
                    Gpu::print("\n");
                }
                else
                {
                    char ip_display[16];
                    char* output = ip_display;
                    for (uint8_t octet = 0; octet < 4; ++octet)
                    {
                        itoa(target_ip.bytes[octet], output);
                        while (*output != '\0')
                        {
                            ++output;
                        }
                        if (octet < 3)
                        {
                            *output++ = '.';
                        }
                    }
                    *output = '\0';

                    Gpu::print_info("PING ");
                    Gpu::print(target_name);
                    Gpu::print(" (");
                    Gpu::print(ip_display);
                    Gpu::print(") (32 bytes of data):\n");

                    uint32_t received = 0;
                    for (int seq = 1; seq <= 4; seq++)
                    {
                        if (Network::ping(target_ip, 1000))
                        {
                            received++;
                            Gpu::print_info("32 bytes from ");
                            Gpu::print(ip_display);
                            Gpu::print(": icmp_seq=");
                            char seq_buf[16];
                            itoa(seq, seq_buf);
                            Gpu::print(seq_buf);
                            Gpu::print(" ttl=64\n");
                        }
                        else
                        {
                            Gpu::print_error("Request timeout for icmp_seq=");
                            char seq_buf[16];
                            itoa(seq, seq_buf);
                            Gpu::print(seq_buf);
                            Gpu::print("\n");
                        }
                    }

                    Gpu::print_info(target_name);
                    Gpu::print(" ping statistics\n");
                    char rec_buf[16];
                    itoa(received, rec_buf);
                    Gpu::print(rec_buf);
                    Gpu::print("/4 packets received.\n");
                }
            }
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
