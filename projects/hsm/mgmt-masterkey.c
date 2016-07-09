/*
 * mgmt-masterkey.c
 * ----------------
 * Masterkey CLI functions.
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

#define HAL_OK CMSIS_HAL_OK

#include "stm-init.h"
#include "stm-uart.h"
#include "mgmt-cli.h"
#include "mgmt-masterkey.h"

/* Rename both CMSIS HAL_OK and libhal HAL_OK to disambiguate */
#undef HAL_OK
#define LIBHAL_OK HAL_OK
#include <hal.h>
#include <masterkey.h>
#undef HAL_OK

#include <stdlib.h>

#define KEK_LENGTH (256 / 8)


static char * _status2str(const hal_error_t status)
{
    switch (status) {
    case LIBHAL_OK:
	return (char *) "Set";
    case HAL_ERROR_MASTERKEY_NOT_SET:
	return (char *) "Not set";
    default:
	return (char *) "Unknown";
    }
}

static int _parse_hex_groups(uint8_t *buf, size_t len, char *argv[], int argc)
{
    int i;
    uint32_t *dst = (uint32_t *) buf;
    uint32_t *end = (uint32_t *) buf + len - 1;
    char *err_ptr = NULL;

    if (! argc) return 0;

    for (i = 0; i < argc; i++) {
	if (dst >= end) return -1;
	*dst++ = strtol(argv[i], &err_ptr, 16);
	if (*err_ptr) return -2;
    }

    return 1;
}

static int cmd_masterkey_status(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_error_t status;

    cli_print(cli, "Status of master key:\n");

    status = masterkey_volatile_read(NULL, 0);
    cli_print(cli, "  volatile: %s / %s", _status2str(status), hal_error_string(status));

    status = masterkey_flash_read(NULL, 0);
    cli_print(cli, "     flash: %s / %s", _status2str(status), hal_error_string(status));

    return CLI_OK;
}

static int cmd_masterkey_set(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint8_t buf[KEK_LENGTH] = {0};
    hal_error_t err;
    int i;

    if ((i = _parse_hex_groups(&buf[0], sizeof(buf), argv, argc)) != 1) {
	cli_print(cli, "Failed parsing master key, expected up to 8 groups of 32-bit hex chars (%i)", i);
	return CLI_OK;
    }

    cli_print(cli, "Parsed key:\n");
    uart_send_hexdump(STM_UART_MGMT, buf, 0, sizeof(buf) - 1);
    cli_print(cli, "\n");

    if ((err = masterkey_volatile_write(buf, sizeof(buf))) == LIBHAL_OK) {
	cli_print(cli, "Master key set in volatile memory");
    } else {
	cli_print(cli, "Failed writing key to volatile memory: %s", hal_error_string(err));
    }
    return CLI_OK;
}

static int cmd_masterkey_erase(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_error_t err;

    if ((err = masterkey_volatile_erase(KEK_LENGTH)) == LIBHAL_OK) {
	cli_print(cli, "Erased master key from volatile memory");
    } else {
	cli_print(cli, "Failed erasing master key from volatile memory: %s", hal_error_string(err));
    }
    return CLI_OK;
}

static int cmd_masterkey_unsecure_set(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    uint8_t buf[KEK_LENGTH] = {0};
    hal_error_t err;
    int i;

    if ((i = _parse_hex_groups(&buf[0], sizeof(buf), argv, argc)) != 1) {
	cli_print(cli, "Failed parsing master key, expected up to 8 groups of 32-bit hex chars (%i)", i);
	return CLI_OK;
    }

    cli_print(cli, "Parsed key:\n");
    uart_send_hexdump(STM_UART_MGMT, buf, 0, sizeof(buf) - 1);
    cli_print(cli, "\n");

    if ((err = masterkey_flash_write(buf, sizeof(buf))) == LIBHAL_OK) {
	cli_print(cli, "Master key set in unsecure flash memory");
    } else {
	cli_print(cli, "Failed writing key to unsecure flash memory: %s", hal_error_string(err));
    }
    return CLI_OK;
}

static int cmd_masterkey_unsecure_erase(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_error_t err;

    if ((err = masterkey_flash_erase(KEK_LENGTH)) == LIBHAL_OK) {
	cli_print(cli, "Erased unsecure master key from flash");
    } else {
	cli_print(cli, "Failed erasing unsecure master key from flash: %s", hal_error_string(err));
    }
    return CLI_OK;
}

void configure_cli_masterkey(struct cli_def *cli)
{
    /* masterkey */
    cli_command_root(masterkey);
    /* masterkey status */
    cli_command_node(masterkey, status, "Show status of master key in RAM/flash");

    /* masterkey set */
    cli_command_node(masterkey, set, "Set the master key in the volatile Master Key Memory");
    /* masterkey erase */
    cli_command_node(masterkey, erase, "Erase the master key from the volatile Master Key Memory");

    cli_command_branch(masterkey, unsecure);
    /* masterkey unsecure set */
    cli_command_node(masterkey_unsecure, set, "Set master key in unprotected flash memory (if unsure, DON'T)");
    /* masterkey unsecure erase */
    cli_command_node(masterkey_unsecure, erase, "Erase master key from unprotected flash memory");
}
