#include <napp.h>

NAPP_APPLICATION("whoami", "Display the current user", false, "");

int _start(const napp_api* api)
{
    if (api->argc != 1)
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: whoami\n");
        return 1;
    }

    if (api->abi_version < 9 || api->get_username == nullptr)
    {
        api->print_error("whoami: current-user information is unavailable.\n");
        return 1;
    }

    const char* username = api->get_username();
    if (username == nullptr || username[0] == '\0')
    {
        api->print_error("whoami: no authenticated user.\n");
        return 1;
    }

    api->print(username);
    api->print("\n");
    return 0;
}
