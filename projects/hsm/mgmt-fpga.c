/*
 * mgmt-fpga.c
 * -----------
 * CLI code to manage the FPGA configuration etc.
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
#include "stm-fpgacfg.h"

#include "mgmt-cli.h"
#include "mgmt-fpga.h"
#include "mgmt-misc.h"

#undef HAL_OK
#define HAL_OK LIBHAL_OK
#include "hal.h"
#undef HAL_OK

#include <string.h>


extern hal_user_t user;

static volatile uint32_t dfu_offset = 0;


static int _flash_write_callback(uint8_t *buf, size_t len) {
    int res = fpgacfg_write_data(dfu_offset, buf, BITSTREAM_UPLOAD_CHUNK_SIZE) == 1;
    dfu_offset += BITSTREAM_UPLOAD_CHUNK_SIZE;
    return res;
}

static int cmd_fpga_bitstream_upload(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }

    uint8_t buf[BITSTREAM_UPLOAD_CHUNK_SIZE];

    dfu_offset = 0;

    fpgacfg_access_control(ALLOW_ARM);

    cli_print(cli, "Checking if FPGA config memory is accessible");
    if (fpgacfg_check_id() != 1) {
	cli_print(cli, "ERROR: FPGA config memory not accessible. Check that jumpers JP7 and JP8 are installed.");
	return CLI_ERROR;
    }

    cli_receive_data(cli, &buf[0], sizeof(buf), _flash_write_callback);

    fpgacfg_access_control(ALLOW_FPGA);

    cli_print(cli, "DFU offset now: %li (%li chunks)", dfu_offset, dfu_offset / BITSTREAM_UPLOAD_CHUNK_SIZE);
    return CLI_OK;
}

static int cmd_fpga_bitstream_erase(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    fpgacfg_access_control(ALLOW_ARM);

    cli_print(cli, "Checking if FPGA config memory is accessible");
    if (fpgacfg_check_id() != 1) {
	cli_print(cli, "ERROR: FPGA config memory not accessible. Check that jumpers JP7 and JP8 are installed.");
	return CLI_ERROR;
    }

    /* Erasing the whole config memory takes a while, we just need to erase the first sector.
     * The bitstream has an EOF marker, so even if the next bitstream uploaded is shorter than
     * the current one there should be no problem.
     *
     * This command could be made to accept an argument indicating the whole memory should be erased.
     */
    if (! fpgacfg_erase_sectors(1)) {
	cli_print(cli, "Erasing first sector in FPGA config memory failed");
	return CLI_ERROR;
    }

    cli_print(cli, "Erased FPGA config memory");
    fpgacfg_access_control(ALLOW_FPGA);

    return CLI_OK;
}

static int cmd_fpga_reset(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    fpgacfg_access_control(ALLOW_FPGA);
    fpgacfg_reset_fpga(RESET_FULL);
    cli_print(cli, "FPGA has been reset");

    return CLI_OK;
}

static int cmd_fpga_reset_registers(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    fpgacfg_access_control(ALLOW_FPGA);
    fpgacfg_reset_fpga(RESET_REGISTERS);
    cli_print(cli, "FPGA registers have been reset");

    return CLI_OK;
}

static int cmd_fpga_show_status(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "FPGA has %sloaded a bitstream", fpgacfg_check_done() ? "":"NOT ");
    return CLI_OK;
}

static int cmd_fpga_show_cores(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    const hal_core_t *core;
    const hal_core_info_t *info;

    for (core = hal_core_iterate(NULL); core != NULL; core = hal_core_iterate(core)) {
	info = hal_core_info(core);
	cli_print(cli, "%04x: %8.8s %4.4s",
                  (unsigned int)info->base, info->name, info->version);
    }

    return CLI_OK;
}

void configure_cli_fpga(struct cli_def *cli)
{
    /* fpga */
    cli_command_root(fpga);

    cli_command_branch(fpga, show);
    /* show fpga status*/
    cli_command_node(fpga_show, status, "Show status about the FPGA");
    /* show fpga cores*/
    cli_command_node(fpga_show, cores, "Show FPGA core names and versions");

    /* fpga reset */
    cli_command_node(fpga, reset, "Reset FPGA (config reset)");
    /* fpga reset registers */
    cli_command_node(fpga_reset, registers, "Reset FPGA registers (soft reset)");

    cli_command_branch(fpga, bitstream);
    /* fpga bitstream upload */
    cli_command_node(fpga_bitstream, upload, "Upload new FPGA bitstream");
    /* fpga bitstream erase */
    cli_command_node(fpga_bitstream, erase, "Erase FPGA config memory");
}
