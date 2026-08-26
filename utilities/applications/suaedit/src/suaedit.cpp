#include <napp.h>

NAPP_APPLICATION("suaedit", "Text editor", true);

static const napp_gui* gui = nullptr;
static const napp_api* api = nullptr;

// Forward declarations for terminal functions
static void terminal_write_char(char c);
static void terminal_write_string(const char* str);

// Application state
static int active_activity_tab = 0; // 0: Explorer, 1: Search, 2: Source Control, 3: Run/Debug, 4: Extensions
static const int ACTIVITY_COUNT = 5;

// Terminal panel state
static bool terminal_panel_visible = false;
static int terminal_panel_h = 200; // Height when visible
static char terminal_input[256];
static int terminal_cursor = 0;
static char terminal_output[64][256];
static int terminal_output_count = 0;
static int terminal_scroll_offset = 0;

// Color scheme
static const uint32_t COLOR_MENU_BAR    = 0xFF323233;
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
            
            // Explorer content (Empty)
            if (active_activity_tab == 0) 
            {
                gui->draw_text("> PROJECT", sidebar_x + 10, middle_y + 40, COLOR_TEXT_WHITE);
            }
        }
        
        // Vertical separator between Side Bar and Editor
        gui->fill_block(sidebar_x + side_bar_w - 1, middle_y, COLOR_SEPARATOR, 1, middle_h);
    }

    // EDITOR GROUP (Empty page, center of the window)
    int editor_x = content_x + activity_bar_w + side_bar_w;
    int editor_w = content_w - activity_bar_w - side_bar_w;
    
     // Terminal panel dimensions
    int terminal_panel_h_local = terminal_panel_visible ? terminal_panel_h : 0;
    int editor_h = middle_h - terminal_panel_h_local;
    
    if (editor_w > 0 && editor_h > 0) 
    {
        gui->fill_block(editor_x, middle_y, COLOR_EDITOR_BG, editor_w, editor_h);
        
        // Empty tab
        const int tab_h = 35;
        if (editor_h > tab_h) 
        {
            gui->fill_block(editor_x, middle_y, COLOR_EDITOR_TAB, editor_w, tab_h);
            
            // Adjust the maximum width of the active tab to the remaining editor space
            int active_tab_w = 120;
            if (active_tab_w > editor_w) 
            {
                active_tab_w = editor_w;
            }
            
            if (active_tab_w > 0) 
            {
                // Background of the active tab itself
                gui->fill_block(editor_x, middle_y, COLOR_EDITOR_BG, active_tab_w, tab_h);
                gui->fill_block(editor_x, middle_y, COLOR_HIGHLIGHT, active_tab_w, 1);
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

static void suaedit_key(napp_window* win, char key)
{
    (void)win; // Unused parameter
    
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
        if (max_visible < 1) max_visible = 1;
        terminal_scroll_offset += max_visible;
        return;
    }

    // Scroll down (Ctrl+L, 0x0C)
    if (key == 0x0C)
    {
        int line_height = 12;
        int terminal_content_h = terminal_panel_h - 24;
        int max_visible = terminal_content_h / line_height;
        if (max_visible < 1) max_visible = 1;
        terminal_scroll_offset -= max_visible;
        if (terminal_scroll_offset < 0) terminal_scroll_offset = 0;
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
                // Terminal menu item clicked (index 6)
                if (i == 6)
                {
                    terminal_panel_visible = !terminal_panel_visible;
                }
                return;
            }
            
            current_menu_x += text_width + 10;
            if (i < 7) 
            {
                current_menu_x += 10; 
            }
        }
        return;
    }
    
    int activity_bar_w = 48;
    if (activity_bar_w > content_w) 
    {
        activity_bar_w = content_w;
    }
    
    int middle_y = content_y + top_menu_h;
    int middle_h = content_h - top_menu_h;

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