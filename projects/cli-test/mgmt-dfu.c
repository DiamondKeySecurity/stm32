/*
 * mgmt-dfu.c
 * ---------
 * Management CLI Device Firmware Upgrade code.
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

#include "stm-init.h"
#include "mgmt-cli.h"
#include "stm-uart.h"
#include "cmsis_nvic.h"

#include <string.h>


#define DFU_BASE_ADDRESS	0x08100000
#define DFU_BASE_PTR		(__IO uint32_t *) DFU_BASE_ADDRESS


extern uint32_t update_crc(uint32_t crc, uint8_t *buf, int len);


/* The chunk size have to be a multiple of the SPI flash page size (256 bytes),
   and it has to match the chunk size in the program sending the bitstream over the UART.
*/
#define DFU_UPLOAD_CHUNK_SIZE 256

int cmd_dfu_upload(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint32_t filesize = 0, crc = 0, my_crc = 0, counter = 0, i, j;
    uint32_t offset = 0, n = DFU_UPLOAD_CHUNK_SIZE;
    uint32_t buf[DFU_UPLOAD_CHUNK_SIZE / 4];
    FLASH_EraseInitTypeDef FLASH_EraseInitStruct;
    uint32_t SectorError = 0;

    cli_print(cli, "OK, write DFU application file size (4 bytes), data in %i byte chunks, CRC-32 (4 bytes)",
	      DFU_UPLOAD_CHUNK_SIZE);

    /* Read file size (4 bytes) */
    uart_receive_bytes(STM_UART_MGMT, (void *) &filesize, 4, 1000);
    cli_print(cli, "File size %li", filesize);

    HAL_FLASH_Unlock();

    FLASH_EraseInitStruct.TypeErase = TYPEERASE_SECTORS;
    FLASH_EraseInitStruct.Sector = 12; /* the sector for DFU_BASE_ADDRESS (0x08100000) */
    FLASH_EraseInitStruct.NbSectors = 1;
    FLASH_EraseInitStruct.VoltageRange = VOLTAGE_RANGE_3;

    if (HAL_FLASHEx_Erase(&FLASH_EraseInitStruct, &SectorError) != HAL_OK) {
	cli_print(cli, "Failed erasing flash sector");
	return CLI_ERROR;
    }

    while (filesize) {
	/* By initializing buf to the same value that erased flash has (0xff), we don't
	 * have to try and be smart when writing the last page of data to the memory.
	 */
	memset(buf, 0xffffffff, sizeof(buf));

	if (filesize < n) {
	    n = filesize;
	}

	if (uart_receive_bytes(STM_UART_MGMT, (void *) &buf, n, 1000) != HAL_OK) {
	    cli_print(cli, "Receive timed out");
	    return CLI_ERROR;
	}
	filesize -= n;

	/* After reception of a chunk but before ACKing we have "all" the time in the world to
	 * calculate CRC and write it to flash.
	 */
	my_crc = update_crc(my_crc, (uint8_t *) buf, n);

	for (i = 0; i < DFU_UPLOAD_CHUNK_SIZE / 4; i++) {
	    if ((j = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, DFU_BASE_ADDRESS + offset, buf[i])) != HAL_OK) {
		cli_print(cli, "Failed writing data at offset %li: %li", offset, j);
		return CLI_ERROR;
	    }
	    offset += 4;
	}

	/* ACK this chunk by sending the current chunk counter (4 bytes) */
	counter++;
	uart_send_bytes(STM_UART_MGMT, (void *) &counter, 4);
    }

    HAL_FLASH_Lock();

    /* The sending side will now send it's calculated CRC-32 */
    cli_print(cli, "Send CRC-32");
    uart_receive_bytes(STM_UART_MGMT, (void *) &crc, 4, 1000);
    cli_print(cli, "CRC-32 %li", crc);
    if (crc == my_crc) {
	cli_print(cli, "CRC checksum MATCHED");
    } else {
	cli_print(cli, "CRC checksum did NOT match");
    }

    return CLI_OK;
}

int cmd_dfu_dump(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "First 256 bytes from DFU application address %p:\r\n", DFU_BASE_PTR);

    uart_send_hexdump(STM_UART_MGMT, (uint8_t *) DFU_BASE_PTR, 0, 0xff);
    uart_send_string2(STM_UART_MGMT, (char *) "\r\n\r\n");

    return CLI_OK;
}

typedef  int (*pFunction)(void);

int cmd_dfu_jump(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint32_t new_msp, i;
    /* Load first byte from the DFU_BASE_PTR to verify it contains an IVT before
     * jumping there.
     */
    new_msp = *DFU_BASE_PTR;
    i = new_msp & 0xFF000000;
    /* 'i' is supposed to be a pointer to the new applications stack, it should
     * point either at RAM (0x20000000) or at the CCM memory (0x10000000).
     */
    if (i == 0x20000000 || i == 0x10000000) {
	uint32_t jmp_to = *(DFU_BASE_PTR + 1);
	pFunction loaded_app = (pFunction) jmp_to;

	__disable_irq();
	HAL_NVIC_DisableIRQ(SysTick_IRQn);

	HAL_DeInit();

	/* Relocate interrupt vector table */
	//NVIC_SetVectorTable(DFU_BASE_ADDRESS);
	SCB->VTOR == DFU_BASE_ADDRESS;
	NVIC_SetVector(WWDG_IRQn, *DFU_BASE_PTR + 1);

	/* Re-initialize stack pointer */
	__set_MSP(new_msp);
	/* Jump to the DFU loaded application */
	loaded_app();
	Error_Handler();
    } else {
	cli_print(cli, "No loaded application found at %p", DFU_BASE_PTR);
    }

    return CLI_OK;
}

void configure_cli_dfu(struct cli_def *cli)
{
    cli_command_root(dfu);

    cli_command_node(dfu, dump, "Show the first 256 bytes of the loaded application");
    cli_command_node(dfu, jump, "Jump to the loaded application");
    cli_command_node(dfu, upload, "Load a new application");
}
