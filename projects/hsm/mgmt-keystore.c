/*
 * mgmt-keystore.c
 * ---------------
 * CLI 'keystore' commands.
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
#include "stm-keystore.h"
#include "stm-fpgacfg.h"
#include "stm-uart.h"

#include "mgmt-cli.h"
#include "mgmt-show.h"

/* Rename both CMSIS HAL_OK and libhal HAL_OK to disambiguate */
#undef HAL_OK
#define LIBHAL_OK HAL_OK
#include "hal.h"
#define HAL_STATIC_PKEY_STATE_BLOCKS 6
#include "hal_internal.h"
#undef HAL_OK

#include <string.h>


int cmd_keystore_set_pin(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    const hal_ks_keydb_t *db;
    hal_user_t user;
    hal_error_t status;
    hal_client_handle_t client = { -1 };

    db = hal_ks_get_keydb();

    if (db == NULL) {
	cli_print(cli, "Could not get a keydb from libhal");
	return CLI_OK;
    }

    if (argc != 2) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore set pin <user|so|wheel> <pin>");
	return CLI_ERROR;
    }

    user = HAL_USER_NONE;
    if (strcmp(argv[0], "user") == 0)  user = HAL_USER_NORMAL;
    if (strcmp(argv[0], "so") == 0)    user = HAL_USER_SO;
    if (strcmp(argv[0], "wheel") == 0) user = HAL_USER_WHEEL;
    if (user == HAL_USER_NONE) {
	cli_print(cli, "First argument must be 'user', 'so' or 'wheel' - not '%s'", argv[0]);
	return CLI_ERROR;
    }

    status = hal_rpc_set_pin(client, user, argv[1], strlen(argv[1]));
    if (status != LIBHAL_OK) {
	cli_print(cli, "Failed setting PIN: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    return CLI_OK;
}

int cmd_keystore_set_pin_iterations(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_error_t status;
    hal_client_handle_t client = { -1 };

    if (argc != 1) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore set pin iterations <number>");
	return CLI_ERROR;
    }

    status = hal_set_pin_default_iterations(client, strtol(argv[0], NULL, 0));
    if (status != LIBHAL_OK) {
	cli_print(cli, "Failed setting iterations: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    return CLI_OK;
}

int cmd_keystore_set_key(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_error_t status;
    int hint = 0;

    if (argc != 2) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore set key <name> <der>");
	return CLI_ERROR;
    }

    if ((status = hal_ks_store(HAL_KEY_TYPE_EC_PUBLIC,
			       HAL_CURVE_NONE,
			       0,
			       (uint8_t *) argv[0], strlen(argv[0]),
			       (uint8_t *) argv[1], strlen(argv[1]),
			       &hint)) != LIBHAL_OK) {

	cli_print(cli, "Failed storing key: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    cli_print(cli, "Stored key %i", hint);

    return CLI_OK;
}

int cmd_keystore_delete_key(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_error_t status;
    int hint = 0;

    if (argc != 1) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore delete key <name>");
	return CLI_ERROR;
    }

    if ((status = hal_ks_delete(HAL_KEY_TYPE_EC_PUBLIC,
				(uint8_t *) argv[0], strlen(argv[0]),
				&hint)) != LIBHAL_OK) {

	cli_print(cli, "Failed deleting key: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    cli_print(cli, "Deleted key %i", hint);

    return CLI_OK;
}

int cmd_keystore_rename_key(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_error_t status;
    int hint = 0;

    if (argc != 2) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore rename key <name> <new name>");
	return CLI_ERROR;
    }

    if ((status = hal_ks_rename(HAL_KEY_TYPE_EC_PUBLIC,
				(uint8_t *) argv[0], strlen(argv[0]),
				(uint8_t *) argv[1], strlen(argv[1]),
				&hint)) != LIBHAL_OK) {

	cli_print(cli, "Failed renaming key: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    cli_print(cli, "Renamed key %i", hint);

    return CLI_OK;
}

int cmd_keystore_show_data(struct cli_def *cli, const char *command, char *argv[], int argc)
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

    return CLI_OK;
}

int cmd_keystore_show_keys(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    const hal_ks_keydb_t *db;

    db = hal_ks_get_keydb();

    if (db == NULL) {
	cli_print(cli, "Could not get a keydb from libhal");
	return CLI_OK;
    }

    cli_print(cli, "Sizeof db->keys is %i, sizeof one key is %i\n", sizeof(db->keys), sizeof(*db->keys));

    for (int i = 0; i < sizeof(db->keys)/sizeof(*db->keys); i++) {
	if (! db->keys[i].in_use) {
	    cli_print(cli, "Key %i, not in use", i);
	} else {
	    cli_print(cli, "Key %i, in use 0x%x, name '%s' der '%s'",
		      i, db->keys[i].in_use, db->keys[i].name, db->keys[i].der);
	}
    }

    cli_print(cli, "\nPins:");
    cli_print(cli, "Wheel iterations: 0x%lx", db->wheel_pin.iterations);
    cli_print(cli, "SO    iterations: 0x%lx", db->so_pin.iterations);
    cli_print(cli, "User  iterations: 0x%lx", db->user_pin.iterations);
    cli_print(cli, "\n");

    return CLI_OK;
}

void configure_cli_keystore(struct cli_def *cli)
{
    /* keystore */
    cli_command_root(keystore);
    /* keystore set */
    cli_command_branch(keystore, set);
    /* keystore delete */
    cli_command_branch(keystore, delete);
    /* keystore rename */
    cli_command_branch(keystore, rename);
    /* keystore show */
    cli_command_branch(keystore, show);

    /* keystore set pin */
    cli_command_node(keystore_set, pin, "Set either 'wheel', 'user' or 'so' PIN");

    /* keystore set pin iterations */
    cli_command_node(keystore_set_pin, iterations, "Set PBKDF2 iterations for PINs");

    /* keystore set key */
    cli_command_node(keystore_set, key, "Set a key");

    /* keystore delete key */
    cli_command_node(keystore_delete, key, "Delete a key");

    /* keystore rename key */
    cli_command_node(keystore_rename, key, "Rename a key");

    /* keystore show data */
    cli_command_node(keystore_show, data, "Dump the first page from the keystore memory");

    /* keystore show keys */
    cli_command_node(keystore_show, keys, "Show what PINs and keys are in the keystore");
}
