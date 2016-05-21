/*
 * stm-keystore.c
 * ----------
 * Functions for accessing the keystore memory.
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
#include "stm-keystore.h"
#include "stm-init.h"

SPI_HandleTypeDef hspi_keystore;

struct spiflash_ctx keystore_ctx = {&hspi_keystore, KSM_PROM_CS_N_GPIO_Port, KSM_PROM_CS_N_Pin};

int keystore_check_id()
{
    return n25q128_check_id(&keystore_ctx);
}

int keystore_read_data(uint32_t offset, uint8_t *buf, const uint32_t len)
{
    return n25q128_read_data(&keystore_ctx, offset, buf, len);
}

int keystore_write_data(uint32_t offset, const uint8_t *buf, const uint32_t len)
{
    return n25q128_write_data(&keystore_ctx, offset, buf, len);
}

int keystore_erase_sectors(int num)
{
    if (num > N25Q128_NUM_SECTORS - 1 || num < 0) num = N25Q128_NUM_SECTORS - 1;
    while (num) {
	int timeout = 1000;
	while (timeout--) {
	    int i = n25q128_get_wip_flag(&keystore_ctx);
	    if (i < 0) return 0;
	    if (! i) break;
	    HAL_Delay(10);
	}

	if (! n25q128_erase_sector(&keystore_ctx, num--)) {
            return 0;
        }
    }
    return 1;
}
