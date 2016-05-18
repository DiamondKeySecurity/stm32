/*
 * stm-uart.c
 * ----------
 * Functions for sending strings and numbers over the uart.
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

#include "stm32f4xx_hal.h"
#include "stm-uart.h"

#include <string.h>

UART_HandleTypeDef huart_mgmt;
UART_HandleTypeDef huart_user;

#define DEFAULT_UART STM_UART_USER


inline UART_HandleTypeDef *_which_uart(enum stm_uart_port port)
{
    if (port == STM_UART_USER) {
        return &huart_user;
    } else if (port == STM_UART_MGMT) {
        return &huart_mgmt;
    }

    return NULL;
}

/* send a single character */
HAL_StatusTypeDef uart_send_char(uint8_t ch)
{
    return uart_send_char2(DEFAULT_UART, ch);
}

HAL_StatusTypeDef uart_send_char2(enum stm_uart_port port, uint8_t ch)
{
    UART_HandleTypeDef *uart = _which_uart(port);

    if (uart)
        return HAL_UART_Transmit(uart, &ch, 1, 0x1);

    return HAL_ERROR;
}

/* receive a single character */
HAL_StatusTypeDef uart_recv_char(uint8_t *cp)
{
    return uart_recv_char2(DEFAULT_UART, cp, HAL_MAX_DELAY);
}

/* receive a single character */
HAL_StatusTypeDef uart_recv_char2(enum stm_uart_port port, uint8_t *cp, uint32_t timeout)
{
    UART_HandleTypeDef *uart = _which_uart(port);

    if (uart)
        return HAL_UART_Receive(uart, cp, 1, timeout);

    return HAL_ERROR;
}

/* send a string */
HAL_StatusTypeDef uart_send_string(char *s)
{
    return uart_send_string2(DEFAULT_UART, s);
}

/* send a string */
HAL_StatusTypeDef uart_send_string2(enum stm_uart_port port, const char *s)
{
    UART_HandleTypeDef *uart = _which_uart(port);

    if (uart)
	return HAL_UART_Transmit(uart, (uint8_t *) s, strlen(s), 0x1);

    return HAL_ERROR;
}

/* send raw bytes */
HAL_StatusTypeDef uart_send_bytes(enum stm_uart_port port, uint8_t *buf, size_t len)
{
    UART_HandleTypeDef *uart = _which_uart(port);

    if (uart)
        return HAL_UART_Transmit(uart, (uint8_t *) buf, (uint32_t) len, 0x1);

    return HAL_ERROR;
}

/* receive raw bytes */
HAL_StatusTypeDef uart_receive_bytes(enum stm_uart_port port, uint8_t *buf, size_t len, uint32_t timeout)
{
    UART_HandleTypeDef *uart = _which_uart(port);

    if (uart)
        return HAL_UART_Receive(uart, (uint8_t *) buf, (uint32_t) len, timeout);

    return HAL_ERROR;
}

/* Generalized routine to send binary, decimal, and hex integers.
 * This code is adapted from Chris Giese's printf.c
 */
HAL_StatusTypeDef uart_send_number(uint32_t num, uint8_t digits, uint8_t radix)
{
    return uart_send_number2(DEFAULT_UART, num, digits, radix);
}

HAL_StatusTypeDef uart_send_number2(enum stm_uart_port port, uint32_t num, uint8_t digits, uint8_t radix)
{
    #define BUFSIZE 32
    char buf[BUFSIZE];
    char *where = buf + BUFSIZE;
    UART_HandleTypeDef *uart = _which_uart(port);

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

    return HAL_UART_Transmit(uart, (uint8_t *) where, digits, 0x1);
}

HAL_StatusTypeDef uart_send_hexdump(enum stm_uart_port port, const uint8_t *buf,
				    const uint8_t start_offset, const uint8_t end_offset)
{
    uint32_t i;

    uart_send_string2(port, "00 -- ");

    for (i = 0; i <= end_offset; i++) {
	if (i && (! (i % 16))) {
	    uart_send_string2(port, "\r\n");

	    if (i != end_offset) {
		/* Output new offset unless the last byte is reached */
		uart_send_number2(port, i, 2, 16);
		uart_send_string2(port, " -- ");
	    }
	}

	uart_send_number2(port, *(buf + i), 2, 16);
	uart_send_string2(port, " ");
    }

    return HAL_OK;
}
