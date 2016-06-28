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

/* Rename both CMSIS HAL_OK and libhal HAL_OK to disambiguate */
#define HAL_OK CMSIS_HAL_OK
#include "dfu.h"
#include "stm-led.h"
#include "stm-uart.h"
#include "stm-flash.h"

#undef HAL_OK
#define HAL_OK LIBHAL_OK
#include "hal.h"
#undef HAL_OK

#include <string.h>

extern uint32_t update_crc(uint32_t crc, uint8_t *buf, int len);

static int getline(char *buf, int len)
{
    int i;
    uint8_t c;

    for (i = 0; i < len; ++i) {
        if (uart_recv_char2(STM_UART_MGMT, &c, HAL_MAX_DELAY) != CMSIS_HAL_OK)
            return -1;
        if (c == '\r') {
            buf[i] = '\0';
            break;
        }
        buf[i] = c;
    }
    return i;
}

static void uart_flush(void)
{
    uint8_t c;
    while (uart_recv_char2(STM_UART_MGMT, &c, 0) == CMSIS_HAL_OK) { ; }
}

static int do_login(void)
{
    char username[8];
    char pin[hal_rpc_max_pin_length];
    hal_client_handle_t client = { -1 };
    hal_user_t user;
    int n;

    uart_flush();
    uart_send_string2(STM_UART_MGMT, "\r\nUsername: ");
    if (getline(username, sizeof(username)) <= 0)
        return -1;
    if (strcmp(username, "wheel") == 0)
        user = HAL_USER_WHEEL;
    else if (strcmp(username, "so") == 0)
        user = HAL_USER_SO;
    else if (strcmp(username, "user") == 0)
        user = HAL_USER_NORMAL;
    else
        user = HAL_USER_NONE;

    uart_flush();
    uart_send_string2(STM_UART_MGMT, "\r\nPassword: ");
    if ((n = getline(pin, sizeof(pin))) <= 0)
        return -1;

    uart_flush();

    if (hal_rpc_login(client, user, pin, n) != LIBHAL_OK) {
        uart_send_string2(STM_UART_MGMT, "\r\nAccess denied\r\n");
        return -1;
    }
    return 0;
}

int dfu_receive_firmware(void)
{
    uint32_t filesize = 0, crc = 0, my_crc = 0, counter = 0;
    uint32_t offset = DFU_FIRMWARE_ADDR, n = DFU_UPLOAD_CHUNK_SIZE;
    uint8_t buf[DFU_UPLOAD_CHUNK_SIZE];

    if (do_login() != 0)
        return -1;

    /* Fake the CLI */
    uart_send_string2(STM_UART_MGMT, "\r\ncryptech> ");
    char cmd[64];
    if (getline(cmd, sizeof(cmd)) <= 0)
        return -1;
    if (strcmp(cmd, "firmware upload") != 0) {
        uart_send_string2(STM_UART_MGMT, "\r\nInvalid command \"");
        uart_send_string2(STM_UART_MGMT, cmd);
        uart_send_string2(STM_UART_MGMT, "\"\r\n");
        return -1;
    }

    uart_send_string2(STM_UART_MGMT, "OK, write size (4 bytes), data in 4096 byte chunks, CRC-32 (4 bytes)\r\n");

    /* Read file size (4 bytes) */
    uart_receive_bytes(STM_UART_MGMT, (void *) &filesize, 4, 10000);
    if (filesize < 512 || filesize > DFU_FIRMWARE_END_ADDR - DFU_FIRMWARE_ADDR) {
        uart_send_string2(STM_UART_MGMT, "Invalid filesize ");
        uart_send_number2(STM_UART_MGMT, filesize, 1, 10);
        uart_send_string2(STM_UART_MGMT, "\r\n");
	return -1;
    }

    HAL_FLASH_Unlock();

    uart_send_string2(STM_UART_MGMT, "Send ");
    uart_send_number2(STM_UART_MGMT, filesize, 1, 10);
    uart_send_string2(STM_UART_MGMT, " bytes of data\r\n");

    while (filesize) {
	/* By initializing buf to the same value that erased flash has (0xff), we don't
	 * have to try and be smart when writing the last page of data to the memory.
	 */
	memset(buf, 0xff, sizeof(buf));

	if (filesize < n) {
	    n = filesize;
	}

	if (uart_receive_bytes(STM_UART_MGMT, (void *) buf, n, 10000) != CMSIS_HAL_OK) {
	    return -2;
	}
	filesize -= n;

	/* After reception of a chunk but before ACKing we have "all" the time in the world to
	 * calculate CRC and write it to flash.
	 */
	my_crc = update_crc(my_crc, buf, n);
	stm_flash_write32(offset, (uint32_t *)buf, sizeof(buf)/4);
	offset += DFU_UPLOAD_CHUNK_SIZE;

	/* ACK this chunk by sending the current chunk counter (4 bytes) */
	counter++;
	uart_send_bytes(STM_UART_MGMT, (void *) &counter, 4);
	led_toggle(LED_BLUE);
    }

    HAL_FLASH_Lock();

    uart_send_string2(STM_UART_MGMT, "Send CRC-32\r\n");

    /* The sending side will now send its calculated CRC-32 */
    uart_receive_bytes(STM_UART_MGMT, (void *) &crc, 4, 10000);

    uart_send_string2(STM_UART_MGMT, "CRC-32 0x");
    uart_send_number2(STM_UART_MGMT, crc, 1, 16);
    uart_send_string2(STM_UART_MGMT, ", calculated CRC 0x");
    uart_send_number2(STM_UART_MGMT, my_crc, 1, 16);
    if (crc == my_crc) {
	uart_send_string2(STM_UART_MGMT, "CRC checksum MATCHED\r\n");
        return 0;
    } else {
	uart_send_string2(STM_UART_MGMT, "CRC checksum did NOT match\r\n");
    }

    led_on(LED_RED);
    led_on(LED_YELLOW);

    /* Better to erase the known bad firmware */
    stm_flash_erase_sectors(DFU_FIRMWARE_ADDR, DFU_FIRMWARE_END_ADDR);

    led_off(LED_YELLOW);

    return 0;
}
