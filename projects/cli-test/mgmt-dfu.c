/*
 * mgmt-dfu.c
 * ---------
 * CLI code for looking at, jumping to or erasing the loaded firmware.
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
#include "stm-flash.h"
#include "mgmt-dfu.h"

#include <string.h>

extern uint32_t update_crc(uint32_t crc, uint8_t *buf, int len);

/* Linker symbols are strange in C. Make regular pointers for sanity. */
__IO uint32_t *dfu_control = &CRYPTECH_DFU_CONTROL;
__IO uint32_t *dfu_firmware = &CRYPTECH_FIRMWARE_START;
__IO uint32_t *dfu_firmware_end = &CRYPTECH_FIRMWARE_END;
/* The first word in the firmware is an address to the stack (msp) */
__IO uint32_t *dfu_msp_ptr = &CRYPTECH_FIRMWARE_START;
/* The second word in the firmware is a pointer to the code
 * (points at the Reset_Handler from the linker script).
 */
__IO uint32_t *dfu_code_ptr = &CRYPTECH_FIRMWARE_START + 1;



int cmd_dfu_dump(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "First 256 bytes from DFU application address %p:\r\n", dfu_firmware);

    uart_send_hexdump(STM_UART_MGMT, (uint8_t *) dfu_firmware, 0, 0xff);
    uart_send_string2(STM_UART_MGMT, (char *) "\r\n\r\n");

    return CLI_OK;
}

int cmd_dfu_erase(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    int status;

    cli_print(cli, "Erasing flash sectors %i to %i (address %p to %p) - expect the CLI to crash now",
	      stm_flash_sector_num((uint32_t) dfu_firmware),
	      stm_flash_sector_num((uint32_t) dfu_firmware_end),
	      dfu_firmware,
	      dfu_firmware_end);

    if ((status = stm_flash_erase_sectors((uint32_t) dfu_firmware, (uint32_t) dfu_firmware_end)) != 0) {
	cli_print(cli, "Failed erasing flash sectors (%i)", status);
    }

    return CLI_OK;
}

int cmd_dfu_jump(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint32_t i;
    /* Load first byte from the DFU_FIRMWARE_PTR to verify it contains an IVT before
     * jumping there.
     */
    cli_print(cli, "Checking for application at %p", dfu_firmware);

    i = *dfu_msp_ptr & 0xFF000000;
    /* 'new_msp' is supposed to be a pointer to the new applications stack, it should
     * point either at RAM (0x20000000) or at the CCM memory (0x10000000).
     */
    if (i == 0x20000000 || i == 0x10000000) {
	/* Set dfu_control to the magic value that will cause the us to jump to the
	 * firmware from the CLI main() function after rebooting.
	 */
	*dfu_control = HARDWARE_EARLY_DFU_JUMP;
	cli_print(cli, "Making the leap");
	HAL_NVIC_SystemReset();
	while (1) { ; }
    } else {
	cli_print(cli, "No loaded application found at %p (read 0x%x)",
		  dfu_firmware, (unsigned int) *dfu_msp_ptr);
    }

    return CLI_OK;
}

void configure_cli_dfu(struct cli_def *cli)
{
    cli_command_root(dfu);

    cli_command_node(dfu, dump, "Show the first 256 bytes of the loaded firmware");
    cli_command_node(dfu, jump, "Jump to the loaded firmware");
    cli_command_node(dfu, erase, "Erase the firmware memory (will crash the CLI)");
}
