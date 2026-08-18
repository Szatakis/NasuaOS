#include <napp.h>

NAPP_APPLICATION("pwd");

int _start(const napp_api* api)
{
    api->print(api->current_path);
    api->print("\n");
    return 0;
}
