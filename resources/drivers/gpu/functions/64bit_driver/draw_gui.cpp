#include "../../driver.hpp"

#include "drivers/rtc/driver.hpp"
#include "drivers/mouse/driver.hpp"
#include "system/sysfunc/notification_manager/notification_manager.hpp"
#include "system/gui/icons/icons.hpp"
#include "system/gui/vars/colors.hpp"
#include "system/gui/gui.hpp"

namespace Gpu
{
    void update_start()
    {
        if (!fb)
        {
            return;
        }
        draw_start_button(6, fb->height - bar_h_scaled);
    }

    void update_time()
    {
        if (!fb)
        {
            return;
        }

        Rtc::RtcTime time = Rtc::get_time();
        char time_buf[9];
        char date_buf[11];

        time_buf[0] = '0' + (time.hour / 10);
        time_buf[1] = '0' + (time.hour % 10);
        time_buf[2] = ':';
        time_buf[3] = '0' + (time.minute / 10);
        time_buf[4] = '0' + (time.minute % 10);
        time_buf[5] = ':';
        time_buf[6] = '0' + (time.second / 10);
        time_buf[7] = '0' + (time.second % 10);
        time_buf[8] = '\0';

        date_buf[0] = '0' + (time.day / 10);
        date_buf[1] = '0' + (time.day % 10);
        date_buf[2] = '-';
        date_buf[3] = '0' + (time.month / 10);
        date_buf[4] = '0' + (time.month % 10);
        date_buf[5] = '-';
        date_buf[6] = '0' + (time.year / 1000);
        date_buf[7] = '0' + ((time.year / 100) % 10);
        date_buf[8] = '0' + ((time.year / 10) % 10);
        date_buf[9] = '0' + (time.year % 10);
        date_buf[10] = '\0';

        const size_t text_x = fb->width - 105;
        const size_t start_y = fb->height - bar_h_scaled + 2;
        print_at8(time_buf, text_x, start_y + 8, COLOR_WHITE);
        print_at8(date_buf, text_x, start_y + 20, COLOR_WHITE);
    }

    void update_bottom_bar()
    {
        if (!fb)
        {
            return;
        }

        const size_t start_y = fb->height - bar_h_scaled + 2;
        const size_t start_x = fb->width;
        draw_icon<32, 32>(speaker_icon, start_x - 175,
                          start_y + ((bar_h_scaled - 32) / 2));

        if (notification_count <= 0)
        {
            draw_icon<32, 32>(notification_icon, start_x - 145,
                              start_y + ((bar_h_scaled - 32) / 2));
        }
        else
        {
            draw_icon<32, 32>(notification_icon_h, start_x - 145,
                              start_y + ((bar_h_scaled - 32) / 2));
        }

        Window_Manager::draw_taskbar_entries();
        update_start();
        update_time();
    }

    void update_gui()
    {
        update_gui_state(Mouse::x, Mouse::y);
        update_bottom_bar();
        draw_text_buffer();
    }
}
