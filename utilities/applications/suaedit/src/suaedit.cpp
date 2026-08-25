#include <napp.h>

NAPP_APPLICATION("suaedit", "Text editor", true);

int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION)
    {
        return 1;
    }

    return 0;
}
