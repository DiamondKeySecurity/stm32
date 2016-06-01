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
#include "stm32f4xx_hal.h"
#include "stm-init.h"
#include "stm-led.h"
#include "stm-uart.h"
#include "stm-fpgacfg.h"
#include "stm-keystore.h"
#include "stm-sdram.h"
#include "mgmt-cli.h"
#include "mgmt-dfu.h"
#include "mgmt-fpga.h"
#include "test_sdram.h"

#include <string.h>
#include <stdlib.h>


extern uint32_t update_crc(uint32_t crc, uint8_t *buf, int len);


int cmd_show_cpuspeed(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    volatile uint32_t hclk;

    hclk = HAL_RCC_GetHCLKFreq();
    cli_print(cli, "HSE_VALUE:       %li", HSE_VALUE);
    cli_print(cli, "HCLK:            %li (%i MHz)", hclk, (int) hclk / 1000 / 1000);
    cli_print(cli, "SystemCoreClock: %li (%i MHz)", SystemCoreClock, (int) SystemCoreClock / 1000 / 1000);
    return CLI_OK;
}

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

int cmd_show_fpga_status(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "FPGA has %sloaded a bitstream", fpgacfg_check_done() ? "":"NOT ");
    return CLI_OK;
}

int cmd_show_keystore_status(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "Keystore memory is %sonline", (keystore_check_id() != 1) ? "NOT ":"");
    return CLI_OK;
}

int cmd_show_keystore_data(struct cli_def *cli, const char *command, char *argv[], int argc)
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
	if ((i = keystore_erase_sectors(1)) != 1) {
	    cli_print(cli, "Failed erasing the first sector: %li", i);
	    return CLI_ERROR;
	}
	cli_print(cli, "Erase result: %li", i);
    }

    return CLI_OK;
}

int cmd_reboot(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "\n\n\nRebooting\n\n\n");
    HAL_NVIC_SystemReset();
    while (1) {};
}

int cmd_test_sdram(struct cli_def *cli, const char *command, char *argv[], int argc)
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

int check_auth(const char *username, const char *password)
{
    if (strcasecmp(username, "ct") != 0)
	return CLI_ERROR;
    if (strcasecmp(password, "ct") != 0)
	return CLI_ERROR;
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

void configure_cli_test(struct cli_def *cli)
{
    /* test */
    cli_command_root(test);

    /* test sdram */
    cli_command_node(test, sdram, "Run SDRAM tests");
}

static void configure_cli_misc(struct cli_def *cli)
{
    /* filetransfer */
    cli_command_root_node(filetransfer, "Test file transfering");
    /* reboot */
    cli_command_root_node(reboot, "Reboot the STM32");
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

    led_off(LED_RED);
    led_on(LED_GREEN);

    embedded_cli_loop(&cli);

    /* embedded_cli_loop returns when the user enters 'quit' or 'exit' */

    cli_print(&cli, "Rebooting in 4 seconds");
    HAL_Delay(3000);
    HAL_NVIC_SystemReset();

    /* NOT REACHED */
    Error_Handler();
}
