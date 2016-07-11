/*
 * mgmt-bootloader.c
 * -----------------
 * CLI code for updating the bootloader.
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
#include "stm-init.h"
#include "stm-uart.h"
#include "stm-flash.h"
#include "stm-fpgacfg.h"
#include "sdram-malloc.h"

#include "mgmt-cli.h"
#include "mgmt-misc.h"
#include "mgmt-bootloader.h"

#undef HAL_OK
#define HAL_OK LIBHAL_OK
#include "hal.h"
#undef HAL_OK

extern hal_user_t user;

static int cmd_bootloader_upload(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }

    /* JP7 and JP8 must be installed in order to reprogram the FPGA.
     * We extend this to an enabling mechanism for reflashing the firmware.
     * Unfortunately, we can't read JP7 and JP8 directly, as that just gives
     * us the last things written to them, so we see if we can read the
     * FPGA configuration memory.
     */
    fpgacfg_access_control(ALLOW_ARM);
    if (fpgacfg_check_id() != 1) {
	cli_print(cli, "ERROR: Check that jumpers JP7 and JP8 are installed.");
        return CLI_ERROR;
    }

    uint8_t *filebuf;
    size_t file_len;

    if (cli_receive_data(cli, &filebuf, &file_len, DFU_UPLOAD_CHUNK_SIZE) == CLI_ERROR)
        return CLI_ERROR;

    cli_print(cli, "Writing flash");
    int res = stm_flash_write32(DFU_BOOTLOADER_ADDR, (uint32_t *)filebuf, file_len/4) == 1;
    sdram_free(filebuf);

    if (res) {
        cli_print(cli, "SUCCESS");
        return CLI_OK;
    }
    else {
        cli_print(cli, "FAIL");
        return CLI_ERROR;
    }
}

void configure_cli_bootloader(struct cli_def *cli)
{
    cli_command_root(bootloader);

    cli_command_node(bootloader, upload, "Upload new bootloader image");
}
