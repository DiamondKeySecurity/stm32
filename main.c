/* A wrapper for test programs that contain main() (currently libhal/tests).
 * We compile them with -Dmain=__main, so we can do stm setup first.
 */

#include "stm-init.h"
#include "stm-led.h"
#include "stm-fmc.h"
#include "stm-uart.h"

extern void __main(void);

int main(void)
{
    stm_init();

    // Blink blue LED for six seconds to not upset the Novena at boot.
    led_on(LED_BLUE);
    for (int i = 0; i < 12; i++) {
	HAL_Delay(500);
	led_toggle(LED_BLUE);
    }
    fmc_init();
    led_off(LED_BLUE);
    led_on(LED_GREEN);

    __main();

    uart_send_string("Done.\r\n\r\n");
    return 0;
}
