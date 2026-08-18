#include <napp.h>

NAPP_APPLICATION("pwd");

int _start(const napp_api* api)
{
    // Check for unexpected arguments
    if (api->argc > 1)
    {
        api->print("Syntax error: pwd does not take arguments\n");
        api->print("Usage: pwd\n");
        return 1;
    }

    api->print(api->current_path);
    api->print("\n");
    return 0;
}
