/*
 * mgmt-show.c
 * -----------
 * CLI 'show' functions.
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
#include "stm-keystore.h"
#include "stm-fpgacfg.h"
#include "stm-uart.h"

#include "mgmt-cli.h"
#include "mgmt-show.h"

#include <string.h>


static int cmd_show_cpuspeed(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    volatile uint32_t hclk;

    hclk = HAL_RCC_GetHCLKFreq();
    cli_print(cli, "HSE_VALUE:       %li", HSE_VALUE);
    cli_print(cli, "HCLK:            %li (%i MHz)", hclk, (int) hclk / 1000 / 1000);
    cli_print(cli, "SystemCoreClock: %li (%i MHz)", SystemCoreClock, (int) SystemCoreClock / 1000 / 1000);
    return CLI_OK;
}

static int cmd_show_fpga_status(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "FPGA has %sloaded a bitstream", fpgacfg_check_done() ? "":"NOT ");
    return CLI_OK;
}

static int cmd_show_keystore_status(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "Keystore memory is %sonline", (keystore_check_id() != 1) ? "NOT ":"");
    return CLI_OK;
}

static int cmd_show_keystore_data(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint8_t buf[KEYSTORE_PAGE_SIZE];
    uint32_t i;

    if (keystore_check_id() != 1) {
	cli_print(cli, "ERROR: The keystore memory is not accessible.");
    }

    memset(buf, 0, sizeof(buf));
    if ((i = keystore_read_data(0, buf, sizeof(buf))) != 1) {
	cli_print(cli, "Failed reading first page from keystore memory: %li", i);
	return CLI_ERROR;
    }

    cli_print(cli, "First page from keystore memory:\r\n");
    uart_send_hexdump(STM_UART_MGMT, buf, 0, sizeof(buf) - 1);
    uart_send_string2(STM_UART_MGMT, (char *) "\r\n\r\n");

    for (i = 0; i < 8; i++) {
	if (buf[i] == 0xff) break;  /* never written */
	if (buf[i] != 0x55) break;  /* something other than a tombstone */
    }
    /* As a demo, tombstone byte after byte of the first 8 bytes in the keystore memory
     * (as long as they do not appear to contain real data).
     * If all of them are tombstones, erase the first sector to start over.
     */
    if (i < 8) {
	if (buf[i] == 0xff) {
	    cli_print(cli, "Tombstoning byte %li", i);
	    buf[i] = 0x55;
	    if ((i = keystore_write_data(0, buf, sizeof(buf))) != 1) {
		cli_print(cli, "Failed writing data at offset 0: %li", i);
		return CLI_ERROR;
	    }
	}
    } else {
	cli_print(cli, "Erasing first sector since all the first 8 bytes are tombstones");
	if ((i = keystore_erase_sectors(1, 1)) != 1) {
	    cli_print(cli, "Failed erasing the first sector: %li", i);
	    return CLI_ERROR;
	}
	cli_print(cli, "Erase result: %li", i);
    }

    return CLI_OK;
}

void configure_cli_show(struct cli_def *cli)
{
    /* show */
    cli_command_root(show);

    /* show cpuspeed */
    cli_command_node(show, cpuspeed, "Show the speed at which the CPU currently operates");

    cli_command_branch(show, fpga);
    /* show fpga status*/
    cli_command_node(show_fpga, status, "Show status about the FPGA");

    cli_command_branch(show, keystore);
    /* show keystore status*/
    cli_command_node(show_keystore, status, "Show status of the keystore memory");
    cli_command_node(show_keystore, data, "Show the first page of the keystore memory");
}
