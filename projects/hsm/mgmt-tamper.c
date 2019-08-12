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
#include "usart3_avrboot.h"

#undef HAL_OK
#define HAL_OK LIBHAL_OK
#include "hal.h"
#undef HAL_OK

#define TAMPERCFG_SECTOR_SIZE		N25Q128_SECTOR_SIZE

extern hal_user_t user;

static volatile uint32_t dfu_offset = 0;

static HAL_StatusTypeDef _tamper_write_callback(uint8_t *buf, size_t len)
// This uses the same pattern as _flash_write_callback, but it should only
// be called once because the chunk size is the firmware size.
{
    int result = avrboot_write_page(buf);
    // TODO evaluate result
    dfu_offset += TAMPER_BITSTREAM_UPLOAD_CHUNK_SIZE;

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

    dfu_offset = 0;

    memset((void *)&buf[0], 0xff, TAMPER_BITSTREAM_UPLOAD_CHUNK_SIZE);

    cli_receive_data(cli, &buf[0], sizeof(buf), _tamper_write_callback);

    uint32_t uploaded = TAMPER_BITSTREAM_UPLOAD_CHUNK_SIZE;

    cli_print(cli, "DFU offset now: %li (%li chunks)", dfu_offset, dfu_offset / TAMPER_BITSTREAM_UPLOAD_CHUNK_SIZE);
    return CLI_OK;
}

static int cmd_tamper_reset(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    command = command;
    argv = argv;
    argc = argc;

    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }

    avrboot_start_tamper();

    cli_print(cli, "Tamper app started");
    return CLI_OK;
}

static int cmd_tamper_check(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    command = command;
    argv = argv;
    argc = argc;

    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }
    uint8_t resp;
    uart_send_char_tamper(&huart_tmpr, 0x4B);
    HAL_Delay(5);
   	HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
    HAL_Delay(200);

	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		cli_print(cli, "Tamper ok");
		return CLI_OK;
	}
	else{
		cli_print(cli, "Tamper not ok");
		return -1;
	}

}

static int cmd_light_check(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    command = command;
    argv = argv;
    argc = argc;
    uint8_t val1, val2, resp;
    signed int temp;
    val1 = 0;
    val2 = 0;
    resp = 0;
    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }

    uart_send_char_tamper(&huart_tmpr, 0x47);
    //HAL_Delay(5);
   	HAL_UART_Receive(&huart_tmpr, &val1, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &val2, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
    HAL_Delay(200);
    temp = (signed int) ((val1<<8) | val2);
	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		cli_print(cli, "Light is %d", temp);
		return CLI_OK;
	}
	else{
		return -1;
	}

}

static int cmd_temp_check(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    command = command;
    argv = argv;
    argc = argc;
    uint8_t val1, val2, resp;
    signed int temp;
    val1 = 0;
    val2 = 0;
    resp = 0;
    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }

    uart_send_char_tamper(&huart_tmpr, 0x48);
    //HAL_Delay(5);
   	HAL_UART_Receive(&huart_tmpr, &val1, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &val2, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
    HAL_Delay(200);
    temp = (signed int) ((val1<<8) | val2);
	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		cli_print(cli, "Temperature is %d", temp);
		return CLI_OK;
	}
	else{
		return -1;
	}

}

static int cmd_vibe_check(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    command = command;
    argv = argv;
    argc = argc;
    uint8_t x_hi, x_lo, y_hi, y_lo, z_hi, z_lo, resp;
    int16_t x, y, z;

    signed int temp;
    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }

    uart_send_char_tamper(&huart_tmpr, 0x4E);
    //HAL_Delay(5);
   	HAL_UART_Receive(&huart_tmpr, &x_hi, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &x_lo, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &y_hi, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &y_lo, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &z_hi, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &z_lo, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
    HAL_Delay(200);
    // the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		x= (int16_t)x_hi<<8 | (int16_t)x_lo;
		y= (int16_t)y_hi<<8 | (int16_t)y_lo;
		z= (int16_t)z_hi<<8 | (int16_t)z_lo;
		cli_print(cli, "Vibe is\n x = %d\n y = %d\n z = %d\n", x, y, z);
		return CLI_OK;
	}
	else{
		return -1;
	}

}

static int cmd_set_config(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    command = command;
    argv = argv;
    argc = argc;
    uint8_t resp;
    signed int temp;
    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }

    uart_send_char_tamper(&huart_tmpr, 0x4A);
    HAL_Delay(5);
   	HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);

	if (resp == AVRBOOT_STK_INSYNC) {
		cli_print(cli, "Configuration is set");
		return CLI_OK;
	}
	else{
		return -1;
	}

}

static int cmd_chk_fault(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    command = command;
    argv = argv;
    argc = argc;
    uint8_t resp, fault_code, fault_val1, fault_val2;
    signed int temp;
    if (user < HAL_USER_SO) {
        cli_print(cli, "Permission denied.");
        return CLI_ERROR;
    }

    uart_send_char_tamper(&huart_tmpr, 0x4C);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &fault_code, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &fault_val1, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &fault_val2, 1, 1000);
    //HAL_Delay(5);
    HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
    HAL_Delay(200);
    temp = (signed int) (fault_val1 | fault_val2);
	if (resp == AVRBOOT_STK_INSYNC) {
		if (fault_code == LIGHT){
			cli_print(cli, "Light tamper value is %i", temp);
		}
		else if (fault_code == TEMP){
			cli_print(cli, "Temperature tamper value is %i", temp);
		}
		else if (fault_code == VIBE){
					cli_print(cli, "Vibe, check vibe values");
		}
		else if (fault_code == CASE){
							cli_print(cli, "Case open");
		}
		else if (fault_code == USART){
			cli_print(cli, "USART failure is %i", temp);
		}
		else if (fault_code == LL){
					cli_print(cli, "Low line (battery)");
		}
		else if (fault_code == 0){
					cli_print(cli, "No faults detected");
		}
		else {
			cli_print(cli, "fault code is %d, fault1 val is %d, fault2 val is %d", fault_code, fault_val1, fault_val2);
		}
//		cli_print(cli, "Configuration is set", temp);
		/*#define LIGHT		0x01
#define TEMP		0x02
#define VIBE		0x04
#define CASE		0x08
#define SSP			0x10
#define LL			0x20
#define USART		0x40
#define UNK			0x80*/

		return CLI_OK;
	}
	else{
		return -1;
	}

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
	uint8_t resp;
	uint8_t val1, val2;

	val1 = (uint8_t)argv[0];
	val2 = (uint8_t)(argv[0]>>8);
	uart_send_char_tamper(&huart_tmpr, 0x41);
	HAL_Delay(5);

	uart_send_char_tamper(&huart_tmpr, val2);
	HAL_Delay(5);

	uart_send_char_tamper(&huart_tmpr, val1);
	HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
	HAL_Delay(200);

	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		return 0;
	}
	else{
		return -1;
	}
}

static int set_tamper_threshold_temp_hi(int argv[], int argc)
{
	uint8_t resp;
	uint8_t val1;

	val1 = (uint8_t)argv[0];

	uart_send_char_tamper(&huart_tmpr, 0x42);
	HAL_Delay(5);

	uart_send_char_tamper(&huart_tmpr, val1);
	HAL_Delay(5);
	HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
	HAL_Delay(200);

	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		return 0;
	}
	else{
		return -1;
	}
}

static int set_tamper_threshold_temp_lo(int argv[], int argc)
{
	uint8_t resp;
	uint8_t val1;

	val1 = (uint8_t)argv[0];
	uart_send_char_tamper(&huart_tmpr, 0x43);
	HAL_Delay(5);

	uart_send_char_tamper(&huart_tmpr, val1);
	HAL_Delay(5);
	HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
	HAL_Delay(200);

	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		return 0;
	}
	else{
		return -1;
	}
}

static int set_tamper_threshold_accelerometer(int argv[], int argc)
{
	uint8_t resp;
	uint8_t val1, val2;

	val1 = (uint8_t)argv[0];
	val2 = (uint8_t)(argv[0]>>8);
	uart_send_char_tamper(&huart_tmpr, 0x44);
	HAL_Delay(5);

	uart_send_char_tamper(&huart_tmpr, val1);
	HAL_Delay(5);

	uart_send_char_tamper(&huart_tmpr, val2);
	HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
	HAL_Delay(200);

	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		return 0;
	}
	else{
		return -1;
	}
}

static int set_tamper_disable(int argv[], int argc)
{
	uint8_t resp;
	uint8_t val1, val2;

	val1 = (uint8_t)argv[0];

	uart_send_char_tamper(&huart_tmpr, 0x46);
	HAL_Delay(5);

	uart_send_char_tamper(&huart_tmpr, val1);
	HAL_Delay(5);
	HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
	HAL_Delay(200);

	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		return 0;
	}
	else{
		return -1;
	}
}

static int set_tamper_enable(int argv[], int argc)
{
	uint8_t resp;
	uint8_t val1, val2;

	val1 = (uint8_t)argv[0];

	uart_send_char_tamper(&huart_tmpr, 0x45);
	HAL_Delay(5);

	uart_send_char_tamper(&huart_tmpr, val1);
	HAL_Delay(5);
	HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
	HAL_Delay(200);

	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		return 0;
	}
	else{
		return -1;
	}
}

static int set_battery_enable(int argv[], int argc)
{
	uint8_t resp;
	uint8_t val1;

	val1 = (uint8_t)argv[0];

	uart_send_char_tamper(&huart_tmpr, 0x4F);
	HAL_Delay(5);

	uart_send_char_tamper(&huart_tmpr, val1);
	HAL_Delay(5);
	HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
	HAL_Delay(200);

	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		return 0;
	}
	else{
		return -1;
	}
}
//7057 mdockter63 TmobTippy1
static int cmd_tamper_threshold_set_light(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    return set_tamper_attribute(cli,
                                "light threshold",
                                argv,
                                argc,
                                1,
                                0,
                                65535,
                                set_tamper_threshold_light);
}

static int cmd_tamper_threshold_set_temp_hi(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    return set_tamper_attribute(cli,
                                "temperature threshold",
                                argv,
                                argc,
                                1,
                                0,
                                250,
                                set_tamper_threshold_temp_hi);
}
static int cmd_tamper_threshold_set_temp_lo(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    return set_tamper_attribute(cli,
                                "temperature threshold",
                                argv,
                                argc,
                                1,
                                0,
                                250,
                                set_tamper_threshold_temp_lo);
}

static int cmd_tamper_threshold_set_accel(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    return set_tamper_attribute(cli,
                                "accelerometer threshold",
                                argv,
                                argc,
                                1,
                                0,
                                65535,
                                set_tamper_threshold_accelerometer);
}

static int cmd_tamper_set_disable(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    return set_tamper_attribute(cli,
                                "tamper disable",
                                argv,
                                argc,
                                1,
                                0,
                                255,
                                set_tamper_disable);
}
static int cmd_tamper_set_enable(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    return set_tamper_attribute(cli,
                                "tamper enable",
                                argv,
                                argc,
                                1,
                                0,
                                255,
                                set_tamper_enable);
}

static int cmd_tamper_set_battery(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    return set_tamper_attribute(cli,
                                "battey on/off",
                                argv,
                                argc,
                                1,
                                0,
                                1,
								set_battery_enable);
}

void configure_cli_tamper(struct cli_def *cli)
{
    struct cli_command *c;
    struct cli_command *threshold;
    struct cli_command *threshold_set;

    c = cli_register_command(cli, NULL, "tamper", NULL, 0, 0, NULL);

    // create command for upload
    cli_register_command(cli, c, "upload", cmd_tamper_upload, 0, 0, "Upload new tamper image");

    // create command for tamper app reset
    cli_register_command(cli, c, "reset", cmd_tamper_reset, 0, 0, "Start tamper application");

    // create command for tamper app event polling
    cli_register_command(cli, c, "check", cmd_tamper_check, 0, 0, "Check tamper application");

    //create command for reading light sensor
    cli_register_command(cli, c, "light value", cmd_light_check, 0, 0, "Check light value");

    //create command for reading temperature sensor
    cli_register_command(cli, c, "temperature value", cmd_temp_check, 0, 0, "Check temperature value");

    //create command for reading vibe sensor
    cli_register_command(cli, c, "vibe value", cmd_vibe_check, 0, 0, "Check vibe value");

    //create command for setting config to release MKM and begin tamper detection
    cli_register_command(cli, c, "set config", cmd_set_config, 0, 0, "Set configuration");

    //create command for checking tamper fault
    cli_register_command(cli, c, "faults", cmd_chk_fault, 0, 0, "Get fault(s)");

    // create parent for threshold commands
    threshold = cli_register_command(cli, c, "threshold", NULL, 0, 0, NULL);

    // create parent for threshold set commands
    threshold_set = cli_register_command(cli, threshold, "set", NULL, 0, 0, NULL);

    // create threshold set commands
    cli_register_command(cli, threshold_set, "light", cmd_tamper_threshold_set_light, 0, 0, "Set the threshold for light sensors");
    cli_register_command(cli, threshold_set, "temphi", cmd_tamper_threshold_set_temp_hi, 0, 0, "Set the hi threshold for temperature sensor");
    cli_register_command(cli, threshold_set, "templo", cmd_tamper_threshold_set_temp_lo, 0, 0, "Set the lo threshold for temperature sensor");
    cli_register_command(cli, threshold_set, "accel", cmd_tamper_threshold_set_accel, 0, 0, "Set the threshold for accelerometer");
    cli_register_command(cli, threshold_set, "disable", cmd_tamper_set_disable, 0, 0, "Disable specific tamper functions");// create command for tamper sensor disable
    cli_register_command(cli, threshold_set, "enable", cmd_tamper_set_enable, 0, 0, "Enable specific tamper functions");// create command for tamper sensor disable
    cli_register_command(cli, threshold_set, "battery", cmd_tamper_set_battery, 0, 0, "Enable/Disbale backup battery");// create command for tamper sensor disable

}
