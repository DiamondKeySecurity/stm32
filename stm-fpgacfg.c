/*
 * stm-fpgacfg.c
 * ----------
 * Functions for accessing the FPGA config memory and controlling
 * the low-level status of the FPGA (reset registers/reboot etc.).
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

#include "stm32f4xx_hal.h"
#include "stm-fpgacfg.h"
#include "stm-init.h"

SPI_HandleTypeDef hspi_fpgacfg;

struct spiflash_ctx fpgacfg_ctx = {&hspi_fpgacfg, PROM_CS_N_GPIO_Port, PROM_CS_N_Pin};

int fpgacfg_check_id()
{
    return n25q128_check_id(&fpgacfg_ctx);
}

int fpgacfg_write_data(uint32_t offset, const uint8_t *buf, const uint32_t len)
{
    if ((offset % N25Q128_SECTOR_SIZE) == 0)
	// first page in sector, need to erase sector
	if (! n25q128_erase_sector(&fpgacfg_ctx, offset / N25Q128_SECTOR_SIZE))
	    return -4;

    return n25q128_write_data(&fpgacfg_ctx, offset, buf, len);
}

void fpgacfg_access_control(enum fpgacfg_access_ctrl access)
{
    if (access == ALLOW_ARM) {
	// fpga disable = 1
	HAL_GPIO_WritePin(PROM_FPGA_DIS_GPIO_Port, PROM_FPGA_DIS_Pin, GPIO_PIN_SET);
	// arm enable = 0
	HAL_GPIO_WritePin(GPIOF, PROM_ARM_ENA_Pin, GPIO_PIN_RESET);
    } else if (access == ALLOW_FPGA) {
	// fpga disable = 0
	HAL_GPIO_WritePin(PROM_FPGA_DIS_GPIO_Port, PROM_FPGA_DIS_Pin, GPIO_PIN_RESET);
	// arm enable = 1
	HAL_GPIO_WritePin(GPIOF, PROM_ARM_ENA_Pin, GPIO_PIN_SET);
    } else {
	// fpga disable = 1
	HAL_GPIO_WritePin(PROM_FPGA_DIS_GPIO_Port, PROM_FPGA_DIS_Pin, GPIO_PIN_SET);
	// arm enable = 1
	HAL_GPIO_WritePin(GPIOF, PROM_ARM_ENA_Pin, GPIO_PIN_SET);
    }
}

void fpgacfg_reset_fpga(enum fpgacfg_reset reset)
{
    if (reset == RESET_FULL) {
	/* The delay should be at least 250 uS. With HAL_Delay(1) the pulse is very close
	 * to that, and With HAL_Delay(3) the pulse is close to 2 ms. */
	HAL_GPIO_WritePin(FPGA_PROGRAM_Port, FPGA_PROGRAM_Pin, GPIO_PIN_RESET);
	HAL_Delay(3);
	HAL_GPIO_WritePin(FPGA_PROGRAM_Port, FPGA_PROGRAM_Pin, GPIO_PIN_SET);
    } else if (reset == RESET_REGISTERS) {
	HAL_GPIO_WritePin(FPGA_INIT_Port, FPGA_INIT_Pin, GPIO_PIN_SET);
	HAL_Delay(3);
	HAL_GPIO_WritePin(FPGA_INIT_Port, FPGA_INIT_Pin, GPIO_PIN_RESET);
    }
}

int fpgacfg_check_done(void)
{
    GPIO_PinState status = HAL_GPIO_ReadPin(FPGA_DONE_Port, FPGA_DONE_Pin);
    return (status == GPIO_PIN_SET);
}

int fpgacfg_erase_sectors(int num)
{
    if (num > N25Q128_NUM_SECTORS || num < 0) num = N25Q128_NUM_SECTORS;
    while (num) {
	if (! n25q128_erase_sector(&fpgacfg_ctx, num - 1)) {
            return -1;
        }
	num--;
    }
    return 1;
}
