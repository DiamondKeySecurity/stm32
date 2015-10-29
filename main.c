/* A wrapper for test programs that contain main() (currently libhal/tests).
 * We compile them with -Dmain=__main, so we can do stm setup first.
 */

#include "stm-init.h"
#include "stm-uart.h"

void main(void)
{
  stm_init();
  fmc_init();
  while(1) {
    __main();
    uart_send_string("\r\n");
    HAL_Delay(2000);
  }
}
