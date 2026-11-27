#include <napp.h>
#include <nasua_user_gui.h>

NAPP_USER_APPLICATION("bootcheck", "Ring-3 syscall smoke test", true, "System");

static void bootcheck_draw(napp_window* window)
{
    if (nasua_user_gui_draw_text("Waiting for timer event",
                                 window->pos_x + 8,
                                 window->pos_y + window->title_height + 8,
                                 0x00FFFFFF) < 0)
    {
        nasua_user_exit(1);
    }
}

static void bootcheck_tick(napp_window*)
{
    static constexpr char message[] =
        "[BOOTCHECK] Ring-3 GUI timer event delivered\n";
    constexpr uint64_t message_length = sizeof(message) - 1;
    if (nasua_user_write(message, message_length) !=
        static_cast<int64_t>(message_length))
    {
        nasua_user_exit(1);
    }

    nasua_user_exit(0);
}

int _start(void)
{
    static constexpr char message[] =
        "[BOOTCHECK] NAPP ran in ring 3 and wrote through syscall\n";
    constexpr uint64_t message_length = sizeof(message) - 1;
    if (nasua_user_write(message, message_length) !=
        static_cast<int64_t>(message_length))
    {
        nasua_user_exit(1);
    }

    napp_window_config config = {};
    config.title = "Ring-3 Timer Check";
    config.width = 220;
    config.height = 160;
    config.draw = bootcheck_draw;
    config.tick = bootcheck_tick;
    config.tick_interval_ms = 20;
    if (!nasua_user_gui_run(&config))
    {
        nasua_user_exit(1);
    }

    nasua_user_exit(0);
}
