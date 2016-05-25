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

extern uint32_t update_crc(uint32_t crc, uint8_t *buf, int len);

/* symbols defined in the linker script (STM32F429BI.ld) */
extern uint32_t CRYPTECH_FIRMWARE_START;
extern uint32_t CRYPTECH_FIRMWARE_END;
extern uint32_t CRYPTECH_DFU_CONTROL;

#define DFU_FIRMWARE_ADDR         ((uint32_t ) &CRYPTECH_FIRMWARE_START)
#define DFU_FIRMWARE_PTR	  ((__IO uint32_t *) (CRYPTECH_FIRMWARE_START))
#define DFU_FIRMWARE_END_ADDR     CRYPTECH_FIRMWARE_END
#define HARDWARE_EARLY_DFU_JUMP   0xBADABADA
#define DFU_UPLOAD_CHUNK_SIZE     256

__IO uint32_t *dfu_control = &CRYPTECH_DFU_CONTROL;
__IO uint32_t *dfu_new_msp = &CRYPTECH_FIRMWARE_START;
__IO uint32_t *dfu_firmware = &CRYPTECH_FIRMWARE_START + 4;

/* Flash sector offsets from RM0090, Table 6. Flash module - 2 Mbyte dual bank organization */
#define FLASH_NUM_SECTORS 24 + 1
uint32_t flash_sector_offsets[FLASH_NUM_SECTORS] = {
    /* Bank 1 */
    0x08000000, /* #0,  16 KBytes */
    0x08004000, /* #1,  16 Kbytes */
    0x08008000, /* #2,  16 Kbytes */
    0x0800C000, /* #3,  16 Kbytes */
    0x08010000, /* #4,  64 Kbytes */
    0x08020000, /* #5,  128 Kbytes */
    0x08040000, /* #6,  128 Kbytes */
    0x08060000, /* #7,  128 Kbytes */
    0x08080000, /* #8,  128 Kbytes */
    0x080A0000, /* #9,  128 Kbytes */
    0x080C0000, /* #10, 128 Kbytes */
    0x080E0000, /* #11, 128 Kbytes */
    /* Bank 2 */
    0x08100000, /* #12,  16 Kbytes */
    0x08104000, /* #13,  16 Kbytes */
    0x08108000, /* #14,  16 Kbytes */
    0x0810C000, /* #15,  16 Kbytes */
    0x08110000, /* #16,  64 Kbytes */
    0x08120000, /* #17, 128 Kbytes */
    0x08140000, /* #18, 128 Kbytes */
    0x08160000, /* #19, 128 Kbytes */
    0x08180000, /* #20, 128 Kbytes */
    0x081A0000, /* #21, 128 Kbytes */
    0x081C0000, /* #22, 128 Kbytes */
    0x081E0000, /* #23, 128 Kbytes */
    0x08200000  /* first address *after* flash */
};


typedef  void (*pFunction)(void);


/* This is it's own function to make it more convenient to set a breakpoint at it in gdb */
void do_early_dfu_jump(void)
{
    //pFunction loaded_app = (pFunction) *(DFU_FIRMWARE_PTR + 1);
    pFunction loaded_app = (pFunction) *dfu_firmware;
    *dfu_control = 0;
    __set_MSP(*dfu_new_msp);
    /* Set the Vector Table Offset Register */
    SCB->VTOR = DFU_FIRMWARE_ADDR;
    loaded_app();
    while (1);
}

/* This function is called from main() before any peripherals are initialized */
void check_early_dfu_jump(void)
{
    if (*dfu_control == HARDWARE_EARLY_DFU_JUMP) {
	do_early_dfu_jump();
    }
}

inline int _flash_sector_num(uint32_t offset)
{
    int i = FLASH_NUM_SECTORS - 1;
    while (i-- >= 0) {
	if (offset >= flash_sector_offsets[i] &&
	    offset < flash_sector_offsets[i + 1]) {
	    return i;
	}
    }
    return -1;
}

int _write_to_flash(uint32_t offset, const uint32_t *buf, uint32_t elements)
{
    uint32_t sector = _flash_sector_num(offset);
    uint32_t SectorError = 0, i, j;

    if (offset == flash_sector_offsets[sector]) {
	/* Request to write to beginning of a flash sector, erase it first. */
	FLASH_EraseInitTypeDef FLASH_EraseInitStruct;

	FLASH_EraseInitStruct.TypeErase = TYPEERASE_SECTORS;
	FLASH_EraseInitStruct.Sector = sector;
	FLASH_EraseInitStruct.NbSectors = 1;
	FLASH_EraseInitStruct.VoltageRange = VOLTAGE_RANGE_3;

	if (HAL_FLASHEx_Erase(&FLASH_EraseInitStruct, &SectorError) != HAL_OK) {
	    return -1;
	}
    }

    for (i = 0; i < elements; i++) {
	if ((j = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, offset, buf[i])) != HAL_OK) {
	    return -2;
	}
	offset += 4;
    }

    return 1;
}

int cmd_dfu_upload(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint32_t filesize = 0, crc = 0, my_crc = 0, counter = 0;
    uint32_t offset = DFU_FIRMWARE_ADDR, n = DFU_UPLOAD_CHUNK_SIZE;
    uint32_t buf[DFU_UPLOAD_CHUNK_SIZE / 4];

    cli_print(cli, "OK, write DFU application file size (4 bytes), data in %i byte chunks, CRC-32 (4 bytes)",
	      DFU_UPLOAD_CHUNK_SIZE);

    /* Read file size (4 bytes) */
    uart_receive_bytes(STM_UART_MGMT, (void *) &filesize, 4, 1000);
    cli_print(cli, "File size %li, will write it to 0x%lx", filesize, offset);

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
	    cli_print(cli, "Receive timed out");
	    return CLI_ERROR;
	}
	filesize -= n;

	/* After reception of a chunk but before ACKing we have "all" the time in the world to
	 * calculate CRC and write it to flash.
	 */
	my_crc = update_crc(my_crc, (uint8_t *) buf, n);
	_write_to_flash(offset, buf, sizeof(buf) / 4);
	offset += DFU_UPLOAD_CHUNK_SIZE;

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
    cli_print(cli, "First 256 bytes from DFU application address %p:\r\n", DFU_FIRMWARE_PTR);

    uart_send_hexdump(STM_UART_MGMT, (uint8_t *) DFU_FIRMWARE_ADDR, 0, 0xff);
    uart_send_string2(STM_UART_MGMT, (char *) "\r\n\r\n");

    return CLI_OK;
}

int cmd_dfu_erase(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint32_t start_sector = _flash_sector_num(DFU_FIRMWARE_ADDR);
    uint32_t end_sector = _flash_sector_num(DFU_FIRMWARE_END_ADDR);
    uint32_t sector;

    cli_print(cli, "Erasing flash sectors %li to %li (address %p to %p)",
	      start_sector, end_sector,
	      (uint32_t *) DFU_FIRMWARE_ADDR,
	      (uint32_t *) DFU_FIRMWARE_END_ADDR);

    if (start_sector > end_sector) {
	cli_print(cli, "ERROR: Bad sectors");
	return CLI_ERROR;
    }

    HAL_FLASH_Unlock();

    for (sector = start_sector; sector <= end_sector; sector++) {
	uint32_t SectorError = 0;
	FLASH_EraseInitTypeDef FLASH_EraseInitStruct;

	FLASH_EraseInitStruct.TypeErase = TYPEERASE_SECTORS;
	FLASH_EraseInitStruct.Sector = sector;
	FLASH_EraseInitStruct.NbSectors = 1;
	FLASH_EraseInitStruct.VoltageRange = VOLTAGE_RANGE_3;

	if (HAL_FLASHEx_Erase(&FLASH_EraseInitStruct, &SectorError) != HAL_OK) {
	    cli_print(cli, "ERROR: Failed erasing sector %li", sector);
	}
    }
    HAL_FLASH_Lock();

    return CLI_OK;
}

int cmd_dfu_jump(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint32_t i;
    /* Load first byte from the DFU_FIRMWARE_PTR to verify it contains an IVT before
     * jumping there.
     */
    cli_print(cli, "Checking for application at %p", DFU_FIRMWARE_PTR);

    //new_msp = (uint32_t) DFU_FIRMWARE_PTR;
    i = *dfu_new_msp & 0xFF000000;
    /* 'new_msp' is supposed to be a pointer to the new applications stack, it should
     * point either at RAM (0x20000000) or at the CCM memory (0x10000000).
     */
    if (i == 0x20000000 || i == 0x10000000) {
	*dfu_control = HARDWARE_EARLY_DFU_JUMP;
	cli_print(cli, "Making the leap");
	HAL_NVIC_SystemReset();
	while (1) { ; }
    } else {
	cli_print(cli, "No loaded application found at %p (read 0x%x)",
		  DFU_FIRMWARE_PTR, (unsigned int) *dfu_new_msp);
    }

    return CLI_OK;
}

void configure_cli_dfu(struct cli_def *cli)
{
    cli_command_root(dfu);

    cli_command_node(dfu, dump, "Show the first 256 bytes of the loaded application");
    cli_command_node(dfu, jump, "Jump to the loaded application");
    cli_command_node(dfu, upload, "Load a new application");
    cli_command_node(dfu, erase, "Erase the application memory");
}
