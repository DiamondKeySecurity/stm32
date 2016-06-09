/*
 * mgmt-set.c
 * -----------
 * CLI 'set' commands.
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


int cmd_set_keystore_pin(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    const hal_ks_keydb_t *db;
    hal_user_t user;
    hal_ks_pin_t pin;
    hal_error_t status;

    db = hal_ks_get_keydb();

    if (db == NULL) {
	cli_print(cli, "Could not get a keydb from libhal");
	return CLI_OK;
    }

    if (argc != 3) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: set keystore pin <user|so|wheel> <iterations> <pin>");
	return CLI_ERROR;
    }

    user = HAL_USER_NONE;
    if (strcmp(argv[0], "user") == 0) user = HAL_USER_NORMAL;
    if (strcmp(argv[0], "so") == 0) user = HAL_USER_SO;
    if (strcmp(argv[0], "wheel") == 0) user = HAL_USER_WHEEL;
    if (user == HAL_USER_NONE) {
	cli_print(cli, "First argument must be 'user', 'so' or 'wheel' - not '%s'", argv[0]);
	return CLI_ERROR;
    }

    pin.iterations = strtol(argv[1], NULL, 0);

    /* We don't actually PBKDF2 the given PIN yet, just testing */
    strncpy((char *) pin.pin, argv[2], sizeof(pin.pin));

    if ((status = hal_ks_set_pin(user, &pin)) != LIBHAL_OK) {
	cli_print(cli, "Failed setting PIN: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    return CLI_OK;
}

int cmd_set_keystore_key(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    const hal_ks_keydb_t *db;
    hal_error_t status;
    int hint;

    db = hal_ks_get_keydb();

    if (db == NULL) {
	cli_print(cli, "Could not get a keydb from libhal");
	return CLI_OK;
    }

    if (argc != 2) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: set keystore key <name> <der>");
	return CLI_ERROR;
    }

    hint = 0;
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

void configure_cli_set(struct cli_def *cli)
{
    /* set */
    cli_command_root(set);
    /* set keystore */
    cli_command_branch(set, keystore);

    /* set keystore pin */
    cli_command_node(set_keystore, pin, "Set either 'wheel', 'user' or 'so' PIN");

    /* set keystore key */
    cli_command_node(set_keystore, key, "Set a key");
}
