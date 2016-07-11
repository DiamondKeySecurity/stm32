/*
 * mgmt-cli.h
 * ---------
 * Management CLI code.
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

#ifndef __STM32_MGMT_CLI_H
#define __STM32_MGMT_CLI_H

#include "stm-init.h"
#include <libcli.h>


/* A bunch of defines to make it easier to add/maintain the CLI commands.
 *
 */
#define _cli_cmd_struct(name, fullname, func, help)		\
    static struct cli_command cmd_##fullname##_s =		\
	{(char *) #name, func, 0, help,				\
	 PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL}

/* ROOT is a top-level label with no command */
#define cli_command_root(name)					\
    _cli_cmd_struct(name, name, NULL, NULL);			\
    cli_register_command2(cli, &cmd_##name##_s, NULL)

/* BRANCH is a label with a parent, but no command */
#define cli_command_branch(parent, name)				\
    _cli_cmd_struct(name, parent##_##name, NULL, NULL);			\
    cli_register_command2(cli, &cmd_##parent##_##name##_s, &cmd_##parent##_s)

/* NODE is a label with a parent and with a command associated with it */
#define cli_command_node(parent, name, help)				\
    _cli_cmd_struct(name, parent##_##name, cmd_##parent##_##name, (char *) help); \
    cli_register_command2(cli, &cmd_##parent##_##name##_s, &cmd_##parent##_s)

/* ROOT NODE is a label without a parent, but with a command associated with it */
#define cli_command_root_node(name, help)				\
    _cli_cmd_struct(name, name, cmd_##name, (char *) help);		\
    cli_register_command2(cli, &cmd_##name##_s, NULL)

#define cli_set_cmd_privilege(name, p)				\
    cmd_##name##_s.privilege = p

#define cli_set_cmd_mode(name, m)				\
    cmd_##name##_s.mode = m


typedef enum {
    DMA_RX_STOP,
    DMA_RX_START,
} mgmt_cli_dma_state_t;

extern int control_mgmt_uart_dma_rx(mgmt_cli_dma_state_t state);

extern int cli_main(void);

#endif /* __STM32_MGMT_CLI_H */
