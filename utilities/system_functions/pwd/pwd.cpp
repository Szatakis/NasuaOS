#include <napp.h>

NAPP_APPLICATION("pwd", "Print the working directory");

int _start(const napp_api* api)
{
    // Check for unexpected arguments
    if (api->argc > 1)
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: pwd\n");
        return 1;
    }

    api->print(api->current_path);
    api->print("\n");
    return 0;
}
