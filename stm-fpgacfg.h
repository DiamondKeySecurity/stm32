/*
 * stm-fpgacfg.h
 * ---------
 * Functions and defines for accessing the FPGA config memory.
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

#ifndef __STM32_FPGACFG_H
#define __STM32_FPGACFG_H

#include "stm32f4xx_hal.h"

#define N25Q128_SPI_HANDLE		(&hspi_fpgacfg)

#define N25Q128_COMMAND_READ_ID		0x9E
#define N25Q128_COMMAND_READ_PAGE	0x03
#define N25Q128_COMMAND_READ_STATUS	0x05
#define N25Q128_COMMAND_WRITE_ENABLE	0x06
#define N25Q128_COMMAND_ERASE_SECTOR	0xD8
#define N25Q128_COMMAND_PAGE_PROGRAM	0x02

#define N25Q128_PAGE_SIZE		0x100		// 256
#define N25Q128_NUM_PAGES		0x10000		// 65536

#define N25Q128_SECTOR_SIZE		0x10000		// 65536
#define N25Q128_NUM_SECTORS		0x100		// 256

#define N25Q128_SPI_TIMEOUT		1000

#define N25Q128_ID_MANUFACTURER		0x20
#define N25Q128_ID_DEVICE_TYPE		0xBA
#define N25Q128_ID_DEVICE_CAPACITY	0x18

#define PROM_FPGA_DIS_Pin		GPIO_PIN_14
#define PROM_FPGA_DIS_GPIO_Port		GPIOI
#define PROM_ARM_ENA_Pin		GPIO_PIN_6
#define PROM_ARM_ENA_GPIO_Port		GPIOF
#define PROM_CS_N_Pin			GPIO_PIN_12
#define PROM_CS_N_GPIO_Port		GPIOB


#define _n25q128_select()	HAL_GPIO_WritePin(PROM_CS_N_GPIO_Port, PROM_CS_N_Pin, GPIO_PIN_RESET);
#define _n25q128_deselect()	HAL_GPIO_WritePin(PROM_CS_N_GPIO_Port, PROM_CS_N_Pin, GPIO_PIN_SET);

extern int n25q128_check_id(void);
extern int n25q128_get_wip_flag(void);
extern int n25q128_read_page(uint32_t page_offset, uint8_t *page_buffer);
extern int n25q128_write_page(uint32_t page_offset, uint8_t *page_buffer);
extern int n25q128_erase_sector(uint32_t sector_offset);

extern void fpgacfg_give_access_to_stm32(void);
extern void fpgacfg_give_access_to_fpga(void);

extern SPI_HandleTypeDef hspi_fpgacfg;

#endif /* __STM32_FPGACFG_H */
