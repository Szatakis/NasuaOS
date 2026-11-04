#include "napp.hpp"

#include "drivers/gpu/driver.hpp"
#include "drivers/cpu/driver.hpp"
#include "drivers/disk/ata/driver.hpp"
#include "drivers/memory/driver.hpp"
#include "drivers/mouse/driver.hpp"
#include "drivers/timer/driver.hpp"
#include "drivers/uart/driver.hpp"
#include "drivers/network/driver.hpp"

#include "system/filesystem/fat/fat.hpp"
#include "system/filesystem/clawfs/clawfs.hpp"
#include "system/filesystem/file_resolver/file_resolver.hpp"

#include "system/gui/gui.hpp"
#include "system/sysfunc/logger/logger.hpp"
#include "system/vars/info_vars/info_vars.hpp"
#include "system/process/process.hpp"
#include "system/scheduler/scheduler.hpp"

#include "shell/shell.hpp"

#include "libs/libc/libc.hpp"

#define NAPP_DIRECTORY "/bin"
#define NAPP_SBIN_DIRECTORY "/sbin"
#define NAPP_EXTENSION ".napp"
#define NAPP_MAX_IMAGE_SIZE (1024 * 1024)

static bool napp_abi_supported(uint32_t version)
{
    return version >= NAPP_ABI_MIN_VERSION && version <= NAPP_ABI_VERSION;
}

static bool napp_user_mode_metadata_valid(const napp_header* header, uint32_t image_size)
{
    constexpr uint32_t BASE_HEADER_END =
        __builtin_offsetof(napp_header, header_size) + sizeof(napp_header::header_size);
    constexpr uint32_t USER_MODE_FIELD_END =
        __builtin_offsetof(napp_header, user_mode) + sizeof(napp_header::user_mode);
    if (header == nullptr || image_size < BASE_HEADER_END ||
        header->magic != NAPP_MAGIC ||
        !napp_abi_supported(header->abi_version))
    {
        return false;
    }

    return header->abi_version < 12 ||
           (image_size >= USER_MODE_FIELD_END &&
            header->header_size >= USER_MODE_FIELD_END &&
            header->header_size <= image_size);
}

static bool napp_header_is_user_mode(const napp_header* header, uint32_t image_size)
{
    return napp_user_mode_metadata_valid(header, image_size) &&
           header->abi_version >= 12 && header->user_mode;
}

static uint32_t napp_entry_offset(const napp_header* header)
{
    // ABI 6 recorded the struct size even though the linker padded it to 64 bytes.
    if (header->abi_version == NAPP_ABI_MIN_VERSION && header->header_size == NAPP_HEADER_SIZE + sizeof(bool) && header->entry_offset == header->header_size)
    {
        return (NAPP_HEADER_SIZE + sizeof(bool) + 63U) & ~63U;
    }
    return header->entry_offset;
}

fat_volume rootfs_volume;
bool rootfs_mounted = false;
void (*napp_print_redirect_cb)(char) = nullptr;
void (*napp_print_redirect_colored_cb)(char, uint32_t) = nullptr;

// Current working directory exposed to napp programs
static char napp_current_path[256] = "/home";

// A graphical application keeps its image mapped for as long as its window
// exists, because the window callbacks live inside that image.
struct napp_window_slot
{
    bool used;

    char owner[NAPP_MAX_NAME];
    char title[NAPP_MAX_NAME];

    Gpu::Window_Manager::window_struct window;
    napp_window view;

    napp_window_draw draw;
    napp_window_key key;
    napp_window_key_event key_event;
    napp_window_close close;
    napp_window_mouse mouse;
    napp_window_mouse_button mouse_button;

    napp_window_tick tick;
    int tick_interval_ms;
    uint64_t last_tick;
};

struct napp_resident_image
{
    bool used;

    char name[NAPP_MAX_NAME];
    uint32_t process_id;
    void* image;
};

static napp_window_slot window_slots[NAPP_MAX_WINDOWS];
static napp_resident_image resident_images[NAPP_MAX_WINDOWS];

static char loading_application[NAPP_MAX_NAME];
static uint32_t loading_window_count = 0;
static uint32_t loading_process_id = 0;
static uint32_t loading_napp_abi_version = NAPP_ABI_VERSION;

static void napp_api_print(const char* text)
{
    if (text != nullptr)
    {
        Gpu::print(text);
    }
}

static void napp_api_print_info(const char* text) 
{ 
    if (text != nullptr) 
    { 
        Gpu::print_info(text);
    }
}

static void napp_api_print_warn(const char* text) 
{ 
    if (text != nullptr) 
    { 
        Gpu::print_warn(text);
    }
}

static void napp_api_print_error(const char* text) 
{ 
    if (text != nullptr) 
    { 
        Gpu::print_error(text);
    }
}

static void napp_api_print_line(const char* text)
{
    if (text != nullptr)
    {
        Gpu::print(text);
        Gpu::print("\n");
    }
}

static void napp_api_print_dec(uint32_t value)
{
    Gpu::print_num8(value);
}

static void napp_api_print_hex(uint32_t value)
{
    Gpu::print_hex(value);
}

static void napp_api_sleep_ms(uint32_t milliseconds)
{
    Timer::sleep(milliseconds);
}

static void napp_api_serial_log(const char* text)
{
    if (text != nullptr)
    {
        Uart::puts(text);
    }
}

static uint64_t napp_api_get_ticks()
{
    return Timer::pit_get_ticks();
}

// wrapper

static void napp_api_set_cwd(const char* path)
{
    if (path != nullptr)
    {
        uint32_t i = 0;
        while (path[i] && i < 255) 
        { 
            napp_current_path[i] = path[i]; i++; 
        }
        napp_current_path[i] = '\0';
    }
}

// ClawFS wrappers (give userspace flat binaries access to the disk FS)
static uint32_t napp_api_clawfs_get_sector(const char* path)
{
    return get_sector_by_path(path);
}

static bool napp_api_clawfs_read_sector(uint32_t sector, void* buffer)
{
    return Disk::read_sector(sector, (uint8_t*)buffer);
}

static bool napp_api_clawfs_write_sector(uint32_t sector, const void* buffer)
{
    return Disk::write_sector(sector, (uint8_t*)buffer);
}

static bool napp_api_clawfs_build_file_path(const char* path, char* absolute_path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return false;
    }

    uint32_t path_length = 0;
    while (path[path_length] != '\0' && path_length < 256)
    {
        ++path_length;
    }
    if (path[path_length] != '\0' || path_length >= 256)
    {
        return false;
    }

    uint32_t index = 0;
    if (path[0] == '/')
    {
        while (index < path_length)
        {
            absolute_path[index] = path[index];
            ++index;
        }
    }
    else
    {
        const char* current = napp_current_path;
        while (*current != '\0' && index < 254)
        {
            absolute_path[index++] = *current++;
        }
        if (index > 0 && absolute_path[index - 1] != '/')
        {
            absolute_path[index++] = '/';
        }
        if (index + path_length >= 256)
        {
            return false;
        }
        for (uint32_t path_index = 0; path_index < path_length; ++path_index)
        {
            absolute_path[index++] = path[path_index];
        }
    }
    if (index == 0 || index >= 256)
    {
        return false;
    }
    absolute_path[index] = '\0';
    return true;
}

static bool napp_api_clawfs_read_file(const char* path, void* buffer, uint32_t capacity, uint32_t* length)
{
    char absolute_path[256];
    return napp_api_clawfs_build_file_path(path, absolute_path) && clawfs_read_file(absolute_path, buffer, capacity, length);
}

static bool napp_api_clawfs_write_file(const char* path, const void* buffer, uint32_t length)
{
    char absolute_path[256];
    return napp_api_clawfs_build_file_path(path, absolute_path) && clawfs_write_file(absolute_path, buffer, length);
}

static bool napp_api_clawfs_get_file_size(const char* path, uint32_t* size)
{
    char absolute_path[256];
    if (!napp_api_clawfs_build_file_path(path, absolute_path) || size == nullptr)
    {
        return false;
    }
    if (clawfs_get_file_size(absolute_path, size))
    {
        return true;
    }

    fat_entry_info info;
    if (!rootfs_mounted || !fat_stat(&rootfs_volume, absolute_path, &info) || info.directory)
    {
        return false;
    }
    *size = info.size;
    return true;
}

static bool napp_api_clawfs_read_file_range(const char* path, uint32_t offset, void* buffer, uint32_t length)
{
    char absolute_path[256];
    if (!napp_api_clawfs_build_file_path(path, absolute_path))
    {
        return false;
    }

    uint32_t clawfs_size;
    if (clawfs_get_file_size(absolute_path, &clawfs_size))
    {
        return clawfs_read_file_range(absolute_path, offset, buffer, length);
    }

    return rootfs_mounted && fat_read_file_range(&rootfs_volume, absolute_path, offset, buffer, length);
}

static void* napp_api_allocate(uint32_t size)
{
    return size == 0 ? nullptr : Memory::heap::kmalloc(size);
}

static void napp_api_deallocate(void* memory)
{
    Memory::heap::kfree(memory);
}

static bool napp_api_enable_fpu()
{
#if defined(__x86_64__)
    uint64_t cr0;
    uint64_t cr4;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr0 &= ~((1ULL << 2) | (1ULL << 3));
    cr0 |= 1ULL << 1;
    cr4 |= (1ULL << 9) | (1ULL << 10);
    asm volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");
    asm volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");
    asm volatile("fninit");
    return true;
#else
    return false;
#endif
}

static void napp_api_clawfs_mkdir(const char* parent_path, const char* dir_name)
{
    clawfs_mkdir(parent_path, dir_name);
}

static void napp_api_clawfs_rm(const char* parent_path, const char* name, uint32_t type)
{
    clawfs_rm(parent_path, name, type);
}

static void napp_api_clawfs_create_file_in(const char* path, const char* name)
{
    clawfs_create_file_in(path, name);
}

static void napp_api_clawfs_dir(const char* path)
{
    clawfs_dir(path);
}

static int napp_api_clawfs_get_entry_type(const char* parent_path, const char* name)
{
    // Walk to parent sector
    uint32_t parent_sector = get_sector_by_path(parent_path);
    if (parent_sector == 0)
    {
        return -1;
    }

    CLAWFSEntry entry;
    if (find_entry_in_dir(parent_sector, name, &entry) == 0)
    {
        return -1;
    }

    return (int)entry.type;
}

static uint32_t napp_api_clawfs_resolve_path(const char* cur_path, const char* rel)
{
    if (rel == nullptr || rel[0] == '\0')
    {
        return get_sector_by_path(cur_path);
    }

    if (rel[0] == '/')
    {
        return get_sector_by_path(rel);
    }

    // Build absolute path
    char abs[256];
    uint32_t i = 0;
    const char* p = cur_path;
    while (*p && i < 254) 
    { 
        abs[i++] = *p++; 
    }

    if (i > 0 && abs[i-1] != '/') 
    { 
        abs[i++] = '/'; 
    }
    const char* r = rel;

    while (*r && i < 254) 
    { 
        abs[i++] = *r++; 
    }
    abs[i] = '\0';

    return get_sector_by_path(abs);
}

static void napp_api_fill_block(int x, int y, uint32_t color, int width, int height)
{
    if (x < 0 || y < 0 || width <= 0 || height <= 0)
    {
        return;
    }

    Gpu::fill_block((size_t)x, (size_t)y, color, (size_t)width, (size_t)height);
}

static void napp_api_draw_text(const char* text, int x, int y, uint32_t color)
{
    if (text == nullptr || x < 0 || y < 0)
    {
        return;
    }

    Gpu::print_at8(text, (size_t)x, (size_t)y, color);
}

static int window_title_height(const Gpu::Window_Manager::window_struct* window)
{
    (void)window;
    return WINDOW_TITLEBAR_HEIGHT;
}

static napp_window_slot* slot_of(Gpu::Window_Manager::window_struct* window)
{
    for (uint32_t i = 0; i < NAPP_MAX_WINDOWS; i++)
    {
        if (window_slots[i].used && &window_slots[i].window == window)
        {
            return &window_slots[i];
        }
    }

    return nullptr;
}

static napp_window_slot* sync_slot(Gpu::Window_Manager::window_struct* window)
{
    napp_window_slot* slot = slot_of(window);

    if (slot == nullptr)
    {
        return nullptr;
    }

    slot->view.pos_x = window->pos_x;
    slot->view.pos_y = window->pos_y;
    slot->view.width = window->width;
    slot->view.height = window->height;
    slot->view.title_height = window_title_height(window);

    return slot;
}

static void napp_window_draw_trampoline(Gpu::Window_Manager::window_struct* window)
{
    napp_window_slot* slot = sync_slot(window);

    if (slot != nullptr && slot->draw != nullptr)
    {
        slot->draw(&slot->view);
    }
}

static void napp_window_key_trampoline(Gpu::Window_Manager::window_struct* window, char key)
{
    napp_window_slot* slot = sync_slot(window);

    if (slot != nullptr && slot->key != nullptr)
    {
        slot->key(&slot->view, key);
    }
}

static void napp_window_key_event_trampoline(Gpu::Window_Manager::window_struct* window, char key, bool pressed)
{
    napp_window_slot* slot = sync_slot(window);

    if (slot != nullptr && slot->key_event != nullptr)
    {
        slot->key_event(&slot->view, key, pressed);
    }
}

static void napp_window_close_trampoline(Gpu::Window_Manager::window_struct* window)
{
    napp_window_slot* slot = sync_slot(window);

    if (slot != nullptr && slot->close != nullptr)
    {
        slot->close(&slot->view);
    }
}

static void napp_window_mouse_trampoline(Gpu::Window_Manager::window_struct* window, int mouse_x, int mouse_y)
{
    napp_window_slot* slot = sync_slot(window);

    if (slot != nullptr && slot->mouse != nullptr)
    {
        slot->mouse(&slot->view, mouse_x, mouse_y);
    }
}

static void napp_window_mouse_button_trampoline(Gpu::Window_Manager::window_struct* window, int mouse_x, int mouse_y, int button)
{
    napp_window_slot* slot = sync_slot(window);

    if (slot != nullptr && slot->mouse_button != nullptr)
    {
        slot->mouse_button(&slot->view, mouse_x, mouse_y, button);
    }
}

static void napp_window_tick_trampoline(Gpu::Window_Manager::window_struct* window)
{
    napp_window_slot* slot = sync_slot(window);

    if (slot != nullptr && slot->tick != nullptr)
    {
        slot->tick(&slot->view);
    }
}

static napp_window_slot* find_window_of(const char* application)
{
    for (uint32_t i = 0; i < NAPP_MAX_WINDOWS; i++)
    {
        if (window_slots[i].used && strcmp(window_slots[i].owner, application) == 0)
        {
            return &window_slots[i];
        }
    }

    return nullptr;
}

static void show_window(napp_window_slot* slot)
{
    slot->window.visible = true;
    slot->window.minimized = false;
    slot->window.id = current_id;

    current_id++;

    Gpu::Window_Manager::register_window(&slot->window);
}

static napp_window_slot* current_napp_slot = nullptr;

void napp_api_resize_window(int width, int height)
{
    if (current_napp_slot == nullptr)
    {
        return;
    }

    current_napp_slot->window.width = width;
    current_napp_slot->window.height = height;
    current_napp_slot->window.restore_width = width;
    current_napp_slot->window.restore_height = height;
}

bool napp_api_open_window(const napp_window_config* config)
{
    if (config == nullptr || config->draw == nullptr || loading_application[0] == '\0')
    {
        return false;
    }

    napp_window_slot* slot = nullptr;

    for (uint32_t i = 0; i < NAPP_MAX_WINDOWS; i++)
    {
        if (!window_slots[i].used)
        {
            slot = &window_slots[i];

            break;
        }
    }

    if (slot == nullptr)
    {
        log(ERROR, "NAPP", "No free window slot");

        return false;
    }

    Memory::memset(slot, 0, sizeof(napp_window_slot));

    strcpy(slot->owner, loading_application);
    strcpy(slot->title, config->title != nullptr ? config->title : loading_application);

    slot->draw = config->draw;
    slot->key = config->key;
    if (loading_napp_abi_version >= 7)
    {
        slot->key_event = config->key_event;
        slot->close = config->close;
    }
    slot->mouse = config->mouse;
    slot->mouse_button = config->mouse_button;

    slot->tick = config->tick;
    slot->tick_interval_ms = config->tick_interval_ms;
    slot->last_tick = Timer::pit_get_ticks();

    slot->view.userdata = config->userdata;

    slot->window.name = slot->title;
    slot->window.process_id = loading_process_id;
    slot->window.pos_x = 10;
    slot->window.pos_y = 10;
    slot->window.width = config->width > 0 ? config->width : 320;
    slot->window.height = config->height > 0 ? config->height : 240;
    if (strcmp(loading_application, "terminal") == 0 && Gpu::fb != nullptr)
    {
        const int right_margin = 20;
        const int bottom_margin = 20;
        const int available_bottom = static_cast<int>(Gpu::fb->height) -
            static_cast<int>(bar_h_scaled) - bottom_margin;
        slot->window.pos_x = static_cast<int>(Gpu::fb->width) -
            slot->window.width - right_margin;
        slot->window.pos_y = available_bottom - slot->window.height;
        if (slot->window.pos_x < 0)
        {
            slot->window.pos_x = 0;
        }
        if (slot->window.pos_y < 0)
        {
            slot->window.pos_y = 0;
        }
        Mouse::x = slot->window.pos_x + (slot->window.width * 3) / 4;
        Mouse::y = slot->window.pos_y + WINDOW_TITLEBAR_HEIGHT +
            (slot->window.height - WINDOW_TITLEBAR_HEIGHT) / 2;
        if (Mouse::x >= static_cast<int32_t>(Gpu::fb->width) - CURSOR_W)
        {
            Mouse::x = static_cast<int32_t>(Gpu::fb->width) - CURSOR_W - 1;
        }
        if (Mouse::y >= static_cast<int32_t>(Gpu::fb->height) - CURSOR_H)
        {
            Mouse::y = static_cast<int32_t>(Gpu::fb->height) - CURSOR_H - 1;
        }
    }
    slot->window.resizable = config->resizable;
    slot->window.can_maximize = config->can_maximize;
    slot->window.userdata = slot;
    slot->window.draw_content = napp_window_draw_trampoline;
    slot->window.key_press = napp_window_key_trampoline;
    slot->window.key_event = slot->key_event != nullptr ? napp_window_key_event_trampoline : nullptr;
    slot->window.close = slot->close != nullptr ? napp_window_close_trampoline : nullptr;
    slot->window.mouse_click = napp_window_mouse_trampoline;
    slot->window.mouse_button = napp_window_mouse_button_trampoline;

    current_napp_slot = slot;

    slot->used = true;

    show_window(slot);

    loading_window_count++;

    return true;
}

static const napp_gui kernel_napp_gui =
{
    napp_api_open_window,
    napp_api_fill_block,
    napp_api_draw_text,
    napp_api_resize_window
};

// Updated at the start of each napp_run_path call so the binary sees the
// shell's current working directory.
static void napp_api_execute_command(const char* cmd)
{
    execute_command(cmd);
}

static void napp_api_set_print_redirect(void (*callback)(char))
{
    napp_print_redirect_cb = callback;
    napp_print_redirect_colored_cb = nullptr;
}

static void napp_api_set_print_redirect_colored(void (*callback)(char, uint32_t))
{
    napp_print_redirect_cb = nullptr;
    napp_print_redirect_colored_cb = callback;
}

static const char* napp_api_get_username()
{
    return user_name;
}

static uint32_t napp_api_get_processes(napp_process_info* output, uint32_t capacity)
{
    if (output == nullptr || capacity == 0)
    {
        return 0;
    }

    Process::Info processes[Process::MAX_PROCESSES];
    const uint32_t count = Process::snapshot(processes, Process::MAX_PROCESSES);
    const uint32_t copied = count < capacity ? count : capacity;
    for (uint32_t index = 0; index < copied; ++index)
    {
        output[index].pid = processes[index].pid;
        output[index].type = static_cast<uint32_t>(processes[index].type);
        output[index].state = static_cast<uint32_t>(processes[index].state);
        output[index].exit_code = processes[index].exit_code;
        output[index].open_windows = processes[index].open_windows;
        output[index].start_tick = processes[index].start_tick;
        for (uint32_t character = 0; character < NAPP_PROCESS_NAME_LENGTH; ++character)
        {
            output[index].name[character] = '\0';
        }
        for (uint32_t character = 0; character + 1 < NAPP_PROCESS_NAME_LENGTH; ++character)
        {
            output[index].name[character] = processes[index].name[character];
            if (processes[index].name[character] == '\0')
            {
                break;
            }
        }
        output[index].name[NAPP_PROCESS_NAME_LENGTH - 1] = '\0';
    }
    return copied;
}

static bool napp_api_get_system_info(napp_system_info* output)
{
    if (output == nullptr)
    {
        return false;
    }

    output->memory_total_bytes = Memory::total_bytes();
    output->storage_total_bytes = Disk::total();
    output->framebuffer_width = Gpu::fb != nullptr ? Gpu::fb->width : 0;
    output->framebuffer_height = Gpu::fb != nullptr ? Gpu::fb->height : 0;

    for (uint32_t index = 0; index < sizeof(output->architecture); ++index)
    {
        output->architecture[index] = '\0';
    }
    const char* architecture = Cpu::get_architecture();
    uint32_t index = 0;
    while (architecture != nullptr && architecture[index] != '\0' && index < sizeof(output->architecture) - 1)
    {
        output->architecture[index] = architecture[index];
        ++index;
    }
    output->architecture[index] = '\0';
    return true;
}

static int32_t napp_api_http_get(const char* hostname, uint16_t port, const char* path, char* response, uint32_t response_capacity, uint32_t timeout_ms)
{
    return Network::http_get(hostname, port, path, response, response_capacity, timeout_ms);
}

static int32_t napp_api_https_get(const char* hostname, uint16_t port, const char* path, char* response, uint32_t response_capacity, uint32_t timeout_ms)
{
    return Network::https_get(hostname, port, path, response, response_capacity, timeout_ms);
}

static napp_api kernel_napp_api =
{
    NAPP_ABI_VERSION,
    napp_api_print,
    napp_api_print_info,
    napp_api_print_warn, 
    napp_api_print_error,
    napp_api_print_line,
    napp_api_print_dec,
    napp_api_print_hex,
    napp_api_sleep_ms,
    napp_api_serial_log,
    &kernel_napp_gui,
    0,  // argc
    nullptr, // argv
    napp_current_path, // current_path
    napp_api_set_cwd,
    napp_api_clawfs_get_sector,
    napp_api_clawfs_read_sector,
    napp_api_clawfs_write_sector,
    napp_api_clawfs_mkdir,
    napp_api_clawfs_rm,
    napp_api_clawfs_create_file_in,
    napp_api_clawfs_dir,
    napp_api_clawfs_get_entry_type,
    napp_api_clawfs_resolve_path,
    napp_api_get_ticks,
    napp_api_execute_command,
    napp_api_set_print_redirect,
    napp_api_http_get,
    napp_api_https_get,
    napp_api_clawfs_read_file,
    napp_api_clawfs_write_file,
    napp_api_allocate,
    napp_api_deallocate,
    napp_api_clawfs_get_file_size,
    napp_api_clawfs_read_file_range,
    napp_api_enable_fpu,
    napp_api_get_username,
    napp_api_get_processes,
    napp_api_get_system_info,
    napp_api_set_print_redirect_colored
};

static bool napp_launch_user_image(const void* image, uint32_t image_size,
                                   uint32_t entry_offset, uint32_t process_id)
{
    constexpr uint64_t USER_IMAGE_OFFSET = 0x10000;
    constexpr uint64_t USER_STACK_OFFSET = 0x200000;
    constexpr uint64_t PAGE_SIZE = Memory::PAGE_SIZE;

    if (image == nullptr || image_size == 0 || entry_offset >= image_size ||
        image_size > NAPP_MAX_IMAGE_SIZE)
    {
        return false;
    }

    Memory::vmm::UserAddressSpace address_space{};
    if (!Memory::vmm::create_user_address_space(&address_space))
    {
        log(ERROR, "NAPP", "Unable to create user address space");
        return false;
    }

    const uint64_t code_address = address_space.base + USER_IMAGE_OFFSET;
    const uint32_t page_count =
        static_cast<uint32_t>((image_size + PAGE_SIZE - 1) / PAGE_SIZE);
    for (uint32_t page_index = 0; page_index < page_count; ++page_index)
    {
        const uint64_t physical_page = Memory::pmm::alloc_page();
        if (physical_page == 0)
        {
            Memory::vmm::destroy_user_address_space(&address_space);
            log(ERROR, "NAPP", "Unable to allocate user image page");
            return false;
        }

        const uint64_t virtual_page = code_address + page_index * PAGE_SIZE;
        if (!Memory::vmm::map_user_page(&address_space, virtual_page, physical_page,
                                        Memory::vmm::PAGE_USER))
        {
            Memory::pmm::free_page(physical_page);
            Memory::vmm::destroy_user_address_space(&address_space);
            log(ERROR, "NAPP", "Unable to map user image page");
            return false;
        }

        auto* destination = reinterpret_cast<void*>(
            Memory::paging::get_hhdm_offset() + physical_page);
        Memory::memset(destination, 0, PAGE_SIZE);
        const uint32_t image_offset = page_index * PAGE_SIZE;
        const uint32_t copy_size =
            image_size - image_offset < PAGE_SIZE ? image_size - image_offset : PAGE_SIZE;
        Memory::memcpy(destination,
                       static_cast<const uint8_t*>(image) + image_offset,
                       copy_size);
    }

    const uint64_t stack_page = Memory::pmm::alloc_page();
    if (stack_page == 0)
    {
        Memory::vmm::destroy_user_address_space(&address_space);
        log(ERROR, "NAPP", "Unable to allocate user stack page");
        return false;
    }

    const uint64_t stack_address = address_space.base + USER_STACK_OFFSET;
    if (!Memory::vmm::map_user_page(&address_space, stack_address, stack_page,
                                    Memory::vmm::PAGE_USER | Memory::vmm::PAGE_WRITE))
    {
        Memory::pmm::free_page(stack_page);
        Memory::vmm::destroy_user_address_space(&address_space);
        log(ERROR, "NAPP", "Unable to map user stack page");
        return false;
    }
    Memory::memset(reinterpret_cast<void*>(
                       Memory::paging::get_hhdm_offset() + stack_page),
                   0, PAGE_SIZE);

    if (Scheduler::create_user_task(&address_space, code_address + entry_offset,
                                    stack_address + PAGE_SIZE, process_id) == 0)
    {
        Memory::vmm::destroy_user_address_space(&address_space);
        log(ERROR, "NAPP", "Unable to schedule user application");
        return false;
    }

    return true;
}

static bool remember_image(const char* name, uint32_t process_id, void* image)
{
    for (uint32_t i = 0; i < NAPP_MAX_WINDOWS; i++)
    {
        if (!resident_images[i].used)
        {
            resident_images[i].used = true;
            resident_images[i].image = image;
            resident_images[i].process_id = process_id;

            strcpy(resident_images[i].name, name);

            return true;
        }
    }

    return false;
}

void napp_process_window_closed(uint32_t process_id)
{
    if (process_id == 0 || Process::window_count(process_id) != 0)
    {
        return;
    }

    for (uint32_t index = 0; index < NAPP_MAX_WINDOWS; ++index)
    {
        if (!resident_images[index].used || resident_images[index].process_id != process_id)
        {
            continue;
        }

        const char* application_name = resident_images[index].name;
        for (uint32_t slot_index = 0; slot_index < NAPP_MAX_WINDOWS; ++slot_index)
        {
            if (window_slots[slot_index].used &&
                strcmp(window_slots[slot_index].owner, application_name) == 0)
            {
                window_slots[slot_index].used = false;
            }
        }

        Memory::heap::kfree(resident_images[index].image);
        resident_images[index] = {};
        return;
    }
}

static void build_application_path(const char* name, char* output)
{
    strcpy(output, NAPP_DIRECTORY);
    strcat(output, "/");
    strcat(output, name);
}

void napp_init(const void* rootfs_image, uint64_t rootfs_size)
{
    Uart::puts("[NAPP] Initializing...\n");
    log(INFO, "NAPP", "Initializing...");

    rootfs_mounted = false;

    if (rootfs_image == nullptr || rootfs_size == 0)
    {
        Uart::puts("[NAPP] No rootfs module provided\n");
        log(WARN, "NAPP", "No rootfs module provided");

        return;
    }

    if (!fat_mount(&rootfs_volume, rootfs_image, rootfs_size))
    {
        Uart::puts("[NAPP] Failed to mount rootfs\n");
        log(ERROR, "NAPP", "Failed to mount rootfs");

        return;
    }

    rootfs_mounted = true;

    Uart::puts("[NAPP] Rootfs mounted\n");
    log(INFO, "NAPP", "Rootfs mounted");
    
    // Initialize file resolver
    file_resolver_init();
}

bool napp_rootfs_available()
{
    return rootfs_mounted;
}

uint32_t napp_list(char names[][NAPP_MAX_NAME], uint32_t max_names)
{
    if (!rootfs_mounted || names == nullptr || max_names == 0)
    {
        return 0;
    }

    static fat_entry_info entries[NAPP_MAX_APPLICATIONS];

    uint32_t entry_count = fat_list_directory(&rootfs_volume, NAPP_DIRECTORY, entries, NAPP_MAX_APPLICATIONS);

    uint32_t found = 0;

    for (uint32_t i = 0; i < entry_count && found < max_names; i++)
    {
        // Skip directories and special files
        if (entries[i].directory || entries[i].name[0] == '.')
        {
            continue;
        }

        // Copy the filename directly (flat binaries in /bin)
        uint32_t j = 0;
        while (entries[i].name[j] && j < NAPP_MAX_NAME - 1)
        {
            names[found][j] = entries[i].name[j];
            j++;
        }
        names[found][j] = '\0';
        found++;
    }

    return found;
}

// Reads the NAPP header of the named /bin application and returns the value of
// its show_in_start_menu field.  Applications compiled with an older header
// format (header_size <= NAPP_HEADER_SIZE) default to true so they remain
// visible in the Start Menu.
bool napp_should_show_in_start_menu(const char* name)
{
    if (!rootfs_mounted || name == nullptr || *name == '\0')
    {
        return false;
    }

    char path[256];
    build_application_path(name, path);

    fat_entry_info info;
    if (!fat_stat(&rootfs_volume, path, &info) || info.directory)
    {
        return true;
    }

    if (info.size < NAPP_HEADER_SIZE)
    {
        return true;
    }

    // fat_read_file rejects reads where buffer_size < file_size, so allocate
    // a buffer large enough for the entire file (we only need the header but
    // must read the whole image to extract it).
    uint8_t* header_buf = (uint8_t*)Memory::heap::kmalloc(info.size);

    if (header_buf == nullptr)
    {
        return true;
    }

    uint32_t read_size = 0;

    if (!fat_read_file(&rootfs_volume, path, header_buf, info.size, &read_size))
    {
        Memory::heap::kfree(header_buf);
        return true;
    }

    if (read_size < NAPP_HEADER_SIZE)
    {
        Memory::heap::kfree(header_buf);
        return true;
    }

    const napp_header* header = (const napp_header*)header_buf;

    bool result = true;

    if (header->magic == NAPP_MAGIC && napp_abi_supported(header->abi_version))
    {
        // If the header carries the show_in_start_menu field, honour it.
        // The field lives at offset NAPP_HEADER_SIZE (96); headers whose
        // header_size <= NAPP_HEADER_SIZE lack it, so we default to true
        // (backward compat).
        if (header->header_size > NAPP_HEADER_SIZE)
        {
            result = header->show_in_start_menu;
        }
    }

    Memory::heap::kfree(header_buf);

    return result;
}

// Iterates all registered NAPP window slots and invokes the tick callback
// of each window whose configured interval has elapsed.  Must be called from
// the kernel main loop (100 Hz) alongside the rest of the GUI update.
void napp_update_ticks()
{
    uint64_t current_ticks = Timer::pit_get_ticks();

    for (uint32_t i = 0; i < NAPP_MAX_WINDOWS; i++)
    {
        napp_window_slot* slot = &window_slots[i];

        if (!slot->used || !slot->window.visible || slot->tick == nullptr || slot->tick_interval_ms <= 0)
        {
            continue;
        }

        uint64_t ticks_per_interval = (uint64_t)slot->tick_interval_ms / 10;
        if (ticks_per_interval == 0)
        {
            ticks_per_interval = 1;
        }

        if (current_ticks - slot->last_tick >= ticks_per_interval)
        {
            slot->last_tick = current_ticks;
            napp_window_tick_trampoline(&slot->window);
        }
    }
}

bool napp_exists(const char* name)
{
    if (!rootfs_mounted || name == nullptr || *name == '\0')
    {
        return false;
    }

    char path[256];

    build_application_path(name, path);

    fat_entry_info info;

    if (!fat_stat(&rootfs_volume, path, &info))
    {
        return false;
    }

    return !info.directory;
}

bool napp_run(const char* name, int* exit_code)
{
    if (!rootfs_mounted || name == nullptr || *name == '\0')
    {
        return false;
    }

    char path[256];

    build_application_path(name, path);

    fat_entry_info info;

    if (!fat_stat(&rootfs_volume, path, &info) || info.directory)
    {
        log(WARN, "NAPP", "Application not found");

        return false;
    }

    if (info.size == 0 || info.size > NAPP_MAX_IMAGE_SIZE)
    {
        log(ERROR, "NAPP", "Invalid application size");

        return false;
    }

    void* image = Memory::heap::kmalloc(info.size);

    if (image == nullptr)
    {
        log(ERROR, "NAPP", "Out of memory");

        return false;
    }

    uint32_t read_size = 0;

    if (!fat_read_file(&rootfs_volume, path, image, info.size, &read_size) || read_size != info.size)
    {
        log(ERROR, "NAPP", "Failed to read application");

        Memory::heap::kfree(image);

        return false;
    }

    napp_window_slot* existing = find_window_of(name);
    if (existing != nullptr)
    {
        if (existing->window.process_id == 0)
        {
            existing->window.process_id = Process::start(name, Process::Type::Application);
            if (existing->window.process_id == 0)
            {
                Memory::heap::kfree(image);
                return false;
            }
        }
        Process::set_running(existing->window.process_id);
        show_window(existing);

        if (exit_code != nullptr)
        {
            *exit_code = 0;
        }

        Memory::heap::kfree(image);
        return true;
    }

    const uint32_t process_id = Process::start(name, Process::Type::Application);
    if (process_id == 0)
    {
        Memory::heap::kfree(image);
        return false;
    }

    const napp_header* header = (const napp_header*)image;
    const bool has_napp_header = header->magic == NAPP_MAGIC && napp_abi_supported(header->abi_version);
    const uint32_t entry_offset = has_napp_header ? napp_entry_offset(header) : header->entry_offset;
    if (has_napp_header && !napp_user_mode_metadata_valid(header, info.size))
    {
        log(ERROR, "NAPP", "Invalid ABI 12 execution-mode metadata");
        Process::finish(process_id, 1);
        Memory::heap::kfree(image);
        return false;
    }
    if (has_napp_header && (entry_offset < header->header_size || entry_offset >= info.size))
    {
        log(ERROR, "NAPP", "Invalid application entry point");
        Process::finish(process_id, 1);
        Memory::heap::kfree(image);
        return false;
    }

    if (napp_header_is_user_mode(header, info.size))
    {
        const bool launched = napp_launch_user_image(image, info.size, entry_offset, process_id);
        Memory::heap::kfree(image);
        if (!launched)
        {
            Process::finish(process_id, 1);
            return false;
        }

        Uart::puts("[NAPP] User-mode application scheduled: ");
        Uart::puts(name);
        Uart::puts("\n");
        if (exit_code != nullptr)
        {
            *exit_code = 0;
        }
        return true;
    }

    Uart::puts("[NAPP] Starting application: ");
    Uart::puts(name);
    Uart::puts("\n");
    log(INFO, "NAPP", "Starting application");

    strcpy(loading_application, name);
    loading_window_count = 0;
    loading_process_id = process_id;
    loading_napp_abi_version = header->magic == NAPP_MAGIC && napp_abi_supported(header->abi_version) ? header->abi_version : NAPP_ABI_VERSION;
    kernel_napp_api.abi_version = loading_napp_abi_version;

    int result = 0;

    // Check for NAPP header (graphical applications)
    if (header->magic == NAPP_MAGIC && napp_abi_supported(header->abi_version))
    {
        napp_entry entry = (napp_entry)((uint8_t*)image + entry_offset);
        result = entry(&kernel_napp_api);
    }
    else
    {
        // Flat binary (console applications)
        typedef int (*flat_entry)(const napp_api*);
        flat_entry entry = (flat_entry)image;
        result = entry(&kernel_napp_api);
    }

    uint32_t opened_windows = loading_window_count;

    loading_application[0] = '\0';
    loading_window_count = 0;
    loading_process_id = 0;
    loading_napp_abi_version = NAPP_ABI_VERSION;
    kernel_napp_api.abi_version = NAPP_ABI_VERSION;

    if (exit_code != nullptr)
    {
        *exit_code = result;
    }

    // A graphical application keeps running through its window callbacks, so
    // its image has to stay mapped.
    if (opened_windows == 0)
    {
        Process::finish(process_id, result);
        Memory::heap::kfree(image);
    }
    else if (remember_image(name, process_id, image))
    {
        Process::set_running(process_id);
    }
    else
    {
        log(ERROR, "NAPP", "Unable to retain application image for open windows");
        Process::finish(process_id, 1);
        Memory::heap::kfree(image);
    }

    Uart::puts("[NAPP] Application finished\n");
    log(INFO, "NAPP", "Application finished");

    return true;
}

// Path-based flat-binary loader (for /bin/<name> and /sbin/<name>)

bool napp_exists_path(const char* path)
{
    if (path == nullptr || *path == '\0')
    {
        return false;
    }

    // Use file resolver for unified path checking
    return system_file_exists(path);
}

bool napp_run_path(const char* path, int argc, const char* const* argv, int* exit_code)
{
    if (path == nullptr || *path == '\0')
    {
        return false;
    }

    // Use file resolver to determine source and load accordingly
    file_resolve_result_t resolve = resolve_system_file(path);
    
    if (!resolve.exists) 
    {
        log(WARN, "NAPP", "Path not found");
        return false;
    }

    void* image = nullptr;
    uint32_t image_size = 0;

    if (resolve.source == FILE_SOURCE_CLAWFS) 
    {
        // Load from ClawFS
        image_size = resolve.size;
        if (image_size == 0 || image_size > NAPP_MAX_IMAGE_SIZE)
        {
            log(ERROR, "NAPP", "Invalid ClawFS application size");
            return false;
        }
        const uint32_t sectors_to_read = (image_size + 511) / 512;
        const uint32_t allocation_size = sectors_to_read * 512;
        image = Memory::heap::kmalloc(allocation_size);
        
        if (image == nullptr) 
        {
            log(ERROR, "NAPP", "Out of memory");
            return false;
        }

        Uart::puts("[NAPP] ClawFS load - image allocated at: ");
        Uart::puthex((uint64_t)image);
        Uart::puts(" size: ");
        Uart::puthex(image_size);
        Uart::puts(" sector: ");
        Uart::puthex(resolve.data_sector);
        Uart::puts("\n");

        // Read the file - may span multiple sectors
        uint8_t* image_ptr = (uint8_t*)image;
        
        for (uint32_t i = 0; i < sectors_to_read; i++) 
        {
            if (!Disk::read_sector(resolve.data_sector + i, image_ptr + (i * 512))) 
            {
                log(ERROR, "NAPP", "Failed to read from ClawFS");
                Memory::heap::kfree(image);
                return false;
            }
        }
        
        // Debug: dump first 16 bytes of loaded binary
        Uart::puts("[NAPP] Binary dump: ");
        uint8_t* img_bytes = (uint8_t*)image;
        for(uint32_t i = 0; i < 16 && i < image_size; i++) 
        {
            Uart::puthex(img_bytes[i]);
            Uart::puts(" ");
        }
        Uart::puts("\n");
        
        Uart::puts("[NAPP] Running from ClawFS: ");
        Uart::puts(path);
        Uart::puts("\n");
    }
    else {
        // Load from rootfs (FAT)
        fat_entry_info info;
        if (!fat_stat(&rootfs_volume, path, &info) || info.directory) 
        {
            log(WARN, "NAPP", "Path not found in rootfs");
            return false;
        }

        if (info.size == 0 || info.size > NAPP_MAX_IMAGE_SIZE) 
        {
            log(ERROR, "NAPP", "Invalid flat binary size");
            return false;
        }

        image_size = info.size;
        image = Memory::heap::kmalloc(image_size);

        if (image == nullptr) 
        {
            log(ERROR, "NAPP", "Out of memory");
            return false;
        }

        uint32_t read_size = 0;
        if (!fat_read_file(&rootfs_volume, path, image, image_size, &read_size) || read_size != image_size) 
        {
            log(ERROR, "NAPP", "Failed to read flat binary");
            Memory::heap::kfree(image);

            return false;
        }
        
        Uart::puts("[NAPP] Running from rootfs: ");
        Uart::puts(path);
        Uart::puts("\n");
    }

    char process_name[Process::PROCESS_NAME_LENGTH];
    const char* name_start = path;
    for (const char* cursor = path; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '/')
        {
            name_start = cursor + 1;
        }
    }
    uint32_t name_length = 0;
    while (name_start[name_length] != '\0' && name_start[name_length] != '.' && name_length < Process::PROCESS_NAME_LENGTH - 1)
    {
        process_name[name_length] = name_start[name_length];
        ++name_length;
    }
    process_name[name_length] = '\0';
    const uint32_t process_id = Process::start(process_name, Process::Type::Command);
    if (process_id == 0)
    {
        Memory::heap::kfree(image);
        return false;
    }

    int result = 0;

    // Check for NAPP header (sbin commands are NAPP binaries without extension)
    const napp_header* header = (const napp_header*)image;
    
    Uart::puts("[NAPP] Checking header at: ");
    Uart::puthex((uint64_t)header);
    Uart::puts(" magic: ");
    Uart::puthex(header->magic);
    Uart::puts(" expected: ");
    Uart::puthex(NAPP_MAGIC);
    Uart::puts("\n");
    
    if (header->magic == NAPP_MAGIC && napp_abi_supported(header->abi_version))
    {
        if (!napp_user_mode_metadata_valid(header, image_size))
        {
            log(ERROR, "NAPP", "Invalid ABI 12 execution-mode metadata");
            Process::finish(process_id, 1);
            Memory::heap::kfree(image);
            return false;
        }

        const uint32_t entry_offset = napp_entry_offset(header);
        if (entry_offset < header->header_size || entry_offset >= image_size)
        {
            log(ERROR, "NAPP", "Invalid application entry point");
            Process::finish(process_id, 1);
            Memory::heap::kfree(image);
            return false;
        }

        if (napp_header_is_user_mode(header, image_size))
        {
            if (argc != 0)
            {
                log(WARN, "NAPP", "User-mode applications do not support command arguments yet");
                Process::finish(process_id, 1);
                Memory::heap::kfree(image);
                return false;
            }

            const bool launched =
                napp_launch_user_image(image, image_size, entry_offset, process_id);
            Memory::heap::kfree(image);
            if (!launched)
            {
                Process::finish(process_id, 1);
                return false;
            }

            if (exit_code != nullptr)
            {
                *exit_code = 0;
            }
            return true;
        }

        kernel_napp_api.argc = argc;
        kernel_napp_api.argv = argv;
        loading_napp_abi_version = header->abi_version;
        kernel_napp_api.abi_version = header->abi_version;
        napp_entry entry = (napp_entry)((uint8_t*)image + entry_offset);
        
        Uart::puts("[NAPP] NAPP entry point: ");
        Uart::puthex((uint64_t)entry);
        Uart::puts("\n");
        
        result = entry(&kernel_napp_api);
    }
    else
    {
        // Flat binary (no header)
        kernel_napp_api.argc = argc;
        kernel_napp_api.argv = argv;
        loading_napp_abi_version = NAPP_ABI_VERSION;
        kernel_napp_api.abi_version = NAPP_ABI_VERSION;
        typedef int (*flat_entry)(const napp_api*);
        flat_entry entry = (flat_entry)image;
        
        Uart::puts("[NAPP] Flat binary entry point: ");
        Uart::puthex((uint64_t)entry);
        Uart::puts("\n");
        
        result = entry(&kernel_napp_api);
    }

    // Reset argc/argv after return
    kernel_napp_api.argc = 0;
    kernel_napp_api.argv = nullptr;
    loading_napp_abi_version = NAPP_ABI_VERSION;
    kernel_napp_api.abi_version = NAPP_ABI_VERSION;

    Memory::heap::kfree(image);
    Process::finish(process_id, result);

    if (exit_code != nullptr)
    {
        *exit_code = result;
    }

    return true;
}

// sbin listing

#define NAPP_SBIN_DEFAULT_DESC "No description available."

static void copy_napp_text(char* destination, uint32_t destination_size, const char* source, uint32_t source_size)
{
    if (destination_size == 0)
    {
        return;
    }

    uint32_t i = 0;
    while (i + 1 < destination_size && i < source_size && source[i] != '\0')
    {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

// Read the NAPP description and optional command help text from the FAT rootfs.
static bool read_sbin_metadata_fat(const char* path, char* desc_buffer, uint32_t desc_size, char* help_buffer, uint32_t help_size)
{
    if (help_size > 0)
    {
        help_buffer[0] = '\0';
    }

    fat_entry_info info;
    if (!fat_stat(&rootfs_volume, path, &info) || info.directory)
    {
        return false;
    }

    if (info.size < NAPP_HEADER_SIZE)
    {
        return false;
    }

    void* image = Memory::heap::kmalloc(info.size);
    if (image == nullptr)
    {
        return false;
    }

    uint32_t read_size = 0;
    bool ok = fat_read_file(&rootfs_volume, path, image, info.size, &read_size);

    bool found = false;
    if (ok && read_size >= NAPP_HEADER_SIZE)
    {
        const napp_header* h = (const napp_header*)image;

        if (h->magic == NAPP_MAGIC && napp_abi_supported(h->abi_version) && h->header_size >= NAPP_HEADER_SIZE && h->header_size <= read_size)
        {
            copy_napp_text(desc_buffer, desc_size, h->description, sizeof(h->description));
            if (h->abi_version >= 8 && h->header_size >= sizeof(napp_header) && read_size >= sizeof(napp_header))
            {
                copy_napp_text(help_buffer, help_size, h->help_text, sizeof(h->help_text));
            }
            found = true;
        }
    }

    Memory::heap::kfree(image);
    return found;
}

// Read NAPP command metadata from a ClawFS file stored in its first sector.
static bool read_sbin_metadata_clawfs(uint32_t data_sector, char* desc_buffer, uint32_t desc_size, char* help_buffer, uint32_t help_size)
{
    if (help_size > 0)
    {
        help_buffer[0] = '\0';
    }

    uint8_t sector_buf[512];

    if (!Disk::read_sector(data_sector, sector_buf))
    {
        return false;
    }

    const napp_header* h = (const napp_header*)sector_buf;

    if (h->magic == NAPP_MAGIC && napp_abi_supported(h->abi_version) && h->header_size >= NAPP_HEADER_SIZE && h->header_size <= sizeof(sector_buf))
    {
        copy_napp_text(desc_buffer, desc_size, h->description, sizeof(h->description));
        if (h->abi_version >= 8 && h->header_size >= sizeof(napp_header))
        {
            copy_napp_text(help_buffer, help_size, h->help_text, sizeof(h->help_text));
        }
        return true;
    }

    return false;
}

uint32_t napp_list_sbin(char names[][NAPP_MAX_NAME], char descriptions[][NAPP_MAX_NAME], char help_texts[][NAPP_HELP_TEXT_LENGTH], uint32_t max_names)
{
    if (!rootfs_mounted || names == nullptr || descriptions == nullptr ||
        help_texts == nullptr || max_names == 0)
    {
        return 0;
    }

    static fat_entry_info files[NAPP_MAX_APPLICATIONS];

    uint32_t file_count = fat_list_directory(&rootfs_volume, NAPP_SBIN_DIRECTORY, files, NAPP_MAX_APPLICATIONS);
    uint32_t found = 0;

    for (uint32_t i = 0; i < file_count && found < max_names; i++)
    {
        if (files[i].directory)
        {
            continue;
        }

        // Skip hidden/internal entries (names starting with '.')
        if (files[i].name[0] == '.')
        {
            continue;
        }

        // Copy name directly — flat binaries have no extension
        uint32_t j = 0;
        while (files[i].name[j] && j < NAPP_MAX_NAME - 1)
        {
            names[found][j] = files[i].name[j];
            j++;
        }
        names[found][j] = '\0';

        // Read command metadata from the NAPP header on the FAT rootfs
        char sbin_path[64];
        strcpy(sbin_path, NAPP_SBIN_DIRECTORY);
        strcat(sbin_path, "/");
        strcat(sbin_path, files[i].name);

        if (!read_sbin_metadata_fat(sbin_path, descriptions[found], NAPP_MAX_NAME, help_texts[found], NAPP_HELP_TEXT_LENGTH) || descriptions[found][0] == '\0')
        {
            strcpy(descriptions[found], NAPP_SBIN_DEFAULT_DESC);
            help_texts[found][0] = '\0';
        }

        found++;
    }

    // When the ClawFS overlay is mounted, also scan /sbin on ClawFS for
    // commands that were added there (not present in the ISO rootfs).
    if (file_resolver_is_mounted() && clawfs_exists())
    {
        uint32_t sbin_sector = get_sector_by_path(NAPP_SBIN_DIRECTORY);
        if (sbin_sector != 0)
        {
            uint8_t dir_buffer[512];
            if (Disk::read_sector(sbin_sector, dir_buffer))
            {
                CLAWFSEntry* entries = (CLAWFSEntry*)dir_buffer;

                for (int j = 0; j < 12 && found < max_names; j++)
                {
                    if (entries[j].name[0] == '\0' || entries[j].type != CLAWFS_FILE || entries[j].name[0] == '.')
                    {
                        continue;
                    }


                    // Skip files marked as deleted via tombstones
                    char tombstone_path[256];
                    strcpy(tombstone_path, NAPP_SBIN_DIRECTORY);
                    strcat(tombstone_path, "/");
                    strcat(tombstone_path, entries[j].name);
                    if (file_resolver_is_deleted(tombstone_path))
                    {
                        continue;
                    }

                    // Skip if already listed from FAT
                    bool already_listed = false;
                    for (uint32_t k = 0; k < found; k++)
                    {
                        if (strcmp(names[k], entries[j].name) == 0)
                        {
                            already_listed = true;
                            break;
                        }
                    }

                    if (already_listed)
                    {
                        continue;
                    }

                    // Copy name
                    uint32_t m = 0;
                    while (entries[j].name[m] && m < NAPP_MAX_NAME - 1)
                    {
                        names[found][m] = entries[j].name[m];
                        m++;
                    }
                    names[found][m] = '\0';

                    // Read command metadata from the NAPP header on ClawFS
                    if (!read_sbin_metadata_clawfs(entries[j].data_sector, descriptions[found], NAPP_MAX_NAME, help_texts[found], NAPP_HELP_TEXT_LENGTH) || descriptions[found][0] == '\0')
                    {
                        // Fallback: try reading from the FAT rootfs
                        char fat_path[64];
                        strcpy(fat_path, NAPP_SBIN_DIRECTORY);
                        strcat(fat_path, "/");
                        strcat(fat_path, entries[j].name);
                        if (!read_sbin_metadata_fat(fat_path, descriptions[found], NAPP_MAX_NAME, help_texts[found], NAPP_HELP_TEXT_LENGTH) || descriptions[found][0] == '\0')
                        {
                            strcpy(descriptions[found], NAPP_SBIN_DEFAULT_DESC);
                            help_texts[found][0] = '\0';
                        }
                    }

                    found++;
                }
            }
        }
    }

    return found;
}

const char* napp_get_current_path(void)
{
    return napp_current_path;
}

void napp_set_current_path(const char* path)
{
    if (path != nullptr)
    {
        uint32_t i = 0;
        while (path[i] && i < 255) 
        { 
            napp_current_path[i] = path[i]; i++; 
        }

        napp_current_path[i] = '\0';
    }
}
