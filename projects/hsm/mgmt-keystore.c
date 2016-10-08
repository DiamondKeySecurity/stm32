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

/* Rename both CMSIS HAL_OK and libhal HAL_OK to disambiguate */
#define HAL_OK CMSIS_HAL_OK
#include "stm-init.h"
#include "stm-keystore.h"
#include "stm-fpgacfg.h"
#include "stm-uart.h"

#include "mgmt-cli.h"

#undef HAL_OK
#define LIBHAL_OK HAL_OK
#include "hal.h"
#warning Really should not be including hal_internal.h here, fix API instead of bypassing it
#include "hal_internal.h"
#undef HAL_OK

#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static int cmd_keystore_set_pin(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_user_t user;
    hal_error_t status;
    hal_client_handle_t client = { -1 };

    if (argc != 2) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore set pin <user|so|wheel> <pin>");
	return CLI_ERROR;
    }

    if (strcmp(argv[0], "user") == 0)
	user = HAL_USER_NORMAL;
    else if (strcmp(argv[0], "so") == 0)
	user = HAL_USER_SO;
    else if (strcmp(argv[0], "wheel") == 0)
	user = HAL_USER_WHEEL;
    else {
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

static int cmd_keystore_clear_pin(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_user_t user;
    hal_error_t status;
    hal_client_handle_t client = { -1 };

    if (argc != 1) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore clear pin <user|so|wheel>");
	return CLI_ERROR;
    }

    user = HAL_USER_NONE;
    if (strcmp(argv[0], "user") == 0)
	user = HAL_USER_NORMAL;
    else if (strcmp(argv[0], "so") == 0)
	user = HAL_USER_SO;
    else if (strcmp(argv[0], "wheel") == 0)
	user = HAL_USER_WHEEL;
    else {
	cli_print(cli, "First argument must be 'user', 'so' or 'wheel' - not '%s'", argv[0]);
	return CLI_ERROR;
    }

    if ((status = hal_rpc_set_pin(client, user, "", 0)) != LIBHAL_OK) {
        cli_print(cli, "Failed clearing PIN: %s", hal_error_string(status));
        return CLI_ERROR;
    }

    return CLI_OK;
}

static int cmd_keystore_set_pin_iterations(struct cli_def *cli, const char *command, char *argv[], int argc)
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

static int cmd_keystore_delete_key(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    const hal_client_handle_t  client  = { HAL_HANDLE_NONE };
    const hal_session_handle_t session = { HAL_HANDLE_NONE };
    hal_pkey_handle_t pkey = { HAL_HANDLE_NONE };
    hal_error_t status;
    hal_uuid_t name;

    if (argc != 1) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore delete key <name>");
	return CLI_ERROR;
    }

    if ((status = hal_uuid_parse(&name, argv[0])) != LIBHAL_OK) {
	cli_print(cli, "Couldn't parse key name: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    if ((status = hal_rpc_pkey_find(client, session, &pkey, &name, HAL_KEY_FLAG_TOKEN)) != LIBHAL_OK ||
	(status = hal_rpc_pkey_delete(pkey)) != LIBHAL_OK) {
	cli_print(cli, "Failed deleting key: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    cli_print(cli, "Deleted key %s", argv[0]);

    return CLI_OK;
}

static int show_keys(struct cli_def *cli, const hal_pkey_info_t * const keys, const unsigned n)
{
    char name[HAL_UUID_TEXT_SIZE];
    const char *type, *curve;
    hal_error_t status;

    for (int i = 0; i < n; i++) {

	switch (keys[i].type) {
	case HAL_KEY_TYPE_RSA_PRIVATE:	type = "RSA private";	break;
	case HAL_KEY_TYPE_RSA_PUBLIC:	type = "RSA public";	break;
	case HAL_KEY_TYPE_EC_PRIVATE:	type = "EC private";	break;
	case HAL_KEY_TYPE_EC_PUBLIC:	type = "EC public";	break;
	default:			type = "unknown";	break;
	}

	switch (keys[i].curve) {
	case HAL_CURVE_NONE:		curve = "none";		break;
	case HAL_CURVE_P256:		curve = "P-256";	break;
	case HAL_CURVE_P384:		curve = "P-384";	break;
	case HAL_CURVE_P521:		curve = "P-521";	break;
	default:			curve = "unknown";	break;
	}

	if ((status = hal_uuid_format(&keys[i].name, name, sizeof(name))) != LIBHAL_OK) {
	    cli_print(cli, "Could not convert key name: %s", hal_error_string(status));
	    return CLI_ERROR;
	}

	cli_print(cli, "Key %2i, name %s, type %s, curve %s, flags 0x%lx",
		  i, name, type, curve, (unsigned long) keys[i].flags);

    }

    return CLI_OK;
}

static int cmd_keystore_show_keys(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_pkey_info_t keys[64];
    unsigned n;
    hal_error_t status;
    hal_session_handle_t session = {HAL_HANDLE_NONE};

    if ((status = hal_rpc_pkey_list(session, keys, &n, sizeof(keys)/sizeof(*keys), 0)) != LIBHAL_OK) {
	cli_print(cli, "Could not fetch memory key info: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    cli_print(cli, "Memory keystore:");

    if (show_keys(cli, keys, n) != CLI_OK)
	return CLI_ERROR;

    if ((status = hal_rpc_pkey_list(session, keys, &n, sizeof(keys)/sizeof(*keys), HAL_KEY_FLAG_TOKEN)) != LIBHAL_OK) {
	cli_print(cli, "Could not fetch token key info: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    cli_print(cli, "Token keystore:");

    if (show_keys(cli, keys, n) != CLI_OK)
	return CLI_ERROR;

    return CLI_OK;
}

static int cmd_keystore_erase(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    int status;

    if (argc != 1 || strcmp(argv[0], "YesIAmSure") != 0) {
	cli_print(cli, "Syntax: keystore erase YesIAmSure");
	return CLI_ERROR;
    }

    cli_print(cli, "OK, erasing keystore, this might take a while...");
    if ((status = keystore_erase_sectors(0, KEYSTORE_NUM_SECTORS - 1)) != 1)
        cli_print(cli, "Failed erasing keystore: %i", status);
    else
        cli_print(cli, "Keystore erased");

#warning Should notify libhal/ks_flash that we whacked the keystore

    return CLI_OK;
}

void configure_cli_keystore(struct cli_def *cli)
{
    struct cli_command *c = cli_register_command(cli, NULL, "keystore", NULL, 0, 0, NULL);

    struct cli_command *c_show   = cli_register_command(cli, c, "show",   NULL, 0, 0, NULL);
    struct cli_command *c_set    = cli_register_command(cli, c, "set",    NULL, 0, 0, NULL);
    struct cli_command *c_clear  = cli_register_command(cli, c, "clear",  NULL, 0, 0, NULL);
    struct cli_command *c_delete = cli_register_command(cli, c, "delete", NULL, 0, 0, NULL);

    /* keystore show keys */
    cli_register_command(cli, c_show, "keys", cmd_keystore_show_keys, 0, 0, "Show what PINs and keys are in the keystore");

    /* keystore set pin */
    struct cli_command *c_set_pin = cli_register_command(cli, c_set, "pin", cmd_keystore_set_pin, 0, 0, "Set either 'wheel', 'user' or 'so' PIN");

    /* keystore set pin iterations */
    cli_register_command(cli, c_set_pin, "iterations", cmd_keystore_set_pin_iterations, 0, 0, "Set PBKDF2 iterations for PINs");

    /* keystore clear pin */
    cli_register_command(cli, c_clear, "pin", cmd_keystore_clear_pin, 0, 0, "Clear either 'wheel', 'user' or 'so' PIN");

    /* keystore delete key */
    cli_register_command(cli, c_delete, "key", cmd_keystore_delete_key, 0, 0, "Delete a key");

    /* keystore erase */
    cli_register_command(cli, c, "erase", cmd_keystore_erase, 0, 0, "Erase the whole keystore");
}
