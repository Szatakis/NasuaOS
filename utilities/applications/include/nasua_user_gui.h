#pragma once

#include <stdint.h>

#include <napp.h>
#include <nasua_syscall.h>

enum nasua_user_gui_event_type
{
    NASUA_USER_GUI_EVENT_DRAW = 1,
    NASUA_USER_GUI_EVENT_KEY,
    NASUA_USER_GUI_EVENT_MOUSE,
    NASUA_USER_GUI_EVENT_CLOSE,
    NASUA_USER_GUI_EVENT_TICK
};

static constexpr int32_t NASUA_USER_GUI_MIN_TICK_INTERVAL_MS = 10;
static constexpr int32_t NASUA_USER_GUI_MAX_TICK_INTERVAL_MS = 60000;

struct nasua_user_gui_event
{
    uint32_t type;
    int32_t pos_x;
    int32_t pos_y;
    int32_t width;
    int32_t height;
    int32_t title_height;
    int32_t mouse_x;
    int32_t mouse_y;
    char key;
    uint8_t button;
    uint8_t pressed;
    uint8_t reserved;
};

static inline int64_t nasua_user_gui_open_window(const char* title,
                                                 int32_t width,
                                                 int32_t height,
                                                 bool resizable,
                                                 bool can_maximize)
{
    if (title == nullptr)
    {
        return -1;
    }

    uint64_t title_length = 0;
    while (title_length < NAPP_NAME_LENGTH && title[title_length] != '\0')
    {
        ++title_length;
    }
    if (title_length == 0 || title_length >= NAPP_NAME_LENGTH)
    {
        return -1;
    }

    const uint64_t flags = (resizable ? 1U : 0U) |
                           (can_maximize ? 2U : 0U);
    return nasua_syscall5(NASUA_SYSCALL_GUI_OPEN_WINDOW,
                          reinterpret_cast<uint64_t>(title), title_length,
                          static_cast<uint64_t>(width),
                          static_cast<uint64_t>(height), flags);
}

static inline int64_t nasua_user_gui_next_event(nasua_user_gui_event* event)
{
    return nasua_syscall5(NASUA_SYSCALL_GUI_NEXT_EVENT,
                          reinterpret_cast<uint64_t>(event), 0, 0, 0, 0);
}

static inline int64_t nasua_user_gui_set_tick_interval(int32_t interval_ms)
{
    if (interval_ms < 0 ||
        interval_ms > NASUA_USER_GUI_MAX_TICK_INTERVAL_MS ||
        (interval_ms > 0 &&
         interval_ms < NASUA_USER_GUI_MIN_TICK_INTERVAL_MS))
    {
        return -1;
    }

    return nasua_syscall5(NASUA_SYSCALL_GUI_SET_TICK_INTERVAL,
                          static_cast<uint64_t>(interval_ms), 0, 0, 0, 0);
}

static inline int64_t nasua_user_gui_begin_frame()
{
    return nasua_syscall5(NASUA_SYSCALL_GUI_BEGIN_FRAME, 0, 0, 0, 0, 0);
}

static inline int64_t nasua_user_gui_fill_block(int32_t x, int32_t y,
                                                uint32_t color,
                                                int32_t width, int32_t height)
{
    return nasua_syscall5(NASUA_SYSCALL_GUI_FILL_BLOCK,
                          static_cast<uint64_t>(static_cast<int64_t>(x)),
                          static_cast<uint64_t>(static_cast<int64_t>(y)),
                          color,
                          static_cast<uint64_t>(static_cast<int64_t>(width)),
                          static_cast<uint64_t>(static_cast<int64_t>(height)));
}

static inline int64_t nasua_user_gui_draw_text(const char* text,
                                               int32_t x, int32_t y,
                                               uint32_t color)
{
    if (text == nullptr)
    {
        return -1;
    }

    uint64_t length = 0;
    while (length < 64 && text[length] != '\0')
    {
        ++length;
    }
    if (length == 64)
    {
        return -1;
    }

    return nasua_syscall5(NASUA_SYSCALL_GUI_DRAW_TEXT,
                          reinterpret_cast<uint64_t>(text),
                          static_cast<uint64_t>(static_cast<int64_t>(x)),
                          static_cast<uint64_t>(static_cast<int64_t>(y)),
                          color, length);
}

static inline int64_t nasua_user_gui_end_frame()
{
    return nasua_syscall5(NASUA_SYSCALL_GUI_END_FRAME, 0, 0, 0, 0, 0);
}

static inline bool nasua_user_gui_run(const napp_window_config* config)
{
    if (config == nullptr || config->draw == nullptr ||
        nasua_user_gui_open_window(config->title, config->width, config->height,
                                   config->resizable, config->can_maximize) < 0)
    {
        return false;
    }
    if (config->tick != nullptr && config->tick_interval_ms > 0 &&
        nasua_user_gui_set_tick_interval(config->tick_interval_ms) < 0)
    {
        return false;
    }

    napp_window window = {};
    window.userdata = config->userdata;

    for (;;)
    {
        nasua_user_gui_event event = {};
        if (nasua_user_gui_next_event(&event) != 1)
        {
            return false;
        }

        window.pos_x = event.pos_x;
        window.pos_y = event.pos_y;
        window.width = event.width;
        window.height = event.height;
        window.title_height = event.title_height;

        switch (event.type)
        {
            case NASUA_USER_GUI_EVENT_DRAW:
                if (nasua_user_gui_begin_frame() < 0)
                {
                    return false;
                }
                config->draw(&window);
                if (nasua_user_gui_end_frame() < 0)
                {
                    return false;
                }
                break;
            case NASUA_USER_GUI_EVENT_KEY:
                if (config->key_event != nullptr)
                {
                    config->key_event(&window, event.key, event.pressed != 0);
                }
                else if (event.pressed != 0 && config->key != nullptr)
                {
                    config->key(&window, event.key);
                }
                if (config->draw != nullptr)
                {
                    if (nasua_user_gui_begin_frame() < 0)
                    {
                        return false;
                    }
                    config->draw(&window);
                    if (nasua_user_gui_end_frame() < 0)
                    {
                        return false;
                    }
                }
                break;
            case NASUA_USER_GUI_EVENT_MOUSE:
                if (config->mouse_button != nullptr)
                {
                    config->mouse_button(&window, event.mouse_x, event.mouse_y,
                                         event.button);
                }
                else if (event.button == 0 && config->mouse != nullptr)
                {
                    config->mouse(&window, event.mouse_x, event.mouse_y);
                }
                if (config->draw != nullptr)
                {
                    if (nasua_user_gui_begin_frame() < 0)
                    {
                        return false;
                    }
                    config->draw(&window);
                    if (nasua_user_gui_end_frame() < 0)
                    {
                        return false;
                    }
                }
                break;
            case NASUA_USER_GUI_EVENT_TICK:
                if (config->tick != nullptr)
                {
                    config->tick(&window);
                }
                if (nasua_user_gui_begin_frame() < 0)
                {
                    return false;
                }
                config->draw(&window);
                if (nasua_user_gui_end_frame() < 0)
                {
                    return false;
                }
                break;
            case NASUA_USER_GUI_EVENT_CLOSE:
                if (config->close != nullptr)
                {
                    config->close(&window);
                }
                nasua_user_exit(0);
            default:
                return false;
        }
    }
}
