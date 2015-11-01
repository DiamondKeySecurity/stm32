/* A wrapper for test programs that contain main() (currently libhal/tests).
 * We compile them with -Dmain=__main, so we can do stm setup first.
 */

#include "stm-init.h"
#include "stm-fmc.h"
#include "stm-uart.h"

extern void __main(void);

void main(void)
{
    stm_init();
    fmc_init();
    __main();
    uart_send_string("Done.\r\n\r\n");
}
