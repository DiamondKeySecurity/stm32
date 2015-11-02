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

/* Generalized routine to send binary, decimal, and hex integers.
 * This code is adapted from Chris Giese's printf.c
 */
void uart_send_number(uint32_t num, uint8_t digits, uint8_t radix)
{
    #define BUFSIZE 32
    char buf[BUFSIZE];
    char *where = buf + BUFSIZE;

    /* initialize buf so we can add leading 0 by adjusting the pointer */
    memset(buf, '0', BUFSIZE);

    /* build the string backwards, starting with the least significant digit */
    do {
	uint32_t temp;
	temp = num % radix;
	where--;
	if (temp < 10)
	    *where = temp + '0';
	else
	    *where = temp - 10 + 'A';
	num = num / radix;
    } while (num != 0);

    if (where > buf + BUFSIZE - digits)
	/* pad with leading 0 */
	where = buf + BUFSIZE - digits;
    else
	/* number is larger than the specified number of digits */
	digits = buf + BUFSIZE - where;

    HAL_UART_Transmit(&huart2, (uint8_t *) where, digits, 0x1);
}
