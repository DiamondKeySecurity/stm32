/*
 * mgmt-thread.c
 * -----------
 * CLI 'thread' functions.
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

/*
 * Show the active threads. This is mostly for debugging, and looks deeply
 * into OS-level structures, but sometimes you just need to know...
 */

#include "mgmt-cli.h"
#include "mgmt-thread.h"

/* rt_TypeDef.h redefines NULL (previously defined in stddef.h, via libcli.h) */
#undef NULL

#include "rt_TypeDef.h"
#include "RTX_Conf.h"

static char *task_state[] = {
    "INACTIVE",
    "READY",
    "RUNNING",
    "WAIT_DLY",
    "WAIT_ITV",
    "WAIT_OR",
    "WAIT_AND",
    "WAIT_SEM",
    "WAIT_MBX",
    "WAIT_MUT",
};

static int cmd_thread_show(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    OS_TID task_id;
    P_TCB task;
    char *name;
    extern void main(void);
    extern void dispatch_thread(void);
    extern void osTimerThread(void);
    extern void uart_rx_thread(void);

    for (task_id = 1; task_id <= os_maxtaskrun; ++ task_id) {
        if ((task = os_active_TCB[task_id-1]) != NULL) {
            if (task->ptask == main)
                name = "main";
            else if (task->ptask == dispatch_thread)
                name = "dispatch_thread";
            else if (task->ptask == osTimerThread)
                name = "osTimerThread";
            else if (task->ptask == uart_rx_thread)
                name = "uart_rx_thread";
            else
                name = "unknown";
            
            cli_print(cli, "%d:\tptask\t%p\t%s", task_id, task->ptask, name);
            cli_print(cli, "\tstate\t%d\t\t%s", (int)task->state, task_state[task->state]);
            cli_print(cli, "\tprio\t%d", (int)task->prio);
            cli_print(cli, "\tstack\t%p", task->stack);
        }
    }
    return CLI_OK;
}

void configure_cli_thread(struct cli_def *cli)
{
    struct cli_command *c = cli_register_command(cli, NULL, "thread", NULL, 0, 0, NULL);

    /* thread show */
    cli_register_command(cli, c, "show", cmd_thread_show, 0, 0, "Show the active threads");
}
