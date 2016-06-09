/*
 * cli-test.c
 * ---------
 * Test code with a small CLI on the management interface
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
#include "stm-led.h"

#include "mgmt-cli.h"
#include "mgmt-dfu.h"
#include "mgmt-fpga.h"
#include "mgmt-misc.h"
#include "mgmt-show.h"
#include "mgmt-test.h"
#include "mgmt-set.h"

#include <string.h>


/* MGMT UART interrupt receive buffer (data will be put in a larger ring buffer) */
volatile uint8_t uart_rx;


int check_auth(const char *username, const char *password)
{
    if (strcasecmp(username, "ct") != 0)
	return CLI_ERROR;
    if (strcasecmp(password, "ct") != 0)
	return CLI_ERROR;
    return CLI_OK;
}

typedef  void (*pFunction)(void);

/* This is it's own function to make it more convenient to set a breakpoint at it in gdb */
void do_early_dfu_jump(void)
{
    pFunction loaded_app = (pFunction) *dfu_code_ptr;
    /* Set the stack pointer to the correct one for the firmware */
    __set_MSP(*dfu_msp_ptr);
    /* Set the Vector Table Offset Register */
    SCB->VTOR = (uint32_t) dfu_firmware;
    loaded_app();
    while (1);
}

int
main()
{
    static struct cli_def cli;

    /* Check if we've just rebooted in order to jump to the firmware. */
    if (*dfu_control == HARDWARE_EARLY_DFU_JUMP) {
	*dfu_control = 0;
	do_early_dfu_jump();
    }

    stm_init();

    led_on(LED_RED);

    mgmt_cli_init(&cli);
    cli_set_auth_callback(&cli, check_auth);

    configure_cli_show(&cli);
    configure_cli_fpga(&cli);
    configure_cli_test(&cli);
    configure_cli_misc(&cli);
    configure_cli_dfu(&cli);
    configure_cli_set(&cli);

    led_off(LED_RED);
    led_on(LED_GREEN);

    while (1) {
        embedded_cli_loop(&cli);
        /* embedded_cli_loop returns when the user enters 'quit' or 'exit' */
        cli_print(&cli, "\nLogging out...\n");
    }

    /* NOT REACHED */
    Error_Handler();
}
