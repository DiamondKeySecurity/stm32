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
#include "stm-uart.h"

#define AVRBOOT_NUM_BYTES	(AVRBOOT_PAGE_SIZE * AVRBOOT_NUM_PAGES)


void avrboot_start_tamper(void)
{
	uint8_t resp;
	while (resp != AVRBOOT_STK_INSYNC) {
				uart_send_char_tamper(&huart_tmpr, AVRBOOT_STK_LEAVE_PROGMODE);
				uart_send_char_tamper(&huart_tmpr, 0x20);

				HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
				HAL_Delay(200);
	}
}


HAL_StatusTypeDef avrboot_write_page(uint8_t *page_buffer)
{
    uint8_t resp;


     uint8_t mybuf[64];
     uint8_t rcvbuf[64];
    volatile uint8_t match;


    for(uint8_t k =0; k<64; k++)
    {
    	rcvbuf[k]= 0x00;

    }
    // load address checking proper response will allow the bit-banged uart
    // on the eARM side a chance to synchronize
    volatile uint8_t lower;
    volatile uint8_t upper;
    int retries =3;
    for (uint16_t offset = 0; offset < 7168; offset+=AVRBOOT_PAGE_SIZE){
restart:
    	resp =0;
        retries = 3;
    	lower = (uint8_t)((offset/2) & 0xFF);
    	upper = (uint8_t)(((offset/2)& 0xFF00)>>8);
		while (resp != AVRBOOT_STK_INSYNC) {
			uart_send_char_tamper(&huart_tmpr, AVRBOOT_STK_LOAD_ADDRESS);    //word addresses sent as low/ high bytes
			uart_send_char_tamper(&huart_tmpr, lower); //lower: bootloader doubles address in conversion
			uart_send_char_tamper(&huart_tmpr, upper); //upper: to word value for internal flash commands
			uart_send_char_tamper(&huart_tmpr, 0x20);

			HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
			//HAL_Delay(800);
			HAL_Delay(400);
			retries--;
			if (retries ==0 ){
				HAL_Delay(400);
				__HAL_UART_FLUSH_DRREGISTER(&huart_tmpr);
				goto restart;
			}
		}
		resp = 0;
		match = 0;
		HAL_Delay(1000);
		//HAL_Delay(100);
		while (resp != AVRBOOT_STK_INSYNC){
			uart_send_char_tamper(&huart_tmpr, AVRBOOT_STK_PROG_PAGE);
			uart_send_char_tamper(&huart_tmpr, 0x00);

			uart_send_char_tamper(&huart_tmpr, AVRBOOT_PAGE_SIZE);
			uart_send_char_tamper(&huart_tmpr, 'F');
			uart_send_bytes_tamper(&huart_tmpr, page_buffer, 64);

			uart_send_char_tamper(&huart_tmpr, 0x20);
			HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
			HAL_Delay(800);
		}
		/*	if (resp != AVRBOOT_STK_INSYNC) {
				goto restart;
			}
		HAL_Delay(200);
		resp =0;
		rcvbuf[0] = 0x00;
		retries =3;
		while (resp != AVRBOOT_STK_INSYNC) {
			uart_send_char_tamper(&huart_tmpr, AVRBOOT_STK_READ_PAGE);
			uart_send_char_tamper(&huart_tmpr, 0x00);

			uart_send_char_tamper(&huart_tmpr, AVRBOOT_PAGE_SIZE);
			uart_send_char_tamper(&huart_tmpr, 'F');
			uart_send_char_tamper(&huart_tmpr, 0x20);
			HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);

			//__HAL_UART_FLUSH_DRREGISTER(&huart_tmpr);
			HAL_UART_Receive(&huart_tmpr, rcvbuf, 65, 1000);

			//HAL_Delay(800);
			//HAL_Delay(100);
			retries--;
			if (retries == 0){
				break;}

		}
		match  = 1;
		int j;
		if (rcvbuf[0] == 0x14){
			j = 1;
		}
		else {
			j = 0;
		}
		for (int k=0; k<64; k++){

			if (page_buffer[k] != rcvbuf[k+j]) {
				match = 0;
				//HAL_Delay(1400);
				HAL_Delay(400);
				goto restart;
			}
		}*/


		resp =0;
		//HAL_Delay(1800);
		HAL_Delay(1800);
		page_buffer += AVRBOOT_PAGE_SIZE;
    }
    // check
    //if (!ok) return HAL_ERROR;


    return HAL_OK;
}



