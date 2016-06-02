/*
 * mgmt-misc.c
 * -----------
 * Miscellaneous CLI functions.
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
#include "stm-uart.h"

#include "mgmt-cli.h"
#include "mgmt-misc.h"

#include <string.h>


extern uint32_t update_crc(uint32_t crc, uint8_t *buf, int len);

int cmd_filetransfer(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint32_t filesize = 0, crc = 0, my_crc = 0, n = 256, counter = 0;
    uint8_t buf[256];

    cli_print(cli, "OK, write file size (4 bytes), data in %li byte chunks, CRC-32 (4 bytes)", n);

    uart_receive_bytes(STM_UART_MGMT, (void *) &filesize, 4, 1000);
    cli_print(cli, "File size %li", filesize);

    while (filesize) {
	if (filesize < n) {
	    n = filesize;
	}

	if (uart_receive_bytes(STM_UART_MGMT, (void *) &buf, n, 1000) != HAL_OK) {
	    cli_print(cli, "Receive timed out");
	    return CLI_ERROR;
	}
	filesize -= n;
	my_crc = update_crc(my_crc, buf, n);
	counter++;
	uart_send_bytes(STM_UART_MGMT, (void *) &counter, 4);
    }

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

int cmd_reboot(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "\n\n\nRebooting\n\n\n");
    HAL_NVIC_SystemReset();
    while (1) {};
}

void configure_cli_misc(struct cli_def *cli)
{
    /* filetransfer */
    cli_command_root_node(filetransfer, "Test file transfering");
    /* reboot */
    cli_command_root_node(reboot, "Reboot the STM32");
}

