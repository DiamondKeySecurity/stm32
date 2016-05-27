/*
 * dfu.c
 * ------------
 * Receive new firmware from MGMT UART and write it to STM32 internal flash.
 *
 * Copyright (c) 2016, NORDUnet A/S All rights reserved.
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
#include "dfu.h"
#include "stm-led.h"
#include "stm-uart.h"
#include "stm-flash.h"

#include <string.h>

extern uint32_t update_crc(uint32_t crc, uint8_t *buf, int len);


int dfu_receive_firmware(void)
{
    uint32_t filesize = 0, crc = 0, my_crc = 0, counter = 0;
    uint32_t offset = DFU_FIRMWARE_ADDR, n = DFU_UPLOAD_CHUNK_SIZE;
    uint32_t buf[DFU_UPLOAD_CHUNK_SIZE / 4];

    uart_send_string2(STM_UART_MGMT, (char *) "\r\nOK, bootloader waiting for new firmware\r\n");

    /* Read file size (4 bytes) */
    uart_receive_bytes(STM_UART_MGMT, (void *) &filesize, 4, 1000);
    if (filesize < 512 || filesize > DFU_FIRMWARE_END_ADDR - DFU_FIRMWARE_ADDR) {
	return -1;
    }

    HAL_FLASH_Unlock();

    while (filesize) {
	/* By initializing buf to the same value that erased flash has (0xff), we don't
	 * have to try and be smart when writing the last page of data to the memory.
	 */
	memset(buf, 0xffffffff, sizeof(buf));

	if (filesize < n) {
	    n = filesize;
	}

	if (uart_receive_bytes(STM_UART_MGMT, (void *) &buf, n, 1000) != HAL_OK) {
	    return -2;
	}
	filesize -= n;

	/* After reception of a chunk but before ACKing we have "all" the time in the world to
	 * calculate CRC and write it to flash.
	 */
	my_crc = update_crc(my_crc, (uint8_t *) buf, n);
	stm_flash_write32(offset, buf, sizeof(buf) / 4);
	offset += DFU_UPLOAD_CHUNK_SIZE;

	/* ACK this chunk by sending the current chunk counter (4 bytes) */
	counter++;
	uart_send_bytes(STM_UART_MGMT, (void *) &counter, 4);
	led_toggle(LED_BLUE);
    }

    HAL_FLASH_Lock();

    /* The sending side will now send it's calculated CRC-32 */
    uart_receive_bytes(STM_UART_MGMT, (void *) &crc, 4, 1000);
    if (crc == my_crc) {
	uart_send_string2(STM_UART_MGMT, (char *) "\r\nSuccess\r\n");
	return 0;
    }

    led_on(LED_RED);
    led_on(LED_YELLOW);

    /* Better to erase the known bad firmware */
    stm_flash_erase_sectors(DFU_FIRMWARE_ADDR, DFU_FIRMWARE_END_ADDR);

    led_off(LED_YELLOW);

    return 0;
}
