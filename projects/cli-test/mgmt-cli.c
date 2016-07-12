/*
 * mgmt-cli.c
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

#include <string.h>

/* Rename both CMSIS HAL_OK and libhal HAL_OK to disambiguate */
#define HAL_OK CMSIS_HAL_OK
#include "cmsis_os.h"

#include "stm-init.h"
#include "stm-uart.h"
#include "stm-led.h"

#include "mgmt-cli.h"
#include "mgmt-fpga.h"
#include "mgmt-misc.h"
#include "mgmt-show.h"
#include "mgmt-test.h"
#include "mgmt-keystore.h"
#include "mgmt-masterkey.h"

#undef HAL_OK
#define HAL_OK LIBHAL_OK
#include "hal.h"
#undef HAL_OK

#ifndef CLI_UART_RECVBUF_SIZE
#define CLI_UART_RECVBUF_SIZE  256
#endif

typedef struct {
    int ridx;
    volatile int widx;
    mgmt_cli_dma_state_t rx_state;
    uint8_t buf[CLI_UART_RECVBUF_SIZE];
} ringbuf_t;

inline void ringbuf_init(ringbuf_t *rb)
{
    memset(rb, 0, sizeof(*rb));
}

/* return number of characters read */
inline int ringbuf_read_char(ringbuf_t *rb, uint8_t *c)
{
    if (rb->ridx != rb->widx) {
        *c = rb->buf[rb->ridx];
        if (++rb->ridx >= sizeof(rb->buf))
            rb->ridx = 0;
        return 1;
    }
    return 0;
}

inline void ringbuf_write_char(ringbuf_t *rb, uint8_t c)
{
    rb->buf[rb->widx] = c;
    if (++rb->widx >= sizeof(rb->buf))
        rb->widx = 0;
}

static ringbuf_t uart_ringbuf;

/* current character received from UART */
static uint8_t uart_rx;

/* Semaphore to inform uart_cli_read that there's a new character.
 */
osSemaphoreId  uart_sem;
osSemaphoreDef(uart_sem);

/* Callback for HAL_UART_Receive_DMA().
 */
void HAL_UART1_RxCpltCallback(UART_HandleTypeDef *huart)
{
    ringbuf_write_char(&uart_ringbuf, uart_rx);
    osSemaphoreRelease(uart_sem);
    HAL_UART_Receive_DMA(huart, &uart_rx, 1);
}

static void uart_cli_print(struct cli_def *cli __attribute__ ((unused)), const char *buf)
{
    char crlf[] = "\r\n";
    uart_send_string2(STM_UART_MGMT, buf);
    uart_send_string2(STM_UART_MGMT, crlf);
}

static int uart_cli_read(struct cli_def *cli __attribute__ ((unused)), void *buf, size_t count)
{
    for (int i = 0; i < count; ++i) {
        while (ringbuf_read_char(&uart_ringbuf, (uint8_t *)(buf + i)) == 0)
            osSemaphoreWait(uart_sem, osWaitForever);
    }
    return count;
}

static int uart_cli_write(struct cli_def *cli __attribute__ ((unused)), const void *buf, size_t count)
{
    uart_send_bytes(STM_UART_MGMT, (uint8_t *) buf, count);
    return (int) count;
}

int control_mgmt_uart_dma_rx(mgmt_cli_dma_state_t state)
{
    if (state == DMA_RX_START) {
	if (uart_ringbuf.rx_state != DMA_RX_START) {
            ringbuf_init(&uart_ringbuf);
	    HAL_UART_Receive_DMA(&huart_mgmt, &uart_rx, 1);
	    uart_ringbuf.rx_state = DMA_RX_START;
	}
	return 1;
    } else if (state == DMA_RX_STOP) {
	if (HAL_UART_DMAStop(&huart_mgmt) != CMSIS_HAL_OK) return 0;
	uart_ringbuf.rx_state = DMA_RX_STOP;
	return 1;
    }
    return 0;
}

static int embedded_cli_loop(struct cli_def *cli)
{
    unsigned char c;
    int n = 0;
    static struct cli_loop_ctx ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.insertmode = 1;

    cli->state = CLI_STATE_LOGIN;

    /* start off in unprivileged mode */
    cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
    cli_set_configmode(cli, MODE_EXEC, NULL);

    cli_error(cli, "%s", cli->banner);

    while (1) {
	cli_loop_start_new_command(cli, &ctx);

	control_mgmt_uart_dma_rx(DMA_RX_START);

	while (1) {
	    cli_loop_show_prompt(cli, &ctx);

	    n = cli_loop_read_next_char(cli, &ctx, &c);

	    /*
	    cli_print(cli, "Next char: '%c'/%i, ringbuf ridx %i, widx %i",
		      c, (int) c,
		      uart_ringbuf.ridx,
		      RINGBUF_WIDX(uart_ringbuf)
	    */
	    if (n == CLI_LOOP_CTRL_BREAK)
		break;
	    if (n == CLI_LOOP_CTRL_CONTINUE)
		continue;

	    n = cli_loop_process_char(cli, &ctx, c);
	    if (n == CLI_LOOP_CTRL_BREAK)
		break;
	    if (n == CLI_LOOP_CTRL_CONTINUE)
		continue;
	}

	if (ctx.l < 0)
            continue;

	/* cli_print(cli, "Process command: '%s'", ctx.cmd); */
	n = cli_loop_process_cmd(cli, &ctx);
	if (n == CLI_LOOP_CTRL_BREAK)
	    break;
    }

    return CLI_OK;
}

static void mgmt_cli_init(struct cli_def *cli)
{
    cli_init(cli);
    cli_read_callback(cli, uart_cli_read);
    cli_write_callback(cli, uart_cli_write);
    cli_print_callback(cli, uart_cli_print);
    cli_set_banner(cli, "Cryptech Alpha test CLI");
    cli_set_hostname(cli, "cryptech");
    cli_telnet_protocol(cli, 0);
}

hal_user_t user;

static int check_auth(const char *username, const char *password)
{
    if (strcasecmp(username, "ct") != 0)
	return CLI_ERROR;
    if (strcasecmp(password, "ct") != 0)
	return CLI_ERROR;
    return CLI_OK;
}

int cli_main(void)
{
    static struct cli_def cli;

    uart_sem = osSemaphoreCreate(osSemaphore(uart_sem), 0);

    mgmt_cli_init(&cli);
    cli_set_auth_callback(&cli, check_auth);

    configure_cli_show(&cli);
    configure_cli_fpga(&cli);
    configure_cli_misc(&cli);
    configure_cli_test(&cli);
    configure_cli_keystore(&cli);
    configure_cli_masterkey(&cli);

    while (1) {
        embedded_cli_loop(&cli);
        /* embedded_cli_loop returns when the user enters 'quit' or 'exit' */
        cli_print(&cli, "\nLogging out...\n");
        user = HAL_USER_NONE;
    }

    /*NOTREACHED*/
    return -1;
}

