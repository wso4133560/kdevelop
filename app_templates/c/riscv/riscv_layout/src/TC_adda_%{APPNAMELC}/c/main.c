#include "project_config.h"

volatile unsigned int g_project_heartbeat = 0;

int main(void)
{
    for (;;) {
        ++g_project_heartbeat;
    }

    return 0;
}
