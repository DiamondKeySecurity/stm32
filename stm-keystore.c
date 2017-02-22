/*
 * stm-keystore.c
 * ----------
 * Functions for accessing the keystore memory.
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

#include "stm32f4xx_hal.h"
#include "stm-keystore.h"
#include "stm-init.h"

SPI_HandleTypeDef hspi_keystore;

struct spiflash_ctx keystore_ctx = {&hspi_keystore, KSM_PROM_CS_N_GPIO_Port, KSM_PROM_CS_N_Pin};

int keystore_check_id(void)
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

int keystore_erase_subsector(uint32_t subsector_offset)
{
    return n25q128_erase_subsector(&keystore_ctx, subsector_offset);
}

int keystore_erase_sector(uint32_t sector_offset)
{
    return n25q128_erase_sector(&keystore_ctx, sector_offset);
}

int keystore_erase_bulk(void)
{
    return n25q128_erase_bulk(&keystore_ctx);
}

/*
 * Deprecated, will be removed when we fix libhal/ks_flash.c to use the
 * new function. I love inter-dependent repos.
 */

int keystore_erase_subsectors(uint32_t start, uint32_t stop)
{
    for (int i = start; i <= stop; ++i) {
        if (keystore_erase_subsector(i) != 1)
            return 0;
    }
    return 1;
}
