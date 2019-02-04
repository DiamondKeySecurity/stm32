/*
 * mgmt-tamper.c
 * ---------------
 * Management CLI Device Tamper Upgrade code.
 *
 * Copyright (c) 2019 Diamond Key Security, NFP  All rights reserved.
 *
 * Contains Code from mgmt-bootloader.c
 * 
 * mgmt-bootloader.c
 * ---------------
 * Management CLI bootloader upgrade code.
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

#include "mgmt-cli.h"
#include "mgmt-misc.h"
#include "mgmt-fpga.h"
#include "mgmt-tamper.h"

#undef HAL_OK
#define HAL_OK LIBHAL_OK
#include "hal.h"
#undef HAL_OK

#define TAMPERCFG_SECTOR_SIZE		N25Q128_SECTOR_SIZE

extern hal_user_t user;

static volatile uint32_t dfu_offset = 0;


static HAL_StatusTypeDef _tamper_write_callback(uint8_t *buf, size_t len)
{
    HAL_StatusTypeDef res;

    if ((dfu_offset % TAMPERCFG_SECTOR_SIZE) == 0)
	/* first page in sector, need to erase sector */
	//TODO//if ((res = fpgacfg_erase_sector(dfu_offset / TAMPERCFG_SECTOR_SIZE)) != CMSIS_HAL_OK)
	//TODO//    return res;

    /* fpgacfg_write_data (a thin wrapper around n25q128_write_data)
     * requires the offset and length to be page-aligned. The last chunk
     * will be short, so we pad it out to the full chunk size.
     */
    len = len;
    //TODO//res = fpgacfg_write_data(dfu_offset, buf, BITSTREAM_UPLOAD_CHUNK_SIZE);
    dfu_offset += BITSTREAM_UPLOAD_CHUNK_SIZE;
    //TODO// return res;

    return CMSIS_HAL_OK;
}

static int cmd_tamper_upload(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    command = command;
    argv = argv;
    argc = argc;

    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }

    uint8_t buf[BITSTREAM_UPLOAD_CHUNK_SIZE];

    dfu_offset = 0;

    //TODO//fpgacfg_access_control(ALLOW_ARM);

    //TODO//cli_print(cli, "Checking if FPGA config memory is accessible");
    //TODO//if (fpgacfg_check_id() != CMSIS_HAL_OK) {
	//TODO//cli_print(cli, "ERROR: FPGA config memory not accessible. Check that jumpers JP7 and JP8 are installed.");
	//TODO//return CLI_ERROR;
    //TODO//}

    cli_receive_data(cli, &buf[0], sizeof(buf), _tamper_write_callback);

    //TODO//fpgacfg_access_control(ALLOW_FPGA);

    cli_print(cli, "DFU offset now: %li (%li chunks)", dfu_offset, dfu_offset / BITSTREAM_UPLOAD_CHUNK_SIZE);
    return CLI_OK;
}

static int cmd_tamper_threshold_set_light(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "Light: Permission denied.");
    return CLI_ERROR;
}

static int cmd_tamper_threshold_set_temp(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "Temp: Permission denied.");
    return CLI_ERROR;
}

static int cmd_tamper_threshold_set_accel(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "Accel: Permission denied.");
    return CLI_ERROR;
}

void configure_cli_tamper(struct cli_def *cli)
{
    struct cli_command *c;
    struct cli_command *threshold;
    struct cli_command *threshold_set;

    c = cli_register_command(cli, NULL, "tamper", NULL, 0, 0, NULL);

    // create command for upload
    cli_register_command(cli, c, "upload", cmd_tamper_upload, 0, 0, "Upload new tamper image");

    // create parent for threshold commands
    threshold = cli_register_command(cli, c, "threshold", NULL, 0, 0, NULL);

    // create parent for threshold set commands
    threshold_set = cli_register_command(cli, threshold, "set", NULL, 0, 0, NULL);

    // create threshold set commands
    cli_register_command(cli, threshold_set, "light", cmd_tamper_threshold_set_light, 0, 0, "Set the threshold for light sensors");
    cli_register_command(cli, threshold_set, "temperature", cmd_tamper_threshold_set_temp, 0, 0, "Set the threshold for temparture sensors");
    cli_register_command(cli, threshold_set, "accel", cmd_tamper_threshold_set_accel, 0, 0, "Set the threshold for accelerometer");
}