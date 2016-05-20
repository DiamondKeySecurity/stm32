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

int cmd_show_fpga_status(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "FPGA has %sloaded a bitstream", fpgacfg_check_done() ? "":"NOT ");
    return CLI_OK;
}

/* The chunk size have to be a multiple of the SPI flash page size (256 bytes),
   and it has to match the chunk size in the program sending the bitstream over the UART.
*/
#define BITSTREAM_UPLOAD_CHUNK_SIZE 4096

int cmd_fpga_bitstream_upload(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint32_t filesize = 0, crc = 0, my_crc = 0, counter = 0, i;
    uint32_t offset = 0, n = BITSTREAM_UPLOAD_CHUNK_SIZE;
    uint8_t buf[BITSTREAM_UPLOAD_CHUNK_SIZE];

    fpgacfg_access_control(ALLOW_ARM);

    cli_print(cli, "Checking if FPGA config memory is accessible");
    if (fpgacfg_check_id() != 1) {
	cli_print(cli, "ERROR: FPGA config memory not accessible. Check that jumpers JP7 and JP8 are installed.");
	return CLI_ERROR;
    }

    cli_print(cli, "OK, write FPGA bitstream file size (4 bytes), data in 4096 byte chunks, CRC-32 (4 bytes)");

    /* Read file size (4 bytes) */
    uart_receive_bytes(STM_UART_MGMT, (void *) &filesize, 4, 1000);
    cli_print(cli, "File size %li", filesize);

    while (filesize) {
	/* By initializing buf to the same value that erased flash has (0xff), we don't
	 * have to try and be smart when writing the last page of data to the memory.
	 */
	memset(buf, 0xff, 4096);

	if (filesize < n) {
	    n = filesize;
	}

	if (uart_receive_bytes(STM_UART_MGMT, (void *) &buf, n, 1000) != HAL_OK) {
	    cli_print(cli, "Receive timed out");
	    return CLI_ERROR;
	}
	filesize -= n;

	/* After reception of 4 KB but before ACKing we have "all" the time in the world to
	 * calculate CRC and write it to flash.
	 */
	my_crc = update_crc(my_crc, buf, n);

	if ((i = fpgacfg_write_data(offset, buf, BITSTREAM_UPLOAD_CHUNK_SIZE)) != 1) {
	    cli_print(cli, "Failed writing data at offset %li (counter = %li): %li", offset, counter, i);
	    return CLI_ERROR;
	}

	offset += BITSTREAM_UPLOAD_CHUNK_SIZE;

	/* ACK this chunk by sending the current chunk counter (4 bytes) */
	counter++;
	uart_send_bytes(STM_UART_MGMT, (void *) &counter, 4);
    }

    /* The sending side will now send it's calculated CRC-32 */
    cli_print(cli, "Send CRC-32");
    uart_receive_bytes(STM_UART_MGMT, (void *) &crc, 4, 1000);
    cli_print(cli, "CRC-32 %li", crc);
    if (crc == my_crc) {
	cli_print(cli, "CRC checksum MATCHED");
    } else {
	cli_print(cli, "CRC checksum did NOT match");
    }

    fpgacfg_access_control(ALLOW_FPGA);

    return CLI_OK;
}

int cmd_fpga_bitstream_erase(struct cli_def *cli, const char *command, char *argv[], int argc)
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

int cmd_fpga_reset(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    fpgacfg_access_control(ALLOW_FPGA);
    fpgacfg_reset_fpga(RESET_FULL);
    cli_print(cli, "FPGA has been reset");

    return CLI_OK;
}

int cmd_fpga_reset_registers(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    fpgacfg_access_control(ALLOW_FPGA);
    fpgacfg_reset_fpga(RESET_REGISTERS);
    cli_print(cli, "FPGA registers have been reset");

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
    static struct cli_def cli;
    struct cli_command cmd_show_s = {(char *) "show", NULL, 0, NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_show_cpuspeed_s = {(char *) "cpuspeed", cmd_show_cpuspeed, 0,
                                             (char *) "Show the speed at which the CPU currently operates",
                                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_show_fpga_s = {(char *) "fpga", NULL, 0, NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_show_fpga_status_s = {(char *) "status", cmd_show_fpga_status, 0, NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};

    struct cli_command cmd_filetransfer_s = {(char *) "filetransfer", cmd_filetransfer, 0,
                                             (char *) "Test file transfering",
                                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};

    struct cli_command cmd_fpga_s = {(char *) "fpga", NULL, 0, NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_fpga_reset_s = {(char *) "reset", cmd_fpga_reset, 0,
					   (char *) "Reset FPGA (config reset)",
					   PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_fpga_reset_registerss = {(char *) "registers", cmd_fpga_reset_registers, 0,
						    (char *) "Reset FPGA registers (soft reset)",
						    PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_fpga_bitstream_s = {(char *) "bitstream", NULL, 0, NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_fpga_bitstream_upload_s = {(char *) "upload", cmd_fpga_bitstream_upload, 0,
						      (char *) "Upload new FPGA bitstream",
						      PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_fpga_bitstream_erase_s = {(char *) "erase", cmd_fpga_bitstream_erase, 0,
						     (char *) "Erase FPGA config memory",
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
    cli_register_command2(&cli, &cmd_show_fpga_s, &cmd_show_s);
    cli_register_command2(&cli, &cmd_show_fpga_status_s, &cmd_show_fpga_s);

    cli_register_command2(&cli, &cmd_filetransfer_s, NULL);

    cli_register_command2(&cli, &cmd_fpga_s, NULL);
    cli_register_command2(&cli, &cmd_fpga_reset_s, &cmd_fpga_s);
    cli_register_command2(&cli, &cmd_fpga_reset_registerss, &cmd_fpga_reset_s);
    cli_register_command2(&cli, &cmd_fpga_bitstream_s, &cmd_fpga_s);
    cli_register_command2(&cli, &cmd_fpga_bitstream_upload_s, &cmd_fpga_bitstream_s);
    cli_register_command2(&cli, &cmd_fpga_bitstream_erase_s, &cmd_fpga_bitstream_s);

    cli_register_command2(&cli, &cmd_reboot_s, NULL);

    led_off(LED_RED);
    led_on(LED_GREEN);

    embedded_cli_loop(&cli);

    cli_print(&cli, "Rebooting in 4 seconds");
    HAL_Delay(3000);
    HAL_NVIC_SystemReset();

    /* NOT REACHED */
    Error_Handler();
}
