#include <napp.h>

NAPP_APPLICATION("bootcheck");

int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION)
    {
        return 1;
    }

    api->serial_log("[BOOTCHECK] Application started\n");

    api->print_line("NasuaOS boot check");
    api->print_line("------------------");
    api->print_line("Application image: loaded from /bin/bootcheck/bootcheck.napp");
    api->print("Application ABI:    ");
    api->print_dec(api->abi_version);
    api->print("\n");
    api->print_line("Boot check passed.");

    api->serial_log("[BOOTCHECK] Application finished\n");

    return 0;
}
