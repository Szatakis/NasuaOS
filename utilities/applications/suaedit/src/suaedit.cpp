#include <napp.h>

#include "suascript.hpp"

NAPP_APPLICATION("suaedit", "Text editor", true);

static const napp_gui* gui = nullptr;
static const napp_api* api = nullptr;

static char editor_buffer[65536];
static char editor_filename[28];
static char filename_prompt[28];
static char explorer_files[12][28];
static int explorer_file_count = 0;
static char explorer_capture[512];
static uint16_t explorer_capture_length = 0;
static uint16_t editor_length = 0;
static uint16_t editor_cursor = 0;
static int file_prompt_mode = 0;
static bool editor_dirty = false;
static const char* editor_status = "Ready";

static void refresh_explorer();

static const char DEFAULT_INFO[] =
    "SuaEdit - Text Editor\n"
    "\n"
    "Open, edit and save text files in the current folder.\n"
    "\n"
    "New / Open / Save: File menu\n"
    "Enter: insert a new line\n"
    "Backspace: delete the character before the cursor\n"
    "Click in the editor: move the cursor\n"
    "Run or F5: compile and execute the current buffer in Terminal\n"
    "SuaScript files use .sua; types: int, bool, string\n"
    "SuaScript: print, if/else, while, for\n"
    "Enter on an empty name: cancel a filename prompt\n"
    "Terminal: show or hide the integrated shell\n"
    "File limit: 65535 characters (64 KiB)\n";

static void clear_editor_buffer()
{
    for (uint32_t index = 0; index < sizeof(editor_buffer); ++index)
    {
        editor_buffer[index] = '\0';
    }
    editor_length = 0;
    editor_cursor = 0;
    editor_dirty = false;
}

static bool is_suascript_file()
{
    uint8_t length = 0;
    while (editor_filename[length] != '\0' && length < sizeof(editor_filename) - 1)
    {
        ++length;
    }
    if (length < 4)
    {
        return false;
    }

    const char suffix[] = ".sua";
    for (uint8_t index = 0; index < 4; ++index)
    {
        char character = editor_filename[length - 4 + index];
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
        if (character != suffix[index])
        {
            return false;
        }
    }
    return true;
}

static bool load_file(const char* filename)
{
    if (!api || !api->clawfs_read_file || !filename || !*filename)
    {
        return false;
    }

    uint32_t loaded_length = 0;
    if (!api->clawfs_read_file(filename, editor_buffer, sizeof(editor_buffer) - 1, &loaded_length))
    {
        editor_status = loaded_length > sizeof(editor_buffer) - 1 ? "File exceeds editor limit" : "Could not read file";
        return false;
    }

    editor_length = static_cast<uint16_t>(loaded_length);
    editor_buffer[editor_length] = '\0';
    editor_cursor = editor_length;
    for (uint8_t index = 0; index < sizeof(editor_filename) - 1 && filename[index] != '\0'; ++index)
    {
        editor_filename[index] = filename[index];
        editor_filename[index + 1] = '\0';
    }
    editor_dirty = false;
    editor_status = "File opened";
    return true;
}

static bool save_file()
{
    if (!api || !api->clawfs_write_file || editor_filename[0] == '\0')
    {
        return false;
    }

    if (!api->clawfs_write_file(editor_filename, editor_buffer, editor_length))
    {
        editor_status = "Save failed";
        return false;
    }
    editor_dirty = false;
    editor_status = "Saved";
    return true;
}

static void begin_filename_prompt(int mode)
{
    file_prompt_mode = mode;
    filename_prompt[0] = '\0';
    editor_status = mode == 1 ? "New file name" : "Open file name";
}

static void create_new_file(const char* filename)
{
    if (!api || !api->clawfs_get_entry_type || !api->clawfs_create_file_in || !api->clawfs_resolve_path || !filename || !*filename)
    {
        editor_status = "Invalid file name";
        return;
    }
    if (!api->clawfs_get_sector || api->clawfs_get_sector("/") == 0)
    {
        editor_status = "Run format in Terminal first";
        return;
    }
    if (api->clawfs_get_sector(api->current_path) == 0)
    {
        editor_status = "Current folder not found";
        return;
    }
    for (uint8_t index = 0; filename[index] != '\0'; ++index)
    {
        if (filename[index] == '/' || filename[index] == '\\')
        {
            editor_status = "Use a file name in the current folder";
            return;
        }
    }
    if (api->clawfs_get_entry_type(api->current_path, filename) != -1)
    {
        editor_status = "File already exists";
        return;
    }

    api->clawfs_create_file_in(api->current_path, filename);
    if (api->clawfs_resolve_path(api->current_path, filename) == 0)
    {
        editor_status = "Could not create file";
        return;
    }
    clear_editor_buffer();
    uint8_t index = 0;
    while (filename[index] != '\0' && index < sizeof(editor_filename) - 1)
    {
        editor_filename[index] = filename[index];
        ++index;
    }
    editor_filename[index] = '\0';
    editor_status = "New file created";
    save_file();
    refresh_explorer();
}

static void finish_filename_prompt()
{
    const int prompt_mode = file_prompt_mode;
    file_prompt_mode = 0;
    if (filename_prompt[0] == '\0')
    {
        editor_status = "Ready";
        return;
    }
    if (filename_prompt[0] == '.' && filename_prompt[1] == '\0')
    {
        editor_status = "Invalid file name";
        return;
    }
    if (filename_prompt[0] == '.' && filename_prompt[1] == '.' && filename_prompt[2] == '\0')
    {
        editor_status = "Invalid file name";
        return;
    }
    if (prompt_mode == 1)
    {
        create_new_file(filename_prompt);
    }
    else if (!load_file(filename_prompt))
    {
        editor_status = "File not found";
    }
}

static void ensure_info_file()
{
    if (!api || !api->clawfs_get_sector || !api->clawfs_resolve_path || !api->clawfs_create_file_in || !api->clawfs_write_sector)
    {
        return;
    }
    if (api->clawfs_get_sector("/") == 0)
    {
        editor_status = "Run format in Terminal first";
        return;
    }
    if (api->clawfs_get_sector(api->current_path) == 0)
    {
        editor_status = "Current folder not found";
        return;
    }
    if (api->clawfs_resolve_path(api->current_path, "info.txt") == 0)
    {
        api->clawfs_create_file_in(api->current_path, "info.txt");
        const uint32_t sector = api->clawfs_resolve_path(api->current_path, "info.txt");
        if (sector == 0)
        {
            editor_status = "Could not create info.txt";
            return;
        }
        else
        {
            char sector_buffer[512] = {};
            uint16_t index = 0;
            while (DEFAULT_INFO[index] != '\0' && index < sizeof(sector_buffer) - 1)
            {
                sector_buffer[index] = DEFAULT_INFO[index];
                ++index;
            }
            if (!api->clawfs_write_sector(sector, sector_buffer))
            {
                editor_status = "Could not write info.txt";
                return;
            }
        }
    }
    refresh_explorer();
}

static void move_editor_cursor(int target_row, int target_column, int columns)
{
    uint16_t index = 0;
    int row = 0;
    int column = 0;
    while (index < editor_length)
    {
        if (row > target_row || (row == target_row && column >= target_column))
        {
            break;
        }
        if (editor_buffer[index] == '\n')
        {
            if (row == target_row)
            {
                break;
            }
            ++index;
            ++row;
            column = 0;
            continue;
        }
        if (column >= columns)
        {
            ++row;
            column = 0;
            continue;
        }
        ++index;
        if (++column >= columns)
        {
            ++row;
            column = 0;
        }
    }
    editor_cursor = index;
}

static void capture_explorer_char(char character)
{
    if (explorer_capture_length < sizeof(explorer_capture) - 1)
    {
        explorer_capture[explorer_capture_length++] = character;
        explorer_capture[explorer_capture_length] = '\0';
    }
}

static void refresh_explorer()
{
    explorer_file_count = 0;
    if (!api || !api->clawfs_get_sector || !api->clawfs_dir || !api->set_print_redirect ||
        api->clawfs_get_sector("/") == 0 || api->clawfs_get_sector(api->current_path) == 0)
    {
        return;
    }

    explorer_capture_length = 0;
    explorer_capture[0] = '\0';
    api->set_print_redirect(capture_explorer_char);
    api->clawfs_dir(api->current_path);
    api->set_print_redirect(nullptr);

    uint16_t line_start = 0;
    while (line_start < explorer_capture_length && explorer_file_count < 12)
    {
        uint16_t line_end = line_start;
        while (line_end < explorer_capture_length && explorer_capture[line_end] != '\n')
        {
            ++line_end;
        }
        const bool is_file = line_end - line_start >= 7 &&
                             explorer_capture[line_start] == '<' &&
                             explorer_capture[line_start + 1] == 'F' &&
                             explorer_capture[line_start + 2] == 'I';
        if (is_file)
        {
            uint16_t name_start = line_start + 7;
            uint16_t name_length = line_end - name_start;
            if (name_length > sizeof(explorer_files[0]) - 1)
            {
                name_length = sizeof(explorer_files[0]) - 1;
            }
            for (uint16_t index = 0; index < name_length; ++index)
            {
                explorer_files[explorer_file_count][index] = explorer_capture[name_start + index];
            }
            explorer_files[explorer_file_count][name_length] = '\0';
            ++explorer_file_count;
        }
        line_start = line_end + 1;
    }
}

// Forward declarations for terminal functions
static void terminal_write_char(char c);
static void terminal_write_string(const char* str);
static void run_current_script();

// Application state
static int active_activity_tab = 0; // 0: Explorer, 1: Search, 2: Source Control, 3: Run/Debug, 4: Extensions
static const int ACTIVITY_COUNT = 5;

// Terminal panel state
static bool terminal_panel_visible = false;
static bool file_menu_open = false;
static int terminal_panel_h = 200; // Height when visible
static char terminal_input[256];
static int terminal_cursor = 0;
static char terminal_output[64][256];
static int terminal_output_count = 0;
static int terminal_scroll_offset = 0;

// Color scheme
static const uint32_t COLOR_MENU_BAR        = 0xFF323233;
static const uint32_t COLOR_ACTIVITY_BAR    = 0xFF333333;
static const uint32_t COLOR_SIDE_BAR        = 0xFF252526;
static const uint32_t COLOR_EDITOR_BG       = 0xFF1E1E1E;
static const uint32_t COLOR_EDITOR_TAB      = 0xFF2D2D2D;
static const uint32_t COLOR_SEPARATOR       = 0xFF3C3C3D;
static const uint32_t COLOR_HIGHLIGHT       = 0xFF007ACC;

// Text colors
static const uint32_t COLOR_TEXT_WHITE      = 0xFFFFFFFF;
static const uint32_t COLOR_TEXT_NORMAL     = 0xFFCCCCCC;
static const uint32_t COLOR_TEXT_DIM        = 0xFF858585;


// Helper functions for drawing
static void draw_rect_outline(int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0) 
    {
        return;
    }

    gui->fill_block(x, y, color, w, 1);
    gui->fill_block(x, y + h - 1, color, w, 1);
    gui->fill_block(x, y, color, 1, h);
    gui->fill_block(x + w - 1, y, color, 1, h);
}


static void draw_suaedit(napp_window* win)
{
    if (!win || gui == nullptr) 
    {
        return;
    }

    const int window_x = win->pos_x;
    const int window_y = win->pos_y;
    const int window_w = win->width;
    const int window_h = win->height;

    if (window_w <= 0 || window_h <= 0) 
    {
        return;
    }

    const int titlebar_h = win->title_height;
    if (window_h <= titlebar_h) 
    {
        return;
    }

    const int content_x = window_x;
    const int content_y = window_y + titlebar_h;
    const int content_w = window_w;
    const int content_h = window_h - titlebar_h;

    // SECTION DIMENSIONS AND OVERFLOW GUARDS
    const int top_menu_h = 26;
    
    // Activity bar
    int activity_bar_w = 48;
    if (activity_bar_w > content_w) 
    {
        activity_bar_w = content_w; // Width safety limit
    }
    
    // Side Bar width calculation
    int side_bar_w = 180;
    if (content_w < 500) 
    {
        side_bar_w = 140;
    }
    
    // Safety check: Side bar cannot be wider than the space left after drawing the Activity Bar
    if (side_bar_w > content_w - activity_bar_w) 
    {
        side_bar_w = content_w - activity_bar_w;
    }

    if (side_bar_w < 0) 
    {
        side_bar_w = 0;
    }

    int middle_y = content_y + top_menu_h;
    int middle_h = content_h - top_menu_h;
    
    if (middle_h <= 0) 
    {
        return;
    }

    // MENU BAR (Top menu with screen overflow protection)
    gui->fill_block(content_x, content_y, COLOR_MENU_BAR, content_w, top_menu_h);
    
    const char* menu_items[] = { "File", "Edit", "Selection", "View", "Go", "Run", "Terminal", "Help" };
    int current_menu_x = content_x + 10;
    
    for (int i = 0; i < 8; i++)
    {
        // Calculate word length to determine its physical width (assuming an average of ~8px per letter)
        int len = 0;
        while(menu_items[i][len] != '\0') 
        {
            len++;
        }

        int text_width = len * 8;
        
        // Stop drawing if the text would exceed the window boundaries (content_x + content_w)
        if (current_menu_x + text_width > content_x + content_w) 
        {
            break; 
        }
        
        // Draw text
        if (i == 0 && file_menu_open)
        {
            gui->fill_block(current_menu_x - 4, content_y + 3, COLOR_EDITOR_TAB, text_width + 8, top_menu_h - 6);
        }
        gui->draw_text(menu_items[i], current_menu_x, content_y + (top_menu_h - 12) / 2, COLOR_TEXT_NORMAL);
        current_menu_x += text_width + 10;
        
        // Draw vertical line (separator), checking if it fits within the window
        if (i < 7 && current_menu_x + 2 <= content_x + content_w)
        {
            gui->fill_block(current_menu_x, content_y + 6, COLOR_SEPARATOR, 1, top_menu_h - 12);
            current_menu_x += 10; 
        }
    }

    // ACTIVITY BAR (Outlines spanning the full width of the panel)
    if (activity_bar_w > 0) 
    {
        gui->fill_block(content_x, middle_y, COLOR_ACTIVITY_BAR, activity_bar_w, middle_h);
        
        const char* activity_icons[] = { "[]", "O", "&", "|>", "::" };
        int icon_y = middle_y + 10;
        
        for (int i = 0; i < ACTIVITY_COUNT; i++) 
        {
            if (icon_y + 32 > middle_y + middle_h) 
            {
                break;
            }
            
            bool is_active = (active_activity_tab == i);
            uint32_t text_color = is_active ? COLOR_TEXT_WHITE : COLOR_TEXT_DIM;
            
            // Outline stretched to the full width of activity_bar_w
            draw_rect_outline(content_x, icon_y, activity_bar_w, 32, COLOR_SEPARATOR);

            // Active element gets an additional blue border (left marker)
            if (is_active) 
            {
                gui->fill_block(content_x, icon_y, COLOR_HIGHLIGHT, 2, 32);
            }
            
            // Draw icon, centering it within the dynamic size, provided it's wide enough
            if (activity_bar_w > 16) 
            {
                int icon_x = content_x + (activity_bar_w / 2) - 8;
                gui->draw_text(activity_icons[i], icon_x, icon_y + 10, text_color);
            }
            icon_y += 48; // Spacing between outlines
        }
    }

    // SIDE BAR (Left panel next to Activity Bar with text protection)
    if (side_bar_w > 0) 
    {
        int sidebar_x = content_x + activity_bar_w;
        gui->fill_block(sidebar_x, middle_y, COLOR_SIDE_BAR, side_bar_w, middle_h);
        
        // Protection against text overlapping edges when narrowing the window aggressively
        if (side_bar_w > 70) 
        {
            const char* tab_titles[] = { "EXPLORER", "SEARCH", "SOURCE CONTROL", "RUN/DEBUG", "EXTENSIONS" };
            gui->draw_text(tab_titles[active_activity_tab], sidebar_x + 14, middle_y + 12, COLOR_TEXT_DIM);
            
            if (active_activity_tab == 0) 
            {
                gui->draw_text("> PROJECT", sidebar_x + 10, middle_y + 40, COLOR_TEXT_WHITE);
                gui->draw_text(api && api->current_path ? api->current_path : "/", sidebar_x + 18, middle_y + 62, COLOR_TEXT_DIM);
                gui->draw_text("FILES", sidebar_x + 10, middle_y + 88, COLOR_TEXT_DIM);
                if (api && api->clawfs_get_sector && api->clawfs_get_sector("/") == 0)
                {
                    gui->draw_text("Run format first", sidebar_x + 18, middle_y + 110, COLOR_TEXT_NORMAL);
                }
                else if (explorer_file_count == 0)
                {
                    gui->draw_text("No files", sidebar_x + 18, middle_y + 110, COLOR_TEXT_DIM);
                }
                else
                {
                    for (int index = 0; index < explorer_file_count; ++index)
                    {
                        char label[21];
                        bool active_file = editor_filename[0] != '\0';
                        for (int character = 0; active_file; ++character)
                        {
                            if (editor_filename[character] != explorer_files[index][character])
                            {
                                active_file = false;
                            }
                            else if (editor_filename[character] == '\0')
                            {
                                break;
                            }
                        }
                        label[0] = active_file ? '>' : ' ';
                        label[1] = ' ';
                        int name_length = 0;
                        while (explorer_files[index][name_length] != '\0' && name_length < 14)
                        {
                            label[name_length + 2] = explorer_files[index][name_length];
                            ++name_length;
                        }
                        if (explorer_files[index][name_length] != '\0')
                        {
                            label[16] = '.';
                            label[17] = '.';
                            label[18] = '.';
                            label[19] = '\0';
                        }
                        else
                        {
                            label[name_length + 2] = '\0';
                        }
                        gui->draw_text(label, sidebar_x + 10, middle_y + 110 + index * 18, COLOR_TEXT_NORMAL);
                    }
                }
            }
        }
        
        // Vertical separator between Side Bar and Editor
        gui->fill_block(sidebar_x + side_bar_w - 1, middle_y, COLOR_SEPARATOR, 1, middle_h);
    }

    // EDITOR GROUP
    int editor_x = content_x + activity_bar_w + side_bar_w;
    int editor_w = content_w - activity_bar_w - side_bar_w;
    
     // Terminal panel dimensions
    int terminal_panel_h_local = terminal_panel_visible ? terminal_panel_h : 0;
    int editor_h = middle_h - terminal_panel_h_local;
    
    if (editor_w > 0 && editor_h > 0) 
    {
        gui->fill_block(editor_x, middle_y, COLOR_EDITOR_BG, editor_w, editor_h);
        
        const int tab_h = 35;
        if (editor_h > tab_h + 18)
        {
            gui->fill_block(editor_x, middle_y, COLOR_EDITOR_TAB, editor_w, tab_h);
            int active_tab_w = 180;
            if (active_tab_w > editor_w) 
            {
                active_tab_w = editor_w;
            }
            
            if (active_tab_w > 0) 
            {
                // Background of the active tab itself
                gui->fill_block(editor_x, middle_y, COLOR_EDITOR_BG, active_tab_w, tab_h);
                gui->fill_block(editor_x, middle_y, COLOR_HIGHLIGHT, active_tab_w, 1);
                char tab_label[40];
                int label_index = 0;
                while (editor_filename[label_index] != '\0' && label_index < 27)
                {
                    tab_label[label_index] = editor_filename[label_index];
                    ++label_index;
                }
                if (editor_dirty && label_index < 37)
                {
                    tab_label[label_index++] = '*';
                }
                tab_label[label_index] = '\0';
                gui->draw_text(tab_label, editor_x + 12, middle_y + 12, COLOR_TEXT_WHITE);
            }

            const int status_h = 18;
            const int text_x = editor_x + 14;
            const int text_y = middle_y + tab_h + 10;
            const int text_w = editor_w - 28;
            const int text_h = editor_h - tab_h - status_h - 14;
            const int char_width = 9;
            const int line_height = 16;
            int columns = text_w / char_width;
            if (columns < 1)
            {
                columns = 1;
            }
            if (columns > 240)
            {
                columns = 240;
            }
            int visible_rows = text_h / line_height;
            if (visible_rows < 1)
            {
                visible_rows = 1;
            }

            int cursor_row = 0;
            int cursor_column = 0;
            for (uint16_t index = 0; index < editor_cursor; ++index)
            {
                if (editor_buffer[index] == '\n')
                {
                    ++cursor_row;
                    cursor_column = 0;
                }
                else if (++cursor_column >= columns)
                {
                    ++cursor_row;
                    cursor_column = 0;
                }
            }
            int first_row = cursor_row - visible_rows + 1;
            if (first_row < 0)
            {
                first_row = 0;
            }

            int visual_row = 0;
            uint16_t index = 0;
            bool draw_final_empty_line = editor_length == 0 || editor_buffer[editor_length - 1] == '\n';
            while (index < editor_length || draw_final_empty_line)
            {
                draw_final_empty_line = false;
                char line[241];
                int line_length = 0;
                while (index < editor_length && editor_buffer[index] != '\n' && line_length < columns)
                {
                    line[line_length++] = editor_buffer[index++];
                }
                line[line_length] = '\0';
                if (visual_row >= first_row && visual_row < first_row + visible_rows)
                {
                    const int row_y = text_y + (visual_row - first_row) * line_height;
                    gui->draw_text(line, text_x, row_y, COLOR_TEXT_NORMAL);
                    if (visual_row == cursor_row)
                    {
                        gui->draw_text("_", text_x + cursor_column * char_width, row_y, COLOR_HIGHLIGHT);
                    }
                }
                if (index < editor_length && editor_buffer[index] == '\n')
                {
                    ++index;
                    if (index == editor_length)
                    {
                        draw_final_empty_line = true;
                    }
                }
                ++visual_row;
            }

            gui->fill_block(editor_x, middle_y + editor_h - status_h, COLOR_SIDE_BAR, editor_w, status_h);
            if (file_prompt_mode != 0)
            {
                gui->draw_text(file_prompt_mode == 1 ? "New:" : "Open:", editor_x + 8, middle_y + editor_h - status_h + 3, COLOR_TEXT_WHITE);
                gui->draw_text(filename_prompt, editor_x + 56, middle_y + editor_h - status_h + 3, COLOR_TEXT_NORMAL);
            }
            else
            {
                gui->draw_text(editor_status, editor_x + 8, middle_y + editor_h - status_h + 3, editor_dirty ? COLOR_HIGHLIGHT : COLOR_TEXT_DIM);
            }
        }
    }
    
    // TERMINAL PANEL (bottom panel when visible)
    if (terminal_panel_visible && terminal_panel_h_local > 0 && editor_w > 0)
    {
        int terminal_y = middle_y + editor_h;
        gui->fill_block(editor_x, terminal_y, 0xFF1E1E1E, editor_w, terminal_panel_h_local);
        
        // Terminal panel header
        gui->fill_block(editor_x, terminal_y, 0xFF252526, editor_w, 24);
        gui->draw_text("TERMINAL", editor_x + 8, terminal_y + 8, COLOR_TEXT_WHITE);
        
        // Terminal content area
        int terminal_content_y = terminal_y + 24;
        int terminal_content_h = terminal_panel_h_local - 24;
        
        if (terminal_content_h > 0 && editor_w > 16)
        {
            // Initialize terminal output if empty
            if (terminal_output_count == 0 || terminal_output[0][0] == '\0')
            {
                terminal_output_count = 1;
                terminal_output[0][0] = '\0';
                terminal_write_string("NasuaOS Terminal Integrated in SuaEdit\n");
            }
            
            // Draw terminal output with text wrapping
            int line_height = 12;
            int char_width = 9;
            int max_cols = (editor_w - 16) / char_width;
            if (max_cols < 10) 
            {
                max_cols = 10;
            }

            if (max_cols > 255) 
            {
                max_cols = 255;
            }
            
            int max_visible_lines = terminal_content_h / line_height;
            if (max_visible_lines < 1) 
            {
                max_visible_lines = 1;
            }
            
            // Count total visual rows (accounting for wrapped lines)
            int total_visual_rows = 0;
            for (int i = 0; i < terminal_output_count; i++)
            {
                int len = 0;
                while (terminal_output[i][len] != '\0') 
                {
                    len++;
                }
                
                if (len == 0)
                {
                    total_visual_rows++;
                }
                else
                {
                    total_visual_rows += (len + max_cols - 1) / max_cols;
                }
            }
            
            // Determine starting visual row for scrolling
            int start_visual_row = 0;
            if (total_visual_rows > max_visible_lines)
            {
                start_visual_row = total_visual_rows - max_visible_lines - terminal_scroll_offset;
                if (start_visual_row < 0) 
                {
                    start_visual_row = 0;
                }
            }
            
            // Draw visual rows with wrapping, tracking the last visible output position
            // so the input can continue on the same line.
            int current_visual_row = 0;
            int current_y = terminal_content_y + 4;
            int terminal_text_x = editor_x + 8;
            int terminal_bottom = terminal_y + terminal_panel_h_local - 4;
            
            int last_output_x = terminal_text_x;
            int last_output_y = current_y;
            
            for (int i = 0; i < terminal_output_count; i++)
            {
                int len = 0;
                while (terminal_output[i][len] != '\0') 
                {
                    len++;
                }
                
                if (len == 0)
                {
                    if (current_visual_row >= start_visual_row && current_y < terminal_bottom)
                    {
                        last_output_x = terminal_text_x;
                        last_output_y = current_y;
                        current_y += line_height;
                    }
                    current_visual_row++;
                }
                else
                {
                    int num_chunks = (len + max_cols - 1) / max_cols;
                    for (int chunk = 0; chunk < num_chunks; chunk++)
                    {
                        int chunk_start = chunk * max_cols;
                        int chunk_len = len - chunk_start;
                        if (chunk_len > max_cols) 
                        {
                            chunk_len = max_cols;
                        }
                        
                        if (current_visual_row >= start_visual_row && current_y < terminal_bottom)
                        {
                            char chunk_buf[256];
                            for (int j = 0; j < chunk_len; j++)
                            {
                                chunk_buf[j] = terminal_output[i][chunk_start + j];
                            }
                            chunk_buf[chunk_len] = '\0';
                            
                            gui->draw_text(chunk_buf, terminal_text_x, current_y, COLOR_TEXT_NORMAL);
                            last_output_x = terminal_text_x + chunk_len * char_width;
                            last_output_y = current_y;
                            current_y += line_height;
                        }
                        current_visual_row++;
                    }
                }
            }
            
            // Draw current input line, continuing from the last visible output
            char input_with_prompt[512];
            int prompt_len = 0;
            const char* prompt = "";
            while (prompt[prompt_len] != '\0') 
            {
                prompt_len++;
            }
            
            for (int j = 0; j < prompt_len; j++)
            {
                input_with_prompt[j] = prompt[j];
            }
            
            int input_len = 0;
            while (terminal_input[input_len] != '\0') 
            {
                input_len++;
            }
            
            for (int j = 0; j < input_len; j++)
            {
                input_with_prompt[prompt_len + j] = terminal_input[j];
            }
            int total_len = prompt_len + input_len;
            input_with_prompt[total_len] = '\0';
            
            // Start input right after the last visible output (same visual row)
            int input_x = last_output_x;
            int input_y = last_output_y;
            
            // If last line fills the entire width, wrap to next line
            int cols_left_on_line = max_cols - (last_output_x - terminal_text_x) / char_width;
            if (cols_left_on_line < 1)
            {
                input_x = terminal_text_x;
                input_y += line_height;
            }
            
            // Draw input with wrapping
            int cursor_x = 0;
            int cursor_y = input_y;
            int cursor_found = 0;
            
            if (total_len <= max_cols)
            {
                if (input_y < terminal_bottom)
                {
                    gui->draw_text(input_with_prompt, input_x, input_y, COLOR_TEXT_WHITE);
                    cursor_x = input_x + total_len * char_width;
                    cursor_y = input_y;
                    cursor_found = 1;
                }
            }
            else
            {
                int num_chunks = (total_len + max_cols - 1) / max_cols;
                int cursor_chunk = (total_len - 1) / max_cols;
                
                for (int chunk = 0; chunk < num_chunks && input_y < terminal_bottom; chunk++)
                {
                    int chunk_start = chunk * max_cols;
                    int chunk_len = total_len - chunk_start;
                    if (chunk_len > max_cols) 
                    {
                        chunk_len = max_cols;
                    }
                    
                    char chunk_buf[256];
                    for (int j = 0; j < chunk_len; j++)
                    {
                        chunk_buf[j] = input_with_prompt[chunk_start + j];
                    }
                    chunk_buf[chunk_len] = '\0';
                    
                    gui->draw_text(chunk_buf, input_x, input_y, COLOR_TEXT_WHITE);
                    
                    if (chunk == cursor_chunk)
                    {
                        cursor_x = input_x + chunk_len * char_width;
                        cursor_y = input_y;
                        cursor_found = 1;
                    }
                    
                    input_x = terminal_text_x;
                    input_y += line_height;
                }
            }
            
            // Draw cursor
            if (cursor_found && cursor_y < terminal_bottom)
            {
                gui->draw_text("_", cursor_x, cursor_y, COLOR_HIGHLIGHT);
            }
        }
    }

    if (file_menu_open)
    {
        const int menu_x = content_x + 6;
        const int menu_y = content_y + top_menu_h;
        int menu_w = 156;
        if (menu_w > content_w - 8)
        {
            menu_w = content_w - 8;
        }
        if (menu_w > 0)
        {
            gui->fill_block(menu_x, menu_y, COLOR_SIDE_BAR, menu_w, 76);
            draw_rect_outline(menu_x, menu_y, menu_w, 76, COLOR_SEPARATOR);
            gui->draw_text("New File", menu_x + 12, menu_y + 8, COLOR_TEXT_NORMAL);
            gui->draw_text("Open File", menu_x + 12, menu_y + 32, COLOR_TEXT_NORMAL);
            gui->draw_text("Save", menu_x + 12, menu_y + 56, COLOR_TEXT_NORMAL);
        }
    }
}


static void terminal_write_char(char c)
{
    if (terminal_output_count == 0) 
    {
        terminal_output_count = 1;
        terminal_output[0][0] = '\0';
    }

    if (c == '\r')
    {
        return;
    }

    if (c == '\n') 
    {
        terminal_scroll_offset = 0;
        if (terminal_output_count < 64) 
        {
            terminal_output[terminal_output_count][0] = '\0';
            terminal_output_count++;
        }
        else 
        {
            for (int i = 0; i < 63; i++) 
            {
                int j = 0;
                while (terminal_output[i + 1][j] != '\0') 
                {
                    terminal_output[i][j] = terminal_output[i + 1][j];
                    j++;
                }
                terminal_output[i][j] = '\0';
            }
            terminal_output[63][0] = '\0';
        }
        return;
    }

    int line = terminal_output_count - 1;
    int len = 0;
    while (terminal_output[line][len] != '\0') 
    {
        len++;
    }
    
    if (len < 255) 
    {
        terminal_output[line][len] = c;
        terminal_output[line][len + 1] = '\0';
    }
}

static void terminal_write_string(const char* str) 
{
    while (*str) 
    {
        terminal_write_char(*str++);
    }
}

static void run_current_script()
{
    terminal_panel_visible = true;
    terminal_output_count = 0;
    terminal_scroll_offset = 0;
    terminal_output[0][0] = '\0';
    if (!is_suascript_file())
    {
        terminal_write_string("Run is only available for .sua files.\n");
        return;
    }
    terminal_write_string("SuaScript: compiling and running...\n");
    if (SuaScript::compile_and_run(editor_buffer, editor_length, terminal_write_char))
    {
        terminal_write_string("Program finished.\n");
    }
}

static void suaedit_key(napp_window* win, char key)
{
    (void)win; // Unused parameter

    if (key == NAPP_KEY_F5)
    {
        run_current_script();
        return;
    }

    if (!terminal_panel_visible)
    {
        if (file_prompt_mode != 0)
        {
            if (key == '\n')
            {
                finish_filename_prompt();
            }
            else if (key == '\b')
            {
                int length = 0;
                while (filename_prompt[length] != '\0')
                {
                    ++length;
                }
                if (length > 0)
                {
                    filename_prompt[length - 1] = '\0';
                }
            }
            else if (key >= 32 && key <= 126 && key != '/' && key != '\\')
            {
                int length = 0;
                while (filename_prompt[length] != '\0')
                {
                    ++length;
                }
                if (length < static_cast<int>(sizeof(filename_prompt) - 1))
                {
                    filename_prompt[length] = key;
                    filename_prompt[length + 1] = '\0';
                }
            }
            return;
        }

        if (key == '\b')
        {
            if (editor_cursor > 0)
            {
                for (uint16_t index = editor_cursor - 1; index < editor_length; ++index)
                {
                    editor_buffer[index] = editor_buffer[index + 1];
                }
                --editor_cursor;
                --editor_length;
                editor_dirty = true;
            }
            return;
        }

        if (key == '\n' || (key >= 32 && key <= 126))
        {
            if (editor_length >= sizeof(editor_buffer) - 1)
            {
                editor_status = "File limit reached";
                return;
            }
            for (uint16_t index = editor_length; index > editor_cursor; --index)
            {
                editor_buffer[index] = editor_buffer[index - 1];
            }
            editor_buffer[editor_cursor++] = key;
            ++editor_length;
            editor_buffer[editor_length] = '\0';
            editor_dirty = true;
            editor_status = "Unsaved changes";
            return;
        }
        return;
    }
    
    if (!terminal_panel_visible)
    {
        return;
    }

    if (key == '\n') 
    {
        if (terminal_cursor > 0) 
        {
            terminal_write_string(terminal_input);
            terminal_write_char('\n');
            
            // Check for "clear" command — handle locally so the main
            // screen text buffer is not wiped by Gpu::init_text_buffer().
            bool is_clear = (terminal_cursor >= 5 && terminal_input[0] == 'c' && terminal_input[1] == 'l' && terminal_input[2] == 'e' && terminal_input[3] == 'a' && terminal_input[4] == 'r' && (terminal_input[5] == '\0' || terminal_input[5] == ' '));
            
            if (is_clear)
            {
                terminal_output_count = 0;
                terminal_output[0][0] = '\0';
                terminal_scroll_offset = 0;
                terminal_write_string("NasuaOS Terminal Integrated in SuaEdit\n");
            }
            else if (api != nullptr && api->execute_command != nullptr && api->set_print_redirect != nullptr)
            {
                // Redirect command output into this terminal panel instead
                // of the kernel's main shell text buffer.
                api->set_print_redirect(terminal_write_char);
                api->execute_command(terminal_input);
                api->set_print_redirect(nullptr);
            }
            else if (api != nullptr && api->execute_command != nullptr)
            {
                api->execute_command(terminal_input);
            }
            else
            {
                terminal_write_string("Command execution not available.\n");
            }

            if (api && api->clawfs_get_sector && api->clawfs_resolve_path && api->clawfs_get_sector("/") != 0)
            {
                if (api->clawfs_resolve_path(api->current_path, "info.txt") == 0)
                {
                    ensure_info_file();
                }
                if (editor_filename[0] == '\0')
                {
                    load_file("info.txt");
                }
                refresh_explorer();
            }
        }
        else 
        {
            terminal_write_char('\n');
        }

        terminal_cursor = 0;
        terminal_input[0] = '\0';
        return;
    }

    if (key == '\b') 
    {
        if (terminal_cursor > 0) 
        {
            terminal_cursor--;
            terminal_input[terminal_cursor] = '\0';
        }
        return;
    }

    // Scroll up (Ctrl+K, 0x0B)
    if (key == 0x0B)
    {
        int line_height = 12;
        int terminal_content_h = terminal_panel_h - 24;
        int max_visible = terminal_content_h / line_height;
        if (max_visible < 1) 
        {
            max_visible = 1;
        }
        terminal_scroll_offset += max_visible;
        return;
    }

    // Scroll down (Ctrl+L, 0x0C)
    if (key == 0x0C)
    {
        int line_height = 12;
        int terminal_content_h = terminal_panel_h - 24;
        int max_visible = terminal_content_h / line_height;
        if (max_visible < 1) 
        {
            max_visible = 1;
        }
        terminal_scroll_offset -= max_visible;
        if (terminal_scroll_offset < 0) 
        {
            terminal_scroll_offset = 0;
        }
        return;
    }

    if (terminal_cursor < 255) 
    {
        terminal_input[terminal_cursor++] = key;
        terminal_input[terminal_cursor] = '\0';
    }
}

static void suaedit_mouse(napp_window* win, int mouse_x, int mouse_y)
{
    if (!win || gui == nullptr)
    {
        return;
    }

    const int titlebar_h = win->title_height;
    const int content_x = win->pos_x;
    const int content_y = win->pos_y + titlebar_h;
    const int content_w = win->width;
    const int content_h = win->height - titlebar_h;

    if (content_w <= 0 || content_h <= 0) 
    {
        return;
    }

    const int top_menu_h = 26;
    
    // Check clicks on the menu bar
    if (mouse_y >= content_y && mouse_y < content_y + top_menu_h)
    {
        const char* menu_items[] = { "File", "Edit", "Selection", "View", "Go", "Run", "Terminal", "Help" };
        int current_menu_x = content_x + 10;
        
        for (int i = 0; i < 8; i++)
        {
            int len = 0;
            while(menu_items[i][len] != '\0') 
            {
                len++;
            }

            int text_width = len * 8;
            
            if (mouse_x >= current_menu_x && mouse_x < current_menu_x + text_width)
            {
                if (i == 0)
                {
                    file_menu_open = !file_menu_open;
                }
                else if (i == 6)
                {
                    file_menu_open = false;
                    terminal_panel_visible = !terminal_panel_visible;
                }
                else if (i == 5)
                {
                    file_menu_open = false;
                    run_current_script();
                }
                else if (i == 7)
                {
                    file_menu_open = false;
                    if (editor_dirty && !save_file())
                    {
                        return;
                    }
                    load_file("info.txt");
                }
                else
                {
                    file_menu_open = false;
                }
                return;
            }
            
            current_menu_x += text_width + 10;
            if (i < 7)
            {
                current_menu_x += 10; 
            }
        }
        file_menu_open = false;
        return;
    }

    if (file_menu_open)
    {
        const int menu_x = content_x + 6;
        const int menu_y = content_y + top_menu_h;
        const int menu_w = content_w - 8 < 156 ? content_w - 8 : 156;
        if (menu_w > 0 && mouse_x >= menu_x && mouse_x < menu_x + menu_w &&
            mouse_y >= menu_y && mouse_y < menu_y + 76)
        {
            const int item = (mouse_y - menu_y) / 24;
            file_menu_open = false;
            if (item == 0 || item == 1)
            {
                if (editor_dirty && !save_file())
                {
                    return;
                }
                begin_filename_prompt(item == 0 ? 1 : 2);
            }
            else if (item == 2)
            {
                if (editor_filename[0] == '\0')
                {
                    begin_filename_prompt(1);
                }
                else
                {
                    save_file();
                }
            }
            return;
        }
        file_menu_open = false;
    }
    
    int activity_bar_w = 48;
    if (activity_bar_w > content_w) 
    {
        activity_bar_w = content_w;
    }
    
    int middle_y = content_y + top_menu_h;
    int middle_h = content_h - top_menu_h;

    int side_bar_w = content_w < 500 ? 140 : 180;
    if (side_bar_w > content_w - activity_bar_w)
    {
        side_bar_w = content_w - activity_bar_w;
    }
    if (side_bar_w < 0)
    {
        side_bar_w = 0;
    }
    const int sidebar_x = content_x + activity_bar_w;
    if (active_activity_tab == 0 && mouse_x >= sidebar_x && mouse_x < sidebar_x + side_bar_w &&
        mouse_y >= middle_y + 108 && mouse_y < middle_y + 108 + explorer_file_count * 18)
    {
        const int file_index = (mouse_y - (middle_y + 108)) / 18;
        if (file_index >= 0 && file_index < explorer_file_count)
        {
            if (editor_dirty && !save_file())
            {
                return;
            }
            if (load_file(explorer_files[file_index]))
            {
                refresh_explorer();
            }
            else
            {
                editor_status = "Could not open file";
            }
            return;
        }
    }

    const int editor_x = content_x + activity_bar_w + side_bar_w;
    const int editor_w = content_w - activity_bar_w - side_bar_w;
    const int editor_h = middle_h - (terminal_panel_visible ? terminal_panel_h : 0);
    const int text_top = middle_y + 45;
    const int status_top = middle_y + editor_h - 18;

    if (mouse_x >= editor_x && mouse_x < editor_x + editor_w &&
        mouse_y >= text_top && mouse_y < status_top && file_prompt_mode == 0)
    {
        const int columns_raw = (editor_w - 28) / 9;
        int columns = columns_raw < 1 ? 1 : columns_raw;
        if (columns > 240)
        {
            columns = 240;
        }
        const int text_height = editor_h - 35 - 18 - 14;
        int visible_rows = text_height / 16;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }
        int cursor_row = 0;
        int cursor_column = 0;
        for (uint16_t index = 0; index < editor_cursor; ++index)
        {
            if (editor_buffer[index] == '\n')
            {
                ++cursor_row;
                cursor_column = 0;
            }
            else if (++cursor_column >= columns)
            {
                ++cursor_row;
                cursor_column = 0;
            }
        }
        const int first_row = cursor_row - visible_rows + 1 > 0 ? cursor_row - visible_rows + 1 : 0;
        const int target_row = first_row + (mouse_y - text_top) / 16;
        const int target_column = (mouse_x - (editor_x + 14)) / 9;
        move_editor_cursor(target_row, target_column, columns);
        return;
    }

    // Check clicks on the Activity Bar
    if (mouse_x >= content_x && mouse_x < content_x + activity_bar_w)
    {
        if (mouse_y >= middle_y && mouse_y < middle_y + middle_h)
        {
            int icon_y = middle_y + 10;
            for (int i = 0; i < ACTIVITY_COUNT; i++)
            {
                // Update collision detection
                if (mouse_y >= icon_y && mouse_y < icon_y + 32)
                {
                    active_activity_tab = i;
                    return;
                }
                icon_y += 48;
            }
        }
    }
}


int _start(const napp_api* api_param)
{
    if (api_param == nullptr || api_param->abi_version != NAPP_ABI_VERSION || api_param->gui == nullptr)
    {
        return 1;
    }

    api = api_param;
    gui = api_param->gui;

    // Initialize terminal state
    terminal_input[0] = '\0';
    terminal_cursor = 0;
    terminal_output_count = 0;
    terminal_scroll_offset = 0;
    for (int i = 0; i < 64; i++)
    {
        terminal_output[i][0] = '\0';
    }

    editor_filename[0] = '\0';
    filename_prompt[0] = '\0';
    file_prompt_mode = 0;
    editor_status = "Ready";
    clear_editor_buffer();
    ensure_info_file();
    if (api->clawfs_get_sector && api->clawfs_get_sector("/") != 0 && !load_file("info.txt"))
    {
        editor_status = "Could not open info.txt";
    }
    refresh_explorer();

    napp_window_config config = {};

    config.title = "SuaEdit";

    config.width = 800; 
    config.height = 500;

    config.resizable = true;
    config.can_maximize = true;

    config.userdata = nullptr;

    config.draw = draw_suaedit;
    config.key = suaedit_key;
    config.mouse = suaedit_mouse;

    if (!gui->open_window(&config))
    {
        api->serial_log("[SuaEdit] Failed to open window\n");
        return 1;
    }

    api->serial_log("[SuaEdit] Window opened\n");
    return 0;
}