/*
 * Test code that just sends the letters 'a' to 'z' over and
 * over again using USART2.
 *
 * Toggles the BLUE LED slowly and the RED LED for every
 * character sent.
 */
#include "stm32f4xx_hal.h"
#include "stm-init.h"
#include "stm-led.h"
#include "stm-uart.h"

#define DELAY() HAL_Delay(100)

int
main()
{
  uint8_t c = 'a';

  stm_init();

  while (1)
  {
    HAL_GPIO_TogglePin(LED_PORT, LED_GREEN);

    uart_send_char(c);
    DELAY();

    if (c++ == 'z') {
      c = 'a';
      HAL_GPIO_TogglePin(LED_PORT, LED_BLUE);
    }
  }
}
