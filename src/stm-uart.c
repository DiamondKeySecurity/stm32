/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "stm-uart.h"

#include <string.h>

static UART_HandleTypeDef huart2;

extern void Error_Handler();


/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* USART2 init function */
void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = USART2_BAUD_RATE;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart2) != HAL_OK) {
    /* Initialization Error */
    Error_Handler();
  }
}

void uart_send_char(uint8_t ch)
{
  HAL_UART_Transmit(&huart2, &ch, 1, 0x1);
}

void uart_send_string(char *s)
{
  HAL_UART_Transmit(&huart2, (uint8_t *) s, strlen(s), 0x1);
}

void uart_send_binary(uint32_t num, uint8_t bits)
{
  uint32_t i;
  uint8_t ch;

  bits--;  /* bits 4 should give i = 1000, not 10000 */

  i = 1 << bits;
  while (i) {
    ch = '0';
    if (num & i) {
      ch = '1';
    }
    uart_send_char(ch);
    i = i >> 1;
  }
}

/* XXX this takes a mask, not a number of digits */
void uart_send_integer(uint32_t data, uint32_t mag) {
  uint32_t i, t;
  uint8_t ch;

  if (! mag) {
    /* Find magnitude */
    if (data < 10) {
      ch = '0' + data;
      uart_send_char(ch);
      return;
    }

    for (mag = 10; mag < data; mag = i) {
      i = mag * 10;
      if (i > data || i < mag)
        break;
    }
  }
  /* mag is now 10 if data is 45, and 1000 if data is 1009 */
  for (i = mag; i; i /= 10) {
    t = (data / i);
    ch = '0' + t;
    uart_send_char(ch);
    data -= (t * i);
  }
}

void uart_send_hex(uint32_t num, uint8_t digits)
{
  uint8_t ch;
  uint32_t mask;

  mask = 0x0FL << ((digits - 1) * 4);

  if (digits == 0 || digits > 8)
      digits = 8;

  while (digits) {
      ch = (num & mask) >> ((digits - 1) * 4);
      if (ch < 10)
	  ch += '0';
      else
	  ch += 'A' - 10;
      uart_send_char(ch);
      --digits;
      mask >>= 4;
  }
}
