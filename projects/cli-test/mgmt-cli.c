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
#include "stm-init.h"
#include "stm-uart.h"
#include "stm-led.h"
#include "mgmt-cli.h"

#include <string.h>

extern uint8_t uart_rx;

struct uart_ringbuf_t {
    uint32_t enabled, ridx, widx, overflow;
    uint8_t buf[CLI_UART_RECVBUF_SIZE];
};

volatile struct uart_ringbuf_t uart_ringbuf = {1, 0, 0, 0, {0}};

#define RINGBUF_RIDX(rb)       (rb.ridx & CLI_UART_RECVBUF_MASK)
#define RINGBUF_WIDX(rb)       (rb.widx & CLI_UART_RECVBUF_MASK)
#define RINGBUF_COUNT(rb)      ((unsigned)(rb.widx - rb.ridx))
#define RINGBUF_FULL(rb)       (RINGBUF_RIDX(rb) == ((rb.widx + 1) & CLI_UART_RECVBUF_MASK))
#define RINGBUF_READ(rb, dst)  {dst = rb.buf[RINGBUF_RIDX(rb)]; rb.buf[RINGBUF_RIDX(rb)] = '.'; rb.ridx++;}
#define RINGBUF_WRITE(rb, src) {rb.buf[RINGBUF_WIDX(rb)] = src; rb.widx++;}

void uart_cli_print(struct cli_def *cli __attribute__ ((unused)), const char *buf)
{
    char crlf[] = "\r\n";
    uart_send_string2(STM_UART_MGMT, buf);
    uart_send_string2(STM_UART_MGMT, crlf);
}

int uart_cli_read(struct cli_def *cli __attribute__ ((unused)), void *buf, size_t count)
{
    uint32_t timeout = 0xffffff;

    /* Always explicitly enable the RX interrupt when we get here.
     * Prevents us getting stuck waiting for an interrupt that will never come.
     */
    __HAL_UART_FLUSH_DRREGISTER(&huart_mgmt);
    HAL_UART_Receive_IT(&huart_mgmt, (uint8_t *) &uart_rx, 1);

    while (count && timeout--) {
	if (RINGBUF_COUNT(uart_ringbuf)) {
	    RINGBUF_READ(uart_ringbuf, *(uint8_t *) buf);
	    buf++;
	    count--;
	} else {
	    led_toggle(LED_GREEN);
	    HAL_Delay(10);
	}
    }
    return 1;
}

int uart_cli_write(struct cli_def *cli __attribute__ ((unused)), const void *buf, size_t count)
{
    uart_send_bytes(STM_UART_MGMT, (uint8_t *) buf, count);
    return (int) count;
}

int embedded_cli_loop(struct cli_def *cli)
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

	while (1) {
	    cli_loop_show_prompt(cli, &ctx);

	    n = cli_loop_read_next_char(cli, &ctx, &c);

	    /*
	    cli_print(cli, "Next char: '%c'/%i, ringbuf ridx %i, widx %i (%i/%i) - count %i",
		      c, (int) c, uart_ringbuf.ridx, uart_ringbuf.widx, RINGBUF_RIDX(uart_ringbuf),
		      RINGBUF_WIDX(uart_ringbuf), RINGBUF_COUNT(uart_ringbuf));
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

	if (ctx.l < 0) break;

	/* cli_print(cli, "Process command: '%s'", ctx.cmd); */
	n = cli_loop_process_cmd(cli, &ctx);
	if (n == CLI_LOOP_CTRL_BREAK)
	    break;
    }

    return CLI_OK;
}

/* Interrupt service routine to be called when data has been received on the MGMT UART. */
void mgmt_cli_uart_isr(const uint8_t *buf, size_t count)
{
    if (! uart_ringbuf.enabled) return;

    while (count) {
	if (RINGBUF_FULL(uart_ringbuf)) {
	    uart_ringbuf.overflow++;
	    return;
	}
	RINGBUF_WRITE(uart_ringbuf, *buf);
	buf++;
	count--;
    }
}

void mgmt_cli_init(struct cli_def *cli)
{
    cli_init(cli);
    cli_read_callback(cli, uart_cli_read);
    cli_write_callback(cli, uart_cli_write);
    cli_print_callback(cli, uart_cli_print);
    cli_set_banner(cli, "Cryptech Alpha");
    cli_set_hostname(cli, "cryptech");
    cli_telnet_protocol(cli, 0);
}
