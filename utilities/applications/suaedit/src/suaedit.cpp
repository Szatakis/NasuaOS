#include <napp.h>

NAPP_APPLICATION("suaedit", "Text editor", true);

static const napp_gui* gui = nullptr;

// Application state
static int active_activity_tab = 0; // 0: Explorer, 1: Search, 2: Source Control, 3: Run/Debug, 4: Extensions
static const int ACTIVITY_COUNT = 5;

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
        while(menu_items[i][len] != '\0') len++;
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
    if (side_bar_w > 0) {
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
    
    if (editor_w > 0) {
        gui->fill_block(editor_x, middle_y, COLOR_EDITOR_BG, editor_w, middle_h);
        
        // Empty tab
        const int tab_h = 35;
        if (middle_h > tab_h) 
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


int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION || api->gui == nullptr)
    {
        return 1;
    }

    gui = api->gui;

    napp_window_config config = {};

    config.title = "SuaEdit";

    config.width = 800; 
    config.height = 500;

    config.resizable = true;
    config.can_maximize = true;

    config.userdata = nullptr;

    config.draw = draw_suaedit;
    config.key = nullptr;
    config.mouse = suaedit_mouse;

    if (!gui->open_window(&config))
    {
        api->serial_log("[SuaEdit] Failed to open window\n");
        return 1;
    }

    api->serial_log("[SuaEdit] Window opened\n");
    return 0;
}