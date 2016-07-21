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
#define HAL_STATIC_PKEY_STATE_BLOCKS 6
#include "hal_internal.h"
#undef HAL_OK

#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static int cmd_keystore_set_pin(struct cli_def *cli, const char *command, char *argv[], int argc)
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

static int cmd_keystore_clear_pin(struct cli_def *cli, const char *command, char *argv[], int argc)
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

    if (argc != 1) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore clear pin <user|so|wheel>");
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

    memset(&pin, 0x0, sizeof(pin));
    if ((status = hal_ks_set_pin(user, &pin)) != LIBHAL_OK) {
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

#if 0
static int cmd_keystore_set_key(struct cli_def *cli, const char *command, char *argv[], int argc)
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
#endif

static int key_by_index(struct cli_def *cli, char *str, const uint8_t **name, size_t *name_len, hal_key_type_t *type)
{
    char *end;
    long index;

    /* base=0, because someone will try to be clever, and enter '0x0001' */
    index = strtol(str, &end, 0);

    /* If strtol converted the whole string, it's an index.
     * Otherwise, it could be something like "3Mustaphas3".
     */
    if (*end == '\0') {
        const hal_ks_keydb_t *db = hal_ks_get_keydb();
        if (index < 0 || index >= sizeof(db->keys)/sizeof(*db->keys)) {
            cli_print(cli, "Index %ld out of range", index);
            return CLI_ERROR_ARG;
        }
	if (! db->keys[index].in_use) {
            cli_print(cli, "Key %ld not in use", index);
            return CLI_ERROR_ARG;
        }
        *name = db->keys[index].name;
        *name_len = db->keys[index].name_len;
        *type = db->keys[index].type;
        return CLI_OK;
    }
    return CLI_ERROR;
}

static int cmd_keystore_delete_key(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_error_t status;
    int hint = 0;
    const uint8_t *name;
    size_t name_len;
    hal_key_type_t type;

    if (argc != 1) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore delete key <name or index>");
	return CLI_ERROR;
    }

    switch (key_by_index(cli, argv[0], &name, &name_len, &type)) {
    case CLI_OK:
        break;
    case CLI_ERROR:
        name = (uint8_t *)argv[0];
        name_len = strlen(argv[0]);
        type = HAL_KEY_TYPE_EC_PUBLIC;
        break;
    default:
        return CLI_ERROR;
    }    

    if ((status = hal_ks_delete(type, name, name_len, &hint)) != LIBHAL_OK) {
        if (status == HAL_ERROR_KEY_NOT_FOUND) {
            /* sigh, try again including the terminal nul */
            if ((status = hal_ks_delete(type, name, name_len+1, &hint)) == LIBHAL_OK) {
                cli_print(cli, "Deleted key %i", hint);
                return CLI_OK;
            }
        }
	cli_print(cli, "Failed deleting key: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    cli_print(cli, "Deleted key %i", hint);

    return CLI_OK;
}

static int cmd_keystore_rename_key(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    hal_error_t status;
    int hint = 0;
    const uint8_t *name;
    size_t name_len;
    hal_key_type_t type;

    if (argc != 2) {
	cli_print(cli, "Wrong number of arguments (%i).", argc);
	cli_print(cli, "Syntax: keystore rename key <name or index> <new name>");
	return CLI_ERROR;
    }

    switch (key_by_index(cli, argv[0], &name, &name_len, &type)) {
    case CLI_OK:
        break;
    case CLI_ERROR:
        name = (uint8_t *)argv[0];
        name_len = strlen(argv[0]);
        type = HAL_KEY_TYPE_EC_PUBLIC;
        break;
    default:
        return CLI_ERROR;
    }    

    if ((status = hal_ks_rename(type, name, name_len, (uint8_t *)argv[1], strlen(argv[1]), &hint)) != LIBHAL_OK) {
        if (status == HAL_ERROR_KEY_NOT_FOUND) {
            /* sigh, try again including the terminal nul */
            if ((status = hal_ks_rename(type, name, name_len+1, (uint8_t *)argv[1], strlen(argv[1]), &hint)) == LIBHAL_OK) {
                cli_print(cli, "Renamed key %i", hint);
                return CLI_OK;
            }
        }
	cli_print(cli, "Failed renaming key: %s", hal_error_string(status));
	return CLI_ERROR;
    }

    cli_print(cli, "Renamed key %i", hint);

    return CLI_OK;
}

static int cmd_keystore_show_keys(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    const hal_ks_keydb_t *db;
    char *type;

    db = hal_ks_get_keydb();

    if (db == NULL) {
	cli_print(cli, "Could not get a keydb from libhal");
	return CLI_OK;
    }

    /* cli_print(cli, "Sizeof db->keys is %i, sizeof one key is %i\n", sizeof(db->keys), sizeof(*db->keys)); */

    for (int i = 0; i < sizeof(db->keys)/sizeof(*db->keys); i++) {
	if (! db->keys[i].in_use) {
	    cli_print(cli, "Key %i, not in use", i);
	} else {
            switch (db->keys[i].type) {
            case HAL_KEY_TYPE_RSA_PRIVATE:
                type = "RSA private";
                break;
            case HAL_KEY_TYPE_RSA_PUBLIC:
                type = "RSA public";
                break;
            case HAL_KEY_TYPE_EC_PRIVATE:
                type = "EC private";
                break;
            case HAL_KEY_TYPE_EC_PUBLIC:
                type = "EC public";
                break;
            default:
                type = "unknown";
                break;
            }
            int printable = 1;
            for (int j = 0; j < db->keys[i].name_len; ++j) {
                if (!isprint(db->keys[i].name[j])) {
                    printable = 0;
                    break;
                }
            }
            if (printable) {
                /* name may not be nul-terminated in the db, and %*s
                 * doesn't seem to be working properly, so copy it
                 */
                uint8_t name[db->keys[i].name_len + 1];
                memcpy(name, db->keys[i].name, db->keys[i].name_len);
                name[db->keys[i].name_len] = '\0';
                cli_print(cli, "Key %i, type %s, name '%s'", i, type, name);
            }
            else {
                /* hexdump name */
                uint8_t name[db->keys[i].name_len * 3];
                for (int j = 0; j < db->keys[i].name_len; ++j) {
                    uint8_t b = db->keys[i].name[j];
                    #define hexify(n) (((n) < 10) ? ((n) + '0') : ((n) - 10 + 'A'))
                    name[j*3] = hexify((b & 0xf0) >> 4);
                    name[j*3+1] = hexify(b & 0x0f);
                    name[j*3+2] = ':';
                }
                name[sizeof(name)-1] = '\0';
                cli_print(cli, "Key %i, type %s, name %s", i, type, name);
            }
	}
    }

    cli_print(cli, "\nPins:");
    cli_print(cli, "Wheel iterations: 0x%lx", db->wheel_pin.iterations);
    cli_print(cli, "SO    iterations: 0x%lx", db->so_pin.iterations);
    cli_print(cli, "User  iterations: 0x%lx", db->user_pin.iterations);

    return CLI_OK;
}

static int cmd_keystore_erase(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    int status;

    if (argc != 1) {
	cli_print(cli, "Syntax: keystore erase YesIAmSure");
	return CLI_ERROR;
    }

    if (strcmp(argv[0], "YesIAmSure") == 0) {
	if ((status = keystore_erase_sectors(0, 1)) != 1) {
	    cli_print(cli, "Failed erasing keystore: %i", status);
	} else {
	    cli_print(cli, "Keystore erased (first two sectors at least)");
	}
    } else {
	cli_print(cli, "Keystore NOT erased");
    }

    return CLI_OK;
}

void configure_cli_keystore(struct cli_def *cli)
{
    struct cli_command *c = cli_register_command(cli, NULL, "keystore", NULL, 0, 0, NULL);

    struct cli_command *c_show = cli_register_command(cli, c, "show", NULL, 0, 0, NULL);
    struct cli_command *c_set = cli_register_command(cli, c, "set", NULL, 0, 0, NULL);
    struct cli_command *c_clear = cli_register_command(cli, c, "clear", NULL, 0, 0, NULL);
    struct cli_command *c_delete = cli_register_command(cli, c, "delete", NULL, 0, 0, NULL);
    struct cli_command *c_rename = cli_register_command(cli, c, "rename", NULL, 0, 0, NULL);

    /* keystore show keys */
    cli_register_command(cli, c_show, "keys", cmd_keystore_show_keys, 0, 0, "Show what PINs and keys are in the keystore");

    /* keystore set pin */
    struct cli_command *c_set_pin = cli_register_command(cli, c_set, "pin", cmd_keystore_set_pin, 0, 0, "Set either 'wheel', 'user' or 'so' PIN");

    /* keystore set pin iterations */
    cli_register_command(cli, c_set_pin, "iterations", cmd_keystore_set_pin_iterations, 0, 0, "Set PBKDF2 iterations for PINs");

    /* keystore clear pin */
    cli_register_command(cli, c_clear, "pin", cmd_keystore_clear_pin, 0, 0, "Clear either 'wheel', 'user' or 'so' PIN");

#if 0
    /* keystore set key */
    cli_register_command(cli, c_set, "key", cmd_keystore_set_key, 0, 0, "Set a key");
#endif

    /* keystore rename key */
    cli_register_command(cli, c_rename, "key", cmd_keystore_rename_key, 0, 0, "Rename a key");

    /* keystore delete key */
    cli_register_command(cli, c_delete, "key", cmd_keystore_delete_key, 0, 0, "Delete a key");

    /* keystore erase */
    cli_register_command(cli, c, "erase", cmd_keystore_erase, 0, 0, "Erase the whole keystore");
}
