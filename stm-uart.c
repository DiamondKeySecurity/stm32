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

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* send a single character */
HAL_StatusTypeDef uart_send_char(uint8_t ch)
{
    return HAL_UART_Transmit(&huart2, &ch, 1, 0x1);
}

HAL_StatusTypeDef uart_send_char2(enum stm_uart_port port, uint8_t ch)
{
    if (port == STM_UART_USER) {
      return HAL_UART_Transmit(&huart2, &ch, 1, 0x1);
    } else if (port == STM_UART_MGMT) {
      return HAL_UART_Transmit(&huart1, &ch, 1, 0x1);
    }
    return HAL_ERROR;
}

/* receive a single character */
HAL_StatusTypeDef uart_recv_char(uint8_t *cp)
{
    return HAL_UART_Receive(&huart2, cp, 1, HAL_MAX_DELAY);
}

/* receive a single character */
HAL_StatusTypeDef uart_recv_char2(enum stm_uart_port port, uint8_t *cp, uint32_t timeout)
{
    if (port == STM_UART_USER) {
      return HAL_UART_Receive(&huart2, cp, 1, timeout);
    } else if (port == STM_UART_MGMT) {
      return HAL_UART_Receive(&huart1, cp, 1, timeout);
    }
    return HAL_ERROR;
}

/* send a string */
HAL_StatusTypeDef uart_send_string(char *s)
{
    return HAL_UART_Transmit(&huart2, (uint8_t *) s, strlen(s), 0x1);
}

/* Generalized routine to send binary, decimal, and hex integers.
 * This code is adapted from Chris Giese's printf.c
 */
HAL_StatusTypeDef uart_send_number(uint32_t num, uint8_t digits, uint8_t radix)
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

    return HAL_UART_Transmit(&huart2, (uint8_t *) where, digits, 0x1);
}
