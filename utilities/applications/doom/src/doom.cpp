#include <napp.h>
#include "doomkeys.h"

extern "C" {
void doomgeneric_Create(int argc, char** argv);
void doomgeneric_Tick(void);
extern unsigned int* DG_ScreenBuffer;
int doom_nasua_file_size(const char* path, uint32_t* size);
int doom_nasua_read_range(const char* path, uint32_t offset, void* buffer, uint32_t length);
void doom_nasua_key_event(unsigned char key, int pressed);
uint64_t doom_nasua_get_ticks(void);
void* doom_nasua_allocate_memory(uint32_t size);
void doom_nasua_deallocate_memory(void* memory);
void doom_nasua_log(const char* text);
void doom_nasua_game_quit(void);
void doom_nasua_game_error(const char* message);
void doom_runtime_cleanup(void);
int doom_runtime_protected_call(void (*function)(void*), void* context);
__attribute__((visibility("hidden"))) extern const uint32_t __napp_relocs_start[];
extern const unsigned char __napp_image_end[];
__attribute__((visibility("hidden"))) void doom_relocate_image(
    void* image_base, const uint32_t* relocations, const void* image_end);
}

NAPP_APPLICATION("doom", "Doom using the Freedoom game data", false);

static const napp_api* doom_api;
static const napp_gui* doom_gui;
static bool doom_started;
static bool doom_quit_requested;
static char doom_error[256];
static const int doom_width = 320;
static const int doom_height = 200;
static const char* doom_default_iwad = "/home/freedoom2.wad";
static char* doom_argv[] = {
    (char*)"doom",
    (char*)"-iwad",
    (char*)"/home/freedoom2.wad"
};

static void doom_create_engine(void* context)
{
    (void)context;
    doomgeneric_Create(3, doom_argv);
}

static void doom_tick_engine(void* context)
{
    (void)context;
    doomgeneric_Tick();
}

static unsigned char doom_key(char key)
{
    switch (key)
    {
        case NAPP_KEY_ESCAPE: return KEY_ESCAPE;
        case NAPP_KEY_LEFT: return KEY_LEFTARROW;
        case NAPP_KEY_RIGHT: return KEY_RIGHTARROW;
        case NAPP_KEY_UP: return KEY_UPARROW;
        case NAPP_KEY_DOWN: return KEY_DOWNARROW;
        case NAPP_KEY_CTRL: return KEY_FIRE;
        case NAPP_KEY_SHIFT: return KEY_RSHIFT;
        case NAPP_KEY_ALT: return KEY_RALT;
        case NAPP_KEY_F5: return KEY_F5;
        case '\n': return KEY_ENTER;
        case '\b': return KEY_BACKSPACE;
        case ' ': return KEY_USE;
        case '/': return KEY_FIRE;
        default:
            if (key >= 'A' && key <= 'Z')
            {
                return (unsigned char)(key - 'A' + 'a');
            }
            return (unsigned char)key;
    }
}

static void doom_draw(napp_window* window)
{
    const int origin_x = window->pos_x + 2;
    const int origin_y = window->pos_y + window->title_height + 2;
    const int canvas_width = window->width - 4;
    const int canvas_height = window->height - window->title_height - 4;

    if (canvas_width <= 0 || canvas_height <= 0)
    {
        return;
    }

    if (doom_error[0] != '\0')
    {
        doom_gui->fill_block(origin_x, origin_y, NAPP_COLOR_BLACK, canvas_width, canvas_height);
        doom_gui->draw_text("Doom stopped:", origin_x + 8, origin_y + 8, 0xFFFF6666);
        doom_gui->draw_text(doom_error, origin_x + 8, origin_y + 28, NAPP_COLOR_WHITE);
        return;
    }

    if (doom_quit_requested || !doom_started || DG_ScreenBuffer == nullptr)
    {
        doom_gui->fill_block(origin_x, origin_y, NAPP_COLOR_BLACK, canvas_width, canvas_height);
        doom_gui->draw_text(doom_quit_requested ? "Doom exited." : "Loading Freedoom...", origin_x + 8, origin_y + 8, NAPP_COLOR_WHITE);
        return;
    }

    int render_width = canvas_width;
    int render_height = canvas_height;
    if ((int64_t)render_width * doom_height > (int64_t)render_height * doom_width)
    {
        render_width = (int64_t)render_height * doom_width / doom_height;
    }
    else
    {
        render_height = (int64_t)render_width * doom_height / doom_width;
    }

    const int render_x = origin_x + (canvas_width - render_width) / 2;
    const int render_y = origin_y + (canvas_height - render_height) / 2;
    doom_gui->fill_block(origin_x, origin_y, NAPP_COLOR_BLACK, canvas_width, canvas_height);

    for (int y = 0; y < doom_height; ++y)
    {
        const int scaled_y_start = y * render_height / doom_height;
        const int scaled_y_end = (y + 1) * render_height / doom_height;
        if (scaled_y_end <= scaled_y_start)
        {
            continue;
        }

        const int row = y * doom_width;
        int x = 0;
        while (x < doom_width)
        {
            const uint32_t pixel = DG_ScreenBuffer[row + x];
            const uint32_t color = 0xFF000000 | (pixel & 0x00FFFFFF);
            int run_end = x + 1;
            while (run_end < doom_width && DG_ScreenBuffer[row + run_end] == pixel)
            {
                ++run_end;
            }

            const int scaled_x_start = x * render_width / doom_width;
            const int scaled_x_end = run_end * render_width / doom_width;
            if (scaled_x_end > scaled_x_start)
            {
                doom_gui->fill_block(render_x + scaled_x_start, render_y + scaled_y_start, color, scaled_x_end - scaled_x_start, scaled_y_end - scaled_y_start);
            }
            x = run_end;
        }
    }
}

static void doom_key_event(napp_window* window, char key, bool pressed)
{
    (void)window;
    doom_nasua_key_event(doom_key(key), pressed ? 1 : 0);
}

static void doom_tick(napp_window* window)
{
    (void)window;
    if (doom_started && !doom_quit_requested && doom_error[0] == '\0')
    {
        doom_runtime_protected_call(doom_tick_engine, nullptr);
    }
}

extern "C" int doom_nasua_file_size(const char* path, uint32_t* size)
{
    if (doom_api == nullptr || path == nullptr || size == nullptr || doom_api->clawfs_get_file_size == nullptr)
    {
        return 0;
    }
    return doom_api->clawfs_get_file_size(path, size);
}

extern "C" int doom_nasua_read_range(const char* path, uint32_t offset, void* buffer, uint32_t length)
{
    if (doom_api == nullptr || path == nullptr || doom_api->clawfs_read_file_range == nullptr)
    {
        return 0;
    }
    return doom_api->clawfs_read_file_range(path, offset, buffer, length);
}

extern "C" void doom_nasua_set_api(const napp_api* api)
{
    doom_api = api;
}

extern "C" uint64_t doom_nasua_get_ticks(void)
{
    return doom_api != nullptr && doom_api->get_ticks != nullptr ? doom_api->get_ticks() : 0;
}

extern "C" void* doom_nasua_allocate_memory(uint32_t size)
{
    return doom_api != nullptr && doom_api->allocate != nullptr
        ? doom_api->allocate(size)
        : nullptr;
}

extern "C" void doom_nasua_deallocate_memory(void* memory)
{
    if (doom_api != nullptr && doom_api->deallocate != nullptr)
    {
        doom_api->deallocate(memory);
    }
}

extern "C" void doom_nasua_log(const char* text)
{
    if (doom_api != nullptr && doom_api->serial_log != nullptr)
    {
        doom_api->serial_log(text);
    }
}

extern "C" void doom_nasua_game_quit(void)
{
    doom_quit_requested = true;
}

extern "C" void doom_nasua_game_error(const char* message)
{
    if (doom_error[0] != '\0')
    {
        return;
    }
    if (message == nullptr)
    {
        message = "Unknown engine error";
    }
    uint32_t i = 0;
    while (message[i] != '\0' && i < sizeof(doom_error) - 1)
    {
        doom_error[i] = message[i];
        ++i;
    }
    doom_error[i] = '\0';
}

static void doom_close(napp_window* window)
{
    (void)window;
    doom_runtime_cleanup();
    doom_started = false;
}

int _start(const napp_api* api)
{
    uintptr_t entry_address;
    uintptr_t relocations_address;
    uintptr_t image_end_address;
    __asm__ volatile("leaq _start(%%rip), %0\n\t" "leaq __napp_relocs_start(%%rip), %1\n\t" "leaq __napp_image_end(%%rip), %2" : "=r"(entry_address), "=r"(relocations_address), "=r"(image_end_address));
    doom_relocate_image((void*)(entry_address - NAPP_ALIGNED_HEADER_SIZE), (const uint32_t*)relocations_address, (const void*)image_end_address);

    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION || api->gui == nullptr || api->allocate == nullptr || api->deallocate == nullptr || api->clawfs_get_file_size == nullptr || api->clawfs_read_file_range == nullptr || api->enable_fpu == nullptr || !api->enable_fpu())
    {
        return 1;
    }

    uint32_t iwad_size = 0;
    if (!api->clawfs_get_file_size(doom_default_iwad, &iwad_size))
    {
        api->print_error("Freedoom IWAD missing from ClawFS and ISO rootfs.\n");
        return 1;
    }

    doom_api = api;
    doom_gui = api->gui;

    napp_window_config config = {};
    config.title = "Doom";
    config.width = doom_width + 4;
    config.height = doom_height + 36;
    config.resizable = false;
    config.can_maximize = true;
    config.draw = doom_draw;
    config.key_event = doom_key_event;
    config.close = doom_close;
    config.tick = doom_tick;
    config.tick_interval_ms = 10;

    if (!doom_gui->open_window(&config))
    {
        api->print_error("Doom could not open its window.\n");
        return 1;
    }

    doom_nasua_set_api(api);
    doom_started = doom_runtime_protected_call(doom_create_engine, nullptr) == 0;
    if (doom_started && doom_error[0] == '\0')
    {
        api->serial_log("[DOOM] Doomgeneric started with Freedoom IWAD\n");
    }
    else if (doom_quit_requested)
    {
        api->serial_log("[DOOM] Doomgeneric exited during startup\n");
    }
    else
    {
        api->serial_log("[DOOM] Doomgeneric reported a startup error\n");
    }
    return 0;
}
