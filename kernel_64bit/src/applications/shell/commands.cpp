#include <cstddef>
#include <cstdint>
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#include "commands.hpp"
#include "screen.h"

#include "system/drivers/drivers.hpp"

#include "kernel/include/panic/kernel_panic.hpp"
#include "kernel/include/logger/logger.hpp"
#include "system/sysfunc/rand/rand.hpp"

#include "applications/applications.hpp"
#include "functions/functions.hpp"

#include "system/vars/info_vars/info_vars.hpp"

#include "system/filesystem/clawfs.hpp"

#include "libs/libc/libc.h"
#include "libs/asm/asm.h"

extern uint32_t current_text_color;
extern bool debug_mode;
extern bool safe_mode;

// Main shell function

void execute_command(const char *cmd) 
{
    print("\n");
    
    if (cmd == nullptr || *cmd == '\0') 
    {
        print_cmd();
        return;
    }

    const char* space_ptr = strchr(cmd, ' ');
    size_t cmd_name_len = space_ptr ? (space_ptr - cmd) : strlen(cmd);
    const char* args = space_ptr ? (space_ptr + 1) : "";

    char single_char_buf[2] = {0, 0};

    print(CMD_TEXT_WHITE);
    // Command: help
    if (cmd_name_len == 4 && strncmp(cmd, "help", 4)) 
    {
        const char* arg_check = args;

        while (*arg_check == ' ') 
        {
            arg_check++;
        }

        if (*arg_check != '\0') 
        {
            if (!strncmp(arg_check, "--page ", 7)) 
            {
                print_error("Syntax error!\n");
                print_info("Usage: help --page [1-10]\n");
                print_cmd();
                return;
            }
        }

        int page = 1; // default page

        const char* page_flag = strstr(args, "--page ");

        if (page_flag) 
        {
            const char* page_num = page_flag + 7;

            page = parse_number(page_num);

            if (page < 1 || page > 10) 
            {
                print_error("Syntax error!\n");
                print_info("Usage: help --page [1-10]\n");
                print_cmd();
                return;
            }
        }

        if (page == 1) 
        {
            print_info("Available commands (Page 1/10):\n");
            print(" -help                               - Show the first page of help\n");
            print("   --page [1-10]                     - Show specific help page\n");
            print(" -info                               - Display OS version and hardware info\n");
            print(" -source                             - Show link to the OS source code\n");
            print(" -shutdown                           - Power off the system safely\n");
            print(" -reboot                             - Restart the computer\n");
            print(" -echo                               - Print text to the screen or write to file\n");
            print("   --text \"text\"                     - (Required) Specify the text to print\n");
            print("   --color \"0xRRGGBB\"                - (Optional) Set custom HEX text color\n");
            print("   --file \"file_name.file_ext\"       - (Optional) Save the text output into a file\n");
            print(" -debug                              - Enable or disable debug mode\n");
            print("   --on                              - (Required 1 of 2) Enable debug mode\n");
            print("   --off                             - (Required 1 of 2) Disable debug mode\n");
        } 

        else if (page == 2) 
        {
            print_info("Available commands (Page 2/10):\n");
            print(" -clear                              - Clear the terminal screen\n");
            print(" -fetch                              - Display system summary and ASCII logo\n");
            print(" -format                             - Format storage drive\n");
            print(" -dir                                - List files and directories in current path\n");
            print(" -touch                              - Create a new empty file\n");
            print("   --file \"file_name.file_ext\"       - (Required) Name of the file to create\n");
            print(" -mount                              - Mount a filesystem or device\n");
            print(" -uart                               - Send data over serial port\n");
            print("   --text \"text\"                     - (Required) Text string to transmit via UART\n");
            print(" -time                               - System clock utility\n");
            print("   --get                             - (Required 1 of 3) Display current date and time\n");
            print("   --set \"DD.MM.YYYY HH:MM:SS\"       - (Required 1 of 3) Set new system date and time\n");
            print("   --info                            - (Required 1 of 3) Display CMOS storage and RTC battery status\n");
        }

        else if (page == 3) 
        {
            print_info("Available commands (Page 3/10):\n");
            print(" -uptime                             - Display system uptime since boot\n");
            print(" -panic                              - Trigger kernel panic for debugging\n");
            print(" -resolution                         - Display current screen resolution and video mode information\n");
            print(" -logs                               - Kernel log management utility\n");
            print("    --show                           - (Required 1 of 5) Display stored kernel logs from memory\n");
            print("    --clear                          - (Required 1 of 5) Clear kernel log buffer\n");
            print("    --level \"INFO|WARN|ERROR|DEBUG\"  - (Required 1 of 5) Display only selected level messages\n");
            print("    --subsystem \"subsystem_name\"     - (Required 1 of 5) Display logs from selected subsystem\n");
            print("    --put \"text\"                     - (Required 1 of 5) Add custom log message with INFO level\n");
            print(" -bootapp                            - Application manager command\n");
            print("    --app \"application_name\"         - (Required) Load and execute selected application\n");
            print("    --list                           - List all available applications\n");
            print(" -cd \"directory\"                     - Change current working directory\n");
        }

        else if (page == 4) 
        {
            print_info("Available commands (Page 4/10):\n");
            print(" -beep                               - System PC Speaker sound generator utility\n");
            print("    --freq \"Hz\"                      - (Optional) Sound frequency in Hertz (Default: 1000 Hz)\n");
            print("    --dur \"ms\"                       - (Optional) Sound duration in milliseconds (Default: 200 ms)\n");
            print(" -calc                               - Simple system calculator utility\n");
            print("    --op \"add|sub|mul|div\"           - (Required) Selection of mathematical operation\n");
            print("    --num1 \"value\"                   - (Required) First numeric value\n");
            print("    --num2 \"value\"                   - (Required) Second numeric value\n");
            print(" -rand                               - Random number generator\n");
            print("    --min \"value\"                   - (Optional) Lower bound of the range (Default: 0)\n");
            print("    --max \"value\"                   - (Required) Upper bound of the range\n");
            print(" -rm                               - Remove file or directory\n");
            print("    --name \"name\"                   - (Required) Name of the item to remove\n");
            print("    --type \"file|dir\"               - (Required) Specify if it is a file or directory\n");
        }

        else if (page == 5) 
        {
            print_info("Available commands (Page 5/10):\n");
            print(" -inb                                - Read a byte from an I/O port\n");
            print("    --port \"0xHEX\"                   - (Required) Specify I/O port address\n");
            print(" -outb                               - Write a byte to an I/O port\n");
            print("    --port \"0xHEX\"                   - (Required) Specify I/O port address\n");
            print("    --val \"0xHEX\"                    - (Required) Value byte to write\n");
            print(" -asciiart                           - Convert text into large ASCII banner\n");
            print("    --text \"string\"                  - (Required) Text to transform\n");
            print(" -mkdir                              - Create a new directory\n");
            print("    --dir_name \"name\"                - (Required) Name of the new directory\n");
            print(" -safe_mode                          - Enable safe mode for system debugging\n");



        }

        else if (page == 6) 
        {
            print_info("Available commands (Page 6/10):\n");
            print("More commands coming soon...\n");
        }

        else if (page == 7) 
        {
            print_info("Available commands (Page 7/10):\n");
            print("More commands coming soon...\n");
        }

        else if (page == 8) 
        {
            print_info("Available commands (Page 8/10):\n");
            print("More commands coming soon...\n");
        }

        else if (page == 9) 
        {
            print_info("Available commands (Page 9/10):\n");
            print("More commands coming soon...\n");
        }

        else if (page == 10) 
        {
            print_info("Available commands (Page 10/10):\n");
            print("More commands coming soon...\n");
        }
    }
    // 2. Command: clear
    else if (cmd_name_len == 5 && strncmp(cmd, "clear", 5)) 
    {
        if (active_terminal_redirect) 
        {
            terminal_clear_output();
        } 
        else 
        {
            init_text_buffer();
        }
        return; 
    } 
    // 3. Command: echo
    else if (cmd_name_len == 4 && strncmp(cmd, "echo", 4)) 
    {
        const char* text_flag = strstr(args, "--text ");
        
        if (text_flag) 
        {
            const char* text_content = text_flag + 7;

            uint32_t old_color = current_text_color;

            const char* color_flag = strstr(args, "--color ");
            if (color_flag) 
            {
                const char* color_val = color_flag + 8;
                current_text_color = parse_hex_color(color_val);
            }

            const char* file_flag = strstr(args, "--file ");
            if (file_flag) 
            {
                const char* file_name = file_flag + 7;
    
                // Bufor for new file name
                char file_name_n[128] = {0}; 

                if (file_name[0] == '"') 
                {
                    file_name++;

                    size_t len = 0;
                    while (file_name[len] != '\0' && file_name[len] != '"') 
                    {
                        len++;
                    }

                    if (len < sizeof(file_name_n)) 
                    {
                        for (size_t i = 0; i < len; i++) 
                        {
                            file_name_n[i] = file_name[i];
                        }
                        file_name_n[len] = '\0';
                    }
                } 
                else 
                {
                    size_t len = 0;
                    while (file_name[len] != '\0' && file_name[len] != ' ') 
                    {
                        len++;
                    }

                    if (len < sizeof(file_name_n)) 
                    {
                        for (size_t i = 0; i < len; i++) 
                        {
                            file_name_n[i] = file_name[i];
                        }
                        file_name_n[len] = '\0';
                    }
                }

                clawfs_create_file_in(current_path, file_name_n);

                print_info("Saved to file ");
                print(file_name_n);
                print(" with text ");
            }

            // Printing text
            if (*text_content == '"') 
            {
                text_content++; 
                while (*text_content && *text_content != '"') 
                {
                    single_char_buf[0] = *text_content;
                    print(single_char_buf);
                    text_content++;
                }
            } 
            else 
            {
                // If arg don't have closing " do to next arg '-'
                while (*text_content && *text_content != '-') 
                {
                    single_char_buf[0] = *text_content;
                    print(single_char_buf);
                    text_content++;
                }
            }
            print("\n");

            // Return to orginal color
            current_text_color = old_color;
        } 
        else 
        {
            print_error("Syntax error!\n");
            print_info("Usage: echo --text \"your text\" --color \"0xRRGGBB\"\n");
        }
    } 
    // 4.Command: fetch
    else if (cmd_name_len == 5 && strncmp(cmd, "fetch", 5)) 
    {
        fetch();
    }
    // 5. Command: time
    else if (cmd_name_len == 4 && strncmp(cmd, "time", 4)) 
    {
        const char* set_flag = strstr(args, "--set ");
        const char* get_flag = strstr(args, "--get");
        const char* status_flag = strstr(args, "--info");

        if (set_flag) 
        {
            // format "--set ": "DD.MM.YYYY HH:MM:SS"
            const char* date_ptr = set_flag + 6;

            // Walidation to (DD.MM.YYYY HH:MM:SS)
            if (strlen(date_ptr) < 19 || date_ptr[2] != '.' || date_ptr[5] != '.' || date_ptr[10] != ' ' || date_ptr[13] != ':' || date_ptr[16] != ':') 
            {
                print_error("Syntax error!\n");
                print_info("Usage: time --set DD.MM.YYYY HH:MM:SS\n");
            } 
            else 
            {
                RtcTime new_time;
                new_time.day    = parse_digits(date_ptr, 2);
                new_time.month  = parse_digits(date_ptr + 3, 2);
                new_time.year   = parse_digits(date_ptr + 6, 4);
                new_time.hour   = parse_digits(date_ptr + 11, 2);
                new_time.minute = parse_digits(date_ptr + 14, 2);
                new_time.second = parse_digits(date_ptr + 17, 2);

                // Saving time to RTC
                set_rtc_time(new_time);
                print_info("System time updated successfully!\n");
            }
        }
        else if (get_flag) 
        {
            RtcTime time = get_rtc_time();
            
            // Display date: DD.MM.YYYY
            print_num_padded(time.day);
            print(".");
            print_num_padded(time.month);
            print(".");
            print_year(time.year);

            print(" ");

            // Display time: HH:MM:SS
            print_num_padded(time.hour);
            print(":");
            print_num_padded(time.minute);
            print(":");
            print_num_padded(time.second);
            print("\n");
        }
        else if (status_flag) 
        {
            if (!is_rtc_battery_ok()) 
            {
                print_warn("CMOS battery is dead. System time may be incorrect.\n");
            } 
            else 
            {
                print_info("CMOS battery is working properly.\n");
            }
        }
        else 
        {
            print_error("Syntax error!\n");
            print("Usage:\n");
            print("  time --get   - display current time\n");
            print("  time --set DD.MM.YYYY HH:MM:SS - set new time\n");
            print("  time --info - Checks RTC battery status\n");
        }
    }
    // 6. Command: reboot
    else if (cmd_name_len == 6 && strncmp(cmd, "reboot", 6)) 
    {
        acpi_reboot();
    }
    // 7. Command: shutdown
    else if (cmd_name_len == 8 && strncmp(cmd, "shutdown", 8)) 
    {
        acpi_shutdown();
    }
    // 8. Command: format
    else if(cmd_name_len == 6 && strncmp(cmd,"format", 6))
    {
        print_warn("Formatting CLAWFS...\n");
        clawfs_format();
        print_info("Done.\n");
    }
    // 9. Command: dir
    else if(cmd_name_len == 3 && strncmp(cmd, "dir", 3))
    {
        clawfs_dir(current_path);
    }
    // 10. Command: mount
    else if(cmd_name_len == 5 && strncmp(cmd, "mount", 5)) 
    {
        if (clawfs_exists())
        {
            print_info("CLAWFS detected.\n");
        }
        else
        {
            print_error("Disk is not formatted as CLAWFS.\n");
        }
    }
    // 11. Command: touch
    else if (cmd_name_len == 5 && strncmp(cmd, "touch", 5)) 
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
                while (*filename_ptr && *filename_ptr != ' ' && *filename_ptr != '-' && i < 63) 
                {
                    name_buf[i++] = *filename_ptr++;
                }
            }
            name_buf[i] = '\0';

            if (i > 0) 
            {
                clawfs_create_file_in(current_path, name_buf);
            } 
            else 
            {
                print_error("Error: Filename cannot be empty!\n");
            }
        }
        else 
        {
            print_error("Syntax error!\n");
            print_info("Usage: touch --file \"name.file_extension\"\n");
        }
    }
    // 12. Command: info
    else if (cmd_name_len == 4 && strncmp(cmd, "info", 4)) 
    {
        char cpu_name[49];
        char buf[32];

        auto print_line = [&](const char* label, const char* value) 
        {
            print(CMD_TEXT_WHITE);
            print(label);
            print(CMD_TEXT_GRAY);
            print(value);
            print("\n");
        };

        auto print_line_mb = [&](const char* label, const char* value) 
        {
            print(CMD_TEXT_WHITE);
            print(label);
            print(CMD_TEXT_GRAY);
            print(value);
            print("MB");
            print("\n");
        };

        auto print_numeric = [&](const char* label, uint64_t bytes) 
        {
            uint64_t mb = bytes / 1024 / 1024;
            itoa(mb, buf);
        
            print(CMD_TEXT_WHITE);
            print(label);
            print(CMD_TEXT_GRAY);
            print(buf);
            print("MB\n");
        };
    
        cpu_get_brand(cpu_name);

        // Section: Software
        print_info("Software information\n");
        print_line("System Version: ", "NasuaOS 0.07");
        print_line("Kernel Version: ", "0.03\n\n");

        print_info("Hardware information\n");

        // Section: Hardware
        print_line("CPU:            ", cpu_name);
        print_line_mb("Total RAM:      ", memory_total());
        print_numeric("Used RAM:       ", memory_used());
        print_numeric("Total Storage:  ", storage_total());
        print_numeric("Used Storage:   ", storage_used());

        // Return to orginal color
        print(CMD_TEXT_WHITE);
    }
    // 13. Command: source
    else if (cmd_name_len == 6 && strncmp(cmd, "source", 6)) 
    {
        print_info("Source anvible at: https://github.com/Szatakis/NasuaOS \n");
    }
    // 14. Command: uart
    else if (cmd_name_len == 4 && strncmp(cmd, "uart", 4)) 
    {
        const char* text_flag = strstr(args, "--text ");

        if (strlen(args) == 0 || !text_flag) 
        {
            print_error("Syntax error! Argument '--text' is required.\n");
            print_info("Usage: uart --text \"text\"\n");
        }
        else 
        {
            const char* text = text_flag + 7;

            if (*text == '"') 
            {
                text++;

                char buf[128];
                size_t i = 0;

                while (*text && *text != '"' && i < sizeof(buf) - 1) {
                    buf[i++] = *text++;
                }

                buf[i] = '\0';
                
                Uart::puts(buf);
                Uart::puts("\n");
            } 
            else 
            {
                Uart::puts(text);
                Uart::puts("\n");
            }
        }
    }
    // 15. Command: debug
    else if (cmd_name_len == 5 && strncmp(cmd, "debug", 5)) 
    {
        if (strlen(args) == 0) 
        {
            print_error("Syntax error! Argument required.\n");
            print_info("Usage: debug --on | --off\n");
        }
        else if (strncmp(args, "--on", 4)) 
        {
            debug_mode = true;

            print_info("Debug mode ON\n");
            Uart::puts("Debug ON\n");
        }
        else if (strncmp(args, "--off", 5)) 
        {
            debug_mode = false;

            print_info("Debug mode OFF\n");
            Uart::puts("Debug OFF\n");
        }
        else 
        {
            print_error("Syntax error! Unknown argument.\n");
            print_info("Usage: debug --on | --off\n");
        }
    }
    // 16. Command: resolution
    else if (cmd_name_len == 10 && strncmp(cmd, "resolution", 10)) 
    {
        print_resolution();
    }
    // 17. Command: uptime
    else if (cmd_name_len == 6 && strncmp(cmd, "uptime", 6)) 
    {
        print_uptime();
    }
    // 18. Command: panic
    else if (cmd_name_len == 5 && strncmp(cmd, "panic", 5)) 
    {
        kernel_panic("Manual panic triggered by user");
    }
    // 19. Command: logs
    else if (cmd_name_len == 4 && strncmp(cmd, "logs", 4)) 
    {
        if(strstr(args, "--show")) 
        {
            print_logs();
        }
        else if(strstr(args, "--clear")) 
        {
            clear_logs();
            print_info("Kernel log buffer cleared\n");
        }
        else if(strstr(args, "--put ")) 
        {
            const char* put_arg = strstr(args, "--put ") + 6;

            char text[128]; // Max lenght of info
            int i = 0;

            if(put_arg[0] == '"') 
            {
                put_arg++;
            }

            while(put_arg[i] != '"' && put_arg[i] != '\0' && i < 127) 
            {
                text[i] = put_arg[i];
                i++;
            }
            text[i] = '\0';

            if(strlen(text) > 0) 
            {
                log(parse_log_level("INFO"), "USER", text);
                print_info("Log added successfully\n");
            } 
            else 
            {
                print_error("Invalid --put format! Use: logs --put \"your message\"\n");
            }
        }
        else if(strstr(args, "--level ")) 
        {
            const char* level_arg = strstr(args, "--level ") + 8;

            char level_name[16];
            int i = 0;

            if(level_arg[0] == '"') 
            {
                level_arg++;

                while(level_arg[i] != '"' && level_arg[i] != '\0' && i < 15) 
                {
                    level_name[i] = level_arg[i];
                    i++;
                }
            }
            else 
            {
                while(level_arg[i] != ' ' && level_arg[i] != '\0' && i < 15) 
                {
                    level_name[i] = level_arg[i];
                    i++;
                }
            }

            level_name[i] = '\0';

            print_logs_level(parse_log_level(level_name));
        }
        else if(strstr(args, "--subsystem ")) 
        {
            const char* subsystem_arg = strstr(args, "--subsystem ") + 12;

            char subsystem[32];
            int i = 0;

            if(subsystem_arg[0] == '"') 
            {
                subsystem_arg++;

                while(subsystem_arg[i] != '"' && subsystem_arg[i] != '\0' && i < 31) 
                {
                    subsystem[i] = subsystem_arg[i];
                    i++;
                }
            }
            else 
            {
                while(subsystem_arg[i] != ' ' && subsystem_arg[i] != '\0' && i < 31) 
                {
                    subsystem[i] = subsystem_arg[i];
                    i++;
                }
            }

            subsystem[i] = '\0';

            print_logs_subsystem(subsystem);
        }
        else 
        {
            print_error("Syntax error!\n");

            print_info("Usage:\n");
            print("  logs --show\n");
            print("  logs --clear\n");
            print("  logs --level \"INFO|WARN|ERROR|DEBUG\"\n");
            print("  logs --subsystem \"name\"\n");
            print("  logs --put \"your message\"\n");
        }
    }
    // 20. Command: bootapp
    else if (cmd_name_len == 7 && strncmp(cmd, "bootapp", 7)) 
    {
        const char* app_flag = strstr(args, "--app ");
        const char* list_flag = strstr(args, "--list");

        if(list_flag) 
        {

            print_info("Available applications:\n");

            print(" - terminal\n");
            print(" - suaedit\n");
            print(" - calculator\n");
            print(" - taskmgr\n");
            print(" - settings\n");

            print("\nUsage:\n");
            print(" bootapp --app \"application_name\"\n");

        }
        else if(app_flag) 
        {
            const char* app_name = app_flag + 6;

            if (*app_name == '"') 
            {
                app_name++;

                char app_name_buf[64] = {0};

                size_t len = 0;
                while (app_name[len] != '\0' && app_name[len] != '"') 
                {
                    len++;
                }

                if (len < sizeof(app_name_buf)) 
                {
                    for (size_t i = 0; i < len; i++) 
                    {
                        app_name_buf[i] = app_name[i];
                    }

                    app_name_buf[len] = '\0';
                }

                if (strncmp(app_name_buf, "terminal", 8)) 
                {
                    terminal.visible = true;
                    terminal.id = current_id;
                    current_id++;
                    register_window(&terminal);
                }
                else if (strncmp(app_name_buf, "suaedit", 7)) 
                {
                    suaedit.visible = true;
                    suaedit.id = current_id;
                    current_id++;
                    register_window(&suaedit);
                }
                else if (strncmp(app_name_buf, "calculator", 10)) 
                {
                    calculator.visible = true;
                    calculator.id = current_id;
                    current_id++;
                    register_window(&calculator);
                }
                else if (strncmp(app_name_buf, "taskmgr", 7)) 
                {
                    task_manager.visible = true;
                    task_manager.id = current_id;
                    current_id++;
                    register_window(&task_manager);
                }
                else if (strncmp(app_name_buf, "settings", 8)) 
                {
                    settings.visible = true;
                    settings.id = current_id;
                    current_id++;
                    register_window(&settings);
                }
                else 
                {
                    print_error("Unknown app\n");
                }
            }
        }
        else 
        {
            print_error("Syntax error!\n");
            print_info("Usage:\n");
            print("  bootapp --list\n");
            print("  bootapp --app \"app_name\"\n");
        }
    }
    // 21. Command: beep
    else if (cmd_name_len == 4 && strncmp(cmd, "beep", 4)) 
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
            print_info("Beeping... Freq: "); 
            print_num8(freq);
            print(" Hz, Duration: ");
            print_num8(dur);
            print(" ms\n");

            sleep(10);

            beep(freq, dur);
        } 
        else 
        {
            print_error("Invalid arguments!\n");
            print_info("Usage:\n");
            print("  beep\n");
            print("  beep --freq \"frequency\" --dur \"time\"\n");
        }
    }
    // 22. Command: calc
    else if (cmd_name_len == 4 && strncmp(cmd, "calc", 4)) 
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
            if (strncmp(op_buf, "add", 3)) 
            {
                result = n1 + n2;
                print_info("Result: ");
                print_num8(result);
                print("\n");
            } 
            else if (strncmp(op_buf, "sub", 3)) 
            {
                result = n1 - n2;
                print_info("Result: ");
                print_num8(result);
                print("\n");
            } 
            else if (strncmp(op_buf, "mul", 3)) 
            {
                result = n1 * n2;
                print_info("Result: ");
                print_num8(result);
                print("\n");
            } 
            else if (strncmp(op_buf, "div", 3)) 
            {
                if (n2 == 0) 
                {
                    print_error("Division by zero error!\n");
                } 
                else 
                {
                    result = n1 / n2;
                    print_info("Result: ");
                    print_num8(result);
                    print("\n");
                }
            } 
            else 
            {
                print_error("Unknown operation! Use: add, sub, mul, div\n");
            }
        } 
        else 
        {
            print_error("Syntax error! Missing required arguments.\n");
            print_info("Usage: calc --op \"add|sub|mul|div\" --num1 [val] --num2 [val]\n");
        }
    }
    // 23. Command: rand
    else if (cmd_name_len == 4 && strncmp(cmd, "rand", 4)) 
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
                print_error("Invalid range! Min cannot be greater than Max.\n");
            } 
            else 
            {
                int range = max_val - min_val + 1;
                
                int random_num = min_val + ((system_rand() & 0x7fffffff) % range);
                
                print_info("Random number: ");
                print_num8(random_num);
                print("\n");
            }
        } 
        else 
        {
            print_error("Syntax error!\n");
            print_info("Usage: rand --min [val] --max [val]\n");
        }
    }
    // 24. Command: inb
    else if (cmd_name_len == 3 && strncmp(cmd, "inb", 3))
    {
        const char* port_flag = strstr(args, "--port ");

        if (!port_flag)
        {
            print_error("Syntax error!\n");
            print_info("Usage: inb --port \"0xHEX\"\n");
        } 
        else {
            const char* port_str = port_flag + 7;

            uint16_t port = (uint16_t)parse_hex(port_str);

            uint8_t value = inb(port);

            print("Port ");
            print_hex(port);
            print(" = ");
            print_hex(value);
            print("\n");
        }
    }
    // 25. Command: outb
    else if (cmd_name_len == 4 && strncmp(cmd, "outb", 4))
    {
        const char* port_flag = strstr(args, "--port ");
        const char* val_flag  = strstr(args, "--val ");

        if (!port_flag || !val_flag)
        {
            print_error("Syntax error!\n");
            print_info("Usage: outb --port \"0xHEX\" --val \"0xHEX\"\n");
        }
        else
        {
            uint16_t port = (uint16_t)parse_hex(port_flag + 7);
            uint8_t value = (uint8_t)parse_hex(val_flag + 6);

            outb(port, value);

            print("Written ");
            print_hex(value);
            print(" -> ");
            print_hex(port);
            print("\n");
        }
    }
    // 26. Command: asciiart
    else if(cmd_name_len == 8 && strncmp(cmd, "asciiart", 8))
    {
        const char* text_flag = strstr(args, "--text ");

        if(!text_flag)
        {
            print("Usage: asciiart --text \"text\"\n");
        }
        else
        {
            text_flag += 7;


            if(text_flag[0] == '"')
                text_flag++;


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
    // 27. Command: cd
    else if(cmd_name_len == 2 && strncmp(cmd, "cd", 2))
    {
        // Handling cd without arguments return to home
        if (args[0] == '\0') {
            strcpy(current_path, "/home");
        }
        // Handling "cd ~"
        else if (strcmp(args, "~") == 0) {
            strcpy(current_path, "/home");
        }
        // Handling "cd .."
        else if (strcmp(args, "..") == 0) {
            char* last_slash = strrchr(current_path, '/');
            if (last_slash != nullptr && last_slash != current_path) {
                *last_slash = '\0';
            } else {
                strcpy(current_path, "/");
            }
        }
        // Handling changing directory
        else {
            char new_path[256];
        
            // If path start with '/' handle as absolute
            if (args[0] == '/') {
                strcpy(new_path, args);
            } else {
                strcpy(new_path, current_path);
                if (strcmp(new_path, "/") != 0) strcat(new_path, "/");
                strcat(new_path, args);
            }

            // Check if path exists
            if (get_sector_by_path(new_path) != 0) {
                strcpy(current_path, new_path);
            }    
            else 
            {
                print_error("Directory not found!\n");
            }
        }
    }
    // 28. Command: mkdir
    else if(cmd_name_len == 5 && strncmp(cmd, "mkdir", 5))
    {
        const char* dirname_flag = strstr(args, "--dir_name ");

        if(dirname_flag)
        {
            const char* dirname_ptr = dirname_flag + 11;

            char name_buf[64];
            int i = 0;

            if(*dirname_ptr == '"')
            {
                dirname_ptr++;

                while(*dirname_ptr && *dirname_ptr != '"' && i < 63)
                {
                    name_buf[i++] = *dirname_ptr++;
                }
            }
            else
            {
                while(*dirname_ptr && *dirname_ptr != ' ' && *dirname_ptr != '-' && i < 63)
                {
                    name_buf[i++] = *dirname_ptr++;
                }
            }

            name_buf[i] = '\0';

            if(i > 0)
            {
                clawfs_mkdir(current_path, name_buf);
            }
            else
            {
                print_error("Error: Directory name cannot be empty!\n");
            }
        }
        else
        {
            print_error("Syntax error!\n");
            print_info("Usage: mkdir --dir_name \"folder_name\"\n");
        }
    }
    // 29. Command: rm
    else if(cmd_name_len == 2 && strncmp(cmd, "rm", 2))
    {
        const char* name_flag = strstr(args, "--name ");
        const char* type_flag = strstr(args, "--type ");

        if (name_flag && type_flag)
        {
            char name_buf[64] = {0};
            char type_buf[16] = {0};
            int i = 0;

            // Parsing --name
            const char* name_arg = name_flag + 7;
            if (*name_arg == '"') {
                name_arg++;
                while (*name_arg != '"' && *name_arg != '\0' && i < 63) name_buf[i++] = *name_arg++;
            } else {
                while (*name_arg != ' ' && *name_arg != '\0' && i < 63) name_buf[i++] = *name_arg++;
            }
            name_buf[i] = '\0';

            // Parsing --type
            const char* type_arg = type_flag + 7;
            i = 0;
            if (*type_arg == '"') {
                type_arg++;
                while (*type_arg != '"' && *type_arg != '\0' && i < 15) type_buf[i++] = *type_arg++;
            } else {
                while (*type_arg != ' ' && *type_arg != '\0' && i < 15) type_buf[i++] = *type_arg++;
            }
            type_buf[i] = '\0';

            // Chossing type
            int type = -1;
            if (strcmp(type_buf, "file") == 0) {
                type = CLAWFS_FILE;
            } else if (strcmp(type_buf, "dir") == 0) {
                type = CLAWFS_DIRECTORY;
            }

            if (type != -1 && strlen(name_buf) > 0) {
                clawfs_rm(current_path, name_buf, type);
            } else {
                print_error("Invalid type! Use: --type \"file\" or --type \"dir\"\n");
            }
        }
        else
        {
            print_error("Syntax error!\n");
            print_info("Usage: rm --name \"name\" --type \"file|dir\"\n");
        }
    }
    // 30. Command: safe_mode
    else if(cmd_name_len == 9 && strncmp(cmd, "safe_mode", 9))
    {
        print_info("Safe mode ON\n");
        safe_mode = true;
    }


    // Command: Unknown
    else 
    {
        print_error("Unknown command: ");
        for (size_t i = 0; i < cmd_name_len; i++) 
        {
            single_char_buf[0] = cmd[i];
            print(single_char_buf);
        }
        print("\n");
    }
    
    print_cmd();
}