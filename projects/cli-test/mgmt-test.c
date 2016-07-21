/*
 * mgmt-test.c
 * -----------
 * CLI hardware test functions.
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
#include "stm-sdram.h"

#include "mgmt-cli.h"
#include "mgmt-test.h"

#include "test_sdram.h"
#include "test_mkmif.h"

#include <stdlib.h>

static int cmd_test_sdram(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    // run external memory initialization sequence
    HAL_StatusTypeDef status;
    int ok, num_cycles = 1, i, test_completed;

    if (argc == 1) {
	num_cycles = strtol(argv[0], NULL, 0);
	if (num_cycles > 100) num_cycles = 100;
	if (num_cycles < 1) num_cycles = 1;
    }

    cli_print(cli, "Initializing SDRAM");
    status = sdram_init();
    if (status != HAL_OK) {
	cli_print(cli, "Failed initializing SDRAM: %i", (int) status);
	return CLI_OK;
    }

    for (i = 1; i <= num_cycles; i++) {
	cli_print(cli, "Starting SDRAM test (%i/%i)", i, num_cycles);
	test_completed = 0;
	// set LFSRs to some initial value, LFSRs will produce
	// pseudo-random 32-bit patterns to test our memories
	lfsr1 = 0xCCAA5533;
	lfsr2 = 0xCCAA5533;

	cli_print(cli, "Run sequential write-then-read test for the first chip");
	ok = test_sdram_sequential(SDRAM_BASEADDR_CHIP1);
	if (!ok) break;

	cli_print(cli, "Run random write-then-read test for the first chip");
	ok = test_sdram_random(SDRAM_BASEADDR_CHIP1);
	if (!ok) break;

	cli_print(cli, "Run sequential write-then-read test for the second chip");
	ok = test_sdram_sequential(SDRAM_BASEADDR_CHIP2);
	if (!ok) break;

	cli_print(cli, "Run random write-then-read test for the second chip");
	ok = test_sdram_random(SDRAM_BASEADDR_CHIP2);
	if (!ok) break;

	// turn blue led on (testing two chips at the same time)
	led_on(LED_BLUE);

	cli_print(cli, "Run interleaved write-then-read test for both chips at once");
	ok = test_sdrams_interleaved(SDRAM_BASEADDR_CHIP1, SDRAM_BASEADDR_CHIP2);

	led_off(LED_BLUE);
	test_completed = 1;
	cli_print(cli, "SDRAM test (%i/%i) completed\r\n", i, num_cycles);
    }

    if (! test_completed) {
	cli_print(cli, "SDRAM test failed (%i/%i)", i, num_cycles);
    } else {
	cli_print(cli, "SDRAM test completed successfully");
    }

    return CLI_OK;
}

void configure_cli_test(struct cli_def *cli)
{
    struct cli_command *c = cli_register_command(cli, NULL, "test", NULL, 0, 0, NULL);

    /* test sdram */
    cli_register_command(cli, c, "sdram", cmd_test_sdram, 0, 0, "Run SDRAM tests");

    /* test mkmif */
    cli_register_command(cli, c, "mkmif", cmd_test_mkmif, 0, 0, "Run Master Key Memory Interface tests");
}
