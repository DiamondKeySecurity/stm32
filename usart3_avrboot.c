/*
 * usart3_avrboot.c
 * ------------------
 * Functions and defines for accessing AVR bootloader.
 *
 * Using USART3 @ 4800 baud, N,8,1
 *
 * Copyright (c) 2016-2017, NORDUnet A/S All rights reserved.
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

#include "usart3_avrboot.h"

#define AVRBOOT_NUM_BYTES	(AVRBOOT_PAGE_SIZE * AVRBOOT_NUM_PAGES)


void avrboot_start_tamper(void)
{
	uint8_t resp;
	while (resp != AVRBOOT_STK_INSYNC) {
				uart_send_char2(&huart_tmpr, AVRBOOT_STK_LEAVE_PROGMODE);
				uart_send_char2(&huart_tmpr, 0x20);

				HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
				DELAY_SH();
	}
}


HAL_StatusTypeDef avrboot_write_page(const uint8_t *page_buffer)
{
    uint8_t resp;

    // check offset
    //if (page_offset >= AVBOOT_NUM_PAGES) return HAL_ERROR;

    // calculate byte address
    //uint32_t byte_offset = page_offset * AVRBOOT_PAGE_SIZE;

    // load address checking proper response will allow the bit-banged uart
    // on the ARM side a chance to synchronize
    for (uint16_t offset = 0; offset < 7168; offset+=40){
		while (resp != AVRBOOT_STK_INSYNC) {
			uart_send_char2(&huart_tmpr, AVRBOOT_STK_LOAD_ADDRESS);
			uart_send_char2(&huart_tmpr, (uint8_t)offset>>8); //upper
			uart_send_char2(&huart_tmpr, (uint8_t)offset && 0xFF);
			uart_send_char2(&huart_tmpr, 0x20);

			HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
			DELAY_SH();
		}

		DELAY_SH();
		uart_send_char2(&huart_tmpr, AVRBOOT_STK_PROG_PAGE);
		uart_send_char2(&huart_tmpr, 0x00);
		uart_send_char2(&huart_tmpr, 0x40);
		uart_send_char2(&huart_tmpr, 'F');
		uart_send_bytes_tamper(&huart_tmpr, page_buffer, 64);
		uart_send_char2(&huart_tmpr, 0x20);

		HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);

		DELAY_SH();
		page_buffer += AVRBOOT_PAGE_SIZE;
    }
    // check
    //if (!ok) return HAL_ERROR;


    return HAL_OK;
}



