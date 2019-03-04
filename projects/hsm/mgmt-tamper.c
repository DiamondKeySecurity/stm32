/*
 * mgmt-tamper.c
 * ---------------
 * Management CLI Device Tamper Upgrade code.
 *
 * Copyright (c) 2019 Diamond Key Security, NFP  All rights reserved.
 *
 * Contains Code from mgmt-fpga.c
 * 
 * mgmt-fpga.c
 * -----------
 * CLI code to manage the FPGA configuration etc.
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
#include "stm-uart.h"
#include "stm-flash.h"
#include "stm-fpgacfg.h"

#include "mgmt-cli.h"
#include "mgmt-misc.h"
#include "mgmt-fpga.h"
#include "mgmt-tamper.h"

#undef HAL_OK
#define HAL_OK LIBHAL_OK
#include "hal.h"
#undef HAL_OK

#define TAMPERCFG_SECTOR_SIZE		N25Q128_SECTOR_SIZE

extern hal_user_t user;

static HAL_StatusTypeDef _tamper_write_callback(uint8_t *buf, size_t len)
// This uses the same pattern as _flash_write_callback, but it should only
// be called once because the chunk size is the firmware size.
{
    // int result = flash_tamper_firmware(buf);
    // TODO evaluate result

    return CMSIS_HAL_OK;
}

static int cmd_tamper_upload(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    command = command;
    argv = argv;
    argc = argc;

    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }

    uint8_t buf[TAMPER_BITSTREAM_UPLOAD_CHUNK_SIZE];
    memset((void *)&buf[0], 0xff, TAMPER_BITSTREAM_UPLOAD_CHUNK_SIZE);

    cli_receive_data(cli, &buf[0], sizeof(buf), _tamper_write_callback);

    uint32_t uploaded = TAMPER_BITSTREAM_UPLOAD_CHUNK_SIZE;

    cli_print(cli, "Tamper uploaded: %li (%li chunks)", uploaded, uploaded / TAMPER_BITSTREAM_UPLOAD_CHUNK_SIZE);
    return CLI_OK;
}

/* Write a chunk of received data to flash. */
typedef int (*set_param_callback)(int argv[], int argc);

/*
set_tamper_attribute
Used to parse and verify parameters and call set functions
*/
int set_tamper_attribute(struct cli_def *cli, const char *name, char *argv[], int argc, int num_args,
                         int min_value, int max_value, set_param_callback set_callback)
{
    int parsed_parameters[argc];

    if (num_args != argc)
    {
        cli_print(cli, "Error: Set %s must have exactly %i argument(s)", name, num_args);
        return CLI_ERROR;
    }

    // parse all of the parameters
    for (int i = 0; i < argc; ++i)
    {
        int parse_result = sscanf(argv[i], "%i", &parsed_parameters[i]);

        if (parse_result != 1)
        {
            cli_print(cli, "Error: Attribute index:%i, %s is not a number. (result == %i)", i, argv[i], parse_result);
        }
        else if (parsed_parameters[i] < min_value)
        {
            cli_print(cli, "Error: Attribute index:%i is less than the min value", i);
        }
        else if (parsed_parameters[i] > max_value)
        {
            cli_print(cli, "Error: Attribute index:%i is greater than the max value", i);
        }
        else
        {
            // parsed correctly
            continue;
        }

        // parser error
        return CLI_ERROR;
    }

    // use callback to set the actual value
    if (set_callback(parsed_parameters, argc) != 0)
    {
        cli_print(cli, "Error setting %s", name);
        for(int n = 0; n < argc; ++n)
        {
            cli_print(cli, "%i", parsed_parameters[n]);
        }
        return CLI_ERROR;
    }

    cli_print(cli, "%s set", name);

    return CLI_OK;
}

static int set_tamper_threshold_light(int argv[], int argc)
{
    // the arguments have been parsed and validated
    return -1;
}

static int set_tamper_threshold_temperature(int argv[], int argc)
{
    // the arguments have been parsed and validated
    return -1;
}

static int set_tamper_threshold_accelerometer(int argv[], int argc)
{
    // the arguments have been parsed and validated
    return -1;
}

static int cmd_tamper_threshold_set_light(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    return set_tamper_attribute(cli,
                                "light threshold",
                                argv,
                                argc,
                                1,
                                0,
                                100,
                                set_tamper_threshold_light);
}

static int cmd_tamper_threshold_set_temp(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    return set_tamper_attribute(cli,
                                "temperature threshold",
                                argv,
                                argc,
                                2,
                                0,
                                100,
                                set_tamper_threshold_temperature);
}

static int cmd_tamper_threshold_set_accel(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    return set_tamper_attribute(cli,
                                "accelerometer threshold",
                                argv,
                                argc,
                                1,
                                0,
                                100,
                                set_tamper_threshold_accelerometer);
}

void configure_cli_tamper(struct cli_def *cli)
{
    struct cli_command *c;
    struct cli_command *threshold;
    struct cli_command *threshold_set;

    c = cli_register_command(cli, NULL, "tamper", NULL, 0, 0, NULL);

    // create command for upload
    cli_register_command(cli, c, "upload", cmd_tamper_upload, 0, 0, "Upload new tamper image");

    // create parent for threshold commands
    threshold = cli_register_command(cli, c, "threshold", NULL, 0, 0, NULL);

    // create parent for threshold set commands
    threshold_set = cli_register_command(cli, threshold, "set", NULL, 0, 0, NULL);

    // create threshold set commands
    cli_register_command(cli, threshold_set, "light", cmd_tamper_threshold_set_light, 0, 0, "Set the threshold for light sensors");
    cli_register_command(cli, threshold_set, "temperature", cmd_tamper_threshold_set_temp, 0, 0, "Set the threshold for temparture sensors");
    cli_register_command(cli, threshold_set, "accel", cmd_tamper_threshold_set_accel, 0, 0, "Set the threshold for accelerometer");
}