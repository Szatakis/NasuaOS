#include <napp.h>
#include <nasua_syscall.h>

NAPP_USER_APPLICATION("bootcheck", "Ring-3 syscall smoke test", true, "System");

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

    nasua_user_exit(0);
}
