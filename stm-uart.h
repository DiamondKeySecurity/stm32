/*
 * stm-uart.h
 * ---------
 * Functions and defines to use the UART.
 *
 * Copyright (c) 2015, NORDUnet A/S All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the NORDUnet nor the names of its contributors may
 *   be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __STM32_UART_H
#define __STM32_UART_H

#include "stm32f4xx_hal.h"

#define USART_MGMT_BAUD_RATE	921600
#define USART_USER_BAUD_RATE	921600

typedef enum {
  STM_UART_USER,
  STM_UART_MGMT
} stm_uart_port_t;

extern UART_HandleTypeDef huart_mgmt;
extern UART_HandleTypeDef huart_user;

extern DMA_HandleTypeDef hdma_usart_mgmt_rx;
extern DMA_HandleTypeDef hdma_usart_user_rx;

extern void uart_set_default(stm_uart_port_t port);

extern HAL_StatusTypeDef uart_send_char(uint8_t ch);
extern HAL_StatusTypeDef uart_recv_char(uint8_t *cp);

extern HAL_StatusTypeDef uart_send_string(char *s);
extern HAL_StatusTypeDef uart_send_number(uint32_t num, uint8_t digits, uint8_t radix);

extern HAL_StatusTypeDef uart_send_char2(stm_uart_port_t port, uint8_t ch);
extern HAL_StatusTypeDef uart_recv_char2(stm_uart_port_t port, uint8_t *cp, uint32_t timeout);

extern HAL_StatusTypeDef uart_send_string2(stm_uart_port_t port, const char *s);
extern HAL_StatusTypeDef uart_send_number2(stm_uart_port_t port, uint32_t num, uint8_t digits, uint8_t radix);

extern HAL_StatusTypeDef uart_send_bytes(stm_uart_port_t port, uint8_t *buf, size_t len);
extern HAL_StatusTypeDef uart_receive_bytes(stm_uart_port_t port, uint8_t *buf, size_t len, uint32_t timeout);

extern HAL_StatusTypeDef uart_send_hexdump(stm_uart_port_t port, const uint8_t *buf,
					   const uint8_t start_offset, const uint8_t end_offset);

#define uart_send_binary(num, bits)    uart_send_number(num, bits, 2)
#define uart_send_integer(num, digits) uart_send_number(num, digits, 10)
#define uart_send_hex(num, digits)     uart_send_number(num, digits, 16)

#endif /* __STM32_UART_H */
