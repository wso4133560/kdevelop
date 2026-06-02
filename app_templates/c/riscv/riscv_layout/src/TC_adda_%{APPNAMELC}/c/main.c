#include "project_config.h"

extern void rpu_thu_printf(const char *fmt, ...);

volatile unsigned int g_project_heartbeat = 0;

int main(void)
{
    drv_clk_init();
    rpu_thu_uart_init();
    rpu_thu_printf("hello world\r\n");

    return 0;
}
