#ifndef __STM32_DEV_BRIDGE_UART_H
#define __STM32_DEV_BRIDGE_UART_H

#include "stm32f4xx_hal.h"

#define USART2_BAUD_RATE	115200

extern void MX_USART2_UART_Init(void);

extern void uart_send_char(uint8_t ch);
extern void uart_send_string(char *s);
extern void uart_send_number(uint32_t num, uint8_t digits, uint8_t radix);
#define uart_send_binary(num, bits)    uart_send_number(num, bits, 2)
#define uart_send_integer(num, digits) uart_send_number(num, digits, 10)
#define uart_send_hex(num, digits)     uart_send_number(num, digits, 16)

#endif /* __STM32_DEV_BRIDGE_UART_H */
