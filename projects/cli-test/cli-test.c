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
#include "mgmt-cli.h"

#include <string.h>


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

int cmd_fpga_bitstream_upload(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint32_t filesize = 0, crc = 0, my_crc = 0, n = 4096, counter = 0;
    uint8_t buf[4096];

    fpgacfg_give_access_to_stm32();

    cli_print(cli, "Checking if FPGA config memory is accessible");
    if (n25q128_check_id() != 1) {
	cli_print(cli, "ERROR: FPGA config memory not accessible. Check that jumpers JP7 and JP8 are installed.");
	return CLI_ERROR;
    }

    cli_print(cli, "OK, write FPGA bitstream file size (4 bytes), data in 4096 byte chunks, CRC-32 (4 bytes)");

    uart_receive_bytes(STM_UART_MGMT, (void *) &filesize, 4, 1000);
    cli_print(cli, "File size %li", filesize);

    while (filesize) {
	uint32_t page, offset;
	uint8_t *ptr;

	memset(buf, 0xff, 4096);

	if (filesize < n) {
	    n = filesize;
	}

	if (uart_receive_bytes(STM_UART_MGMT, (void *) &buf, n, 1000) != HAL_OK) {
	    cli_print(cli, "Receive timed out");
	    return CLI_ERROR;
	}
	filesize -= n;
	my_crc = update_crc(my_crc, buf, n);

	if ((counter % (N25Q128_SECTOR_SIZE / 4096)) == 0) {
	    /* first page in sector, need to erase sector */
	    offset = (counter * 4096) / N25Q128_SECTOR_SIZE;
	    if (! n25q128_erase_sector(offset)) {
		cli_print(cli, "Failed erasing sector at offset %li (counter = %li)", offset, counter);
		return CLI_ERROR;
	    }
	    /* XXX add timeout and check for < 0 */
	    while (n25q128_get_wip_flag()) { HAL_Delay(10); };
	}

	ptr = buf;
	for (page = 0; page < 4096 / N25Q128_PAGE_SIZE; page++) {
	    offset = counter * (4096 / N25Q128_PAGE_SIZE) + page;
	    if (! n25q128_write_page(offset, ptr)) {
		cli_print(cli, "Failed writing page %li at offset %li (counter = %li)", page, offset, counter);
		return CLI_ERROR;
	    }
	    ptr += N25Q128_PAGE_SIZE;

	    /* XXX add timeout and check for < 0 */
	    while (n25q128_get_wip_flag()) { HAL_Delay(10); };

	    /* XXX read back data and verify it */
	}

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

    fpgacfg_give_access_to_fpga();

    return CLI_OK;
}

int cmd_reboot(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "\n\n\nRebooting\n\n\n");
    HAL_NVIC_SystemReset();
    while (1) {};
}

int check_auth(const char *username, const char *password)
{
    if (strcasecmp(username, "ct") != 0)
	return CLI_ERROR;
    if (strcasecmp(password, "ct") != 0)
	return CLI_ERROR;
    return CLI_OK;
}

int
main()
{
    int i;
    static struct cli_def cli;
    struct cli_command cmd_show_s = {(char *) "show", NULL, 0, NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_show_cpuspeed_s = {(char *) "cpuspeed", cmd_show_cpuspeed, 0,
                                             (char *) "Show the speed at which the CPU currently operates",
                                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_filetransfer_s = {(char *) "filetransfer", cmd_filetransfer, 0,
                                             (char *) "Test file transfering",
                                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};

    struct cli_command cmd_fpga_s = {(char *) "fpga", NULL, 0, NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_fpga_bitstream_s = {(char *) "bitstream", NULL, 0, NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_fpga_bitstream_upload_s = {(char *) "upload", cmd_fpga_bitstream_upload, 0,
						      (char *) "Upload new FPGA bitstream",
						      PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_reboot_s = {(char *) "reboot", cmd_reboot, 0,
				       (char *) "Reboot the STM32",
				       PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};

    stm_init();

    led_on(LED_RED);

    mgmt_cli_init(&cli);
    led_on(LED_YELLOW);
    cli_set_auth_callback(&cli, check_auth);

    cli_register_command2(&cli, &cmd_show_s, NULL);
    cli_register_command2(&cli, &cmd_show_cpuspeed_s, &cmd_show_s);

    cli_register_command2(&cli, &cmd_filetransfer_s, NULL);

    cli_register_command2(&cli, &cmd_fpga_s, NULL);
    cli_register_command2(&cli, &cmd_fpga_bitstream_s, &cmd_fpga_s);
    cli_register_command2(&cli, &cmd_fpga_bitstream_upload_s, &cmd_fpga_bitstream_s);

    cli_register_command2(&cli, &cmd_reboot_s, NULL);

    led_off(LED_RED);
    led_on(LED_GREEN);

    embedded_cli_loop(&cli);

    cli_print(&cli, "Rebooting in 3 seconds");
    HAL_Delay(3000);
    HAL_NVIC_SystemReset();

    /* NOT REACHED */
    Error_Handler();
}
