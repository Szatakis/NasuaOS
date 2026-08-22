#include "../driver.hpp"

#ifdef __x86_64__

#include "drivers/mouse/driver.hpp"

#include "system/gui/vars/colors.hpp"

namespace Gpu 
{
    void handle_mouse()
    {
        if (!fb)
        {
            return;
        }

        uint32_t* bb_ptr = get_backbuffer();
        size_t bb_pitch = get_backbuffer_pitch();

        for (int y = 0; y < CURSOR_H; y++)
        {
            for (int x = 0; x < CURSOR_W; x++)
            {
                int px = Mouse::x + x;
                int py = Mouse::y + y;

                if (px < 0 || py < 0)
                {
                    continue;
                }

                if ((size_t)px >= fb->width || (size_t)py >= fb->height)
                {
                    continue;
                }

                char p = Mouse::arrow_cursor[y][x];
                uint32_t color = 0;

                if (p == 'W')
                {
                    color = COLOR_MOUSE;
                }
                else if (p == 'B')
                {
                    color = 0x00000000;
                }
                else
                {
                    continue;
                }

                bb_ptr[py * bb_pitch + px] = color;
            }
        }
    }
} // namespace Gpu

#endif // __x86_64__
