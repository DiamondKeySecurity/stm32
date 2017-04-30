/*
 * hsm.c
 * ----------------
 * Main module for the HSM project.
 *
 * Copyright (c) 2016-2017, NORDUnet A/S All rights reserved.
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
 * This is the main RPC server module. At the moment, it has a single
 * worker thread to handle RPC requests, while the main thread handles CLI
 * activity. The design allows for multiple worker threads to handle
 * concurrent RPC requests from multiple clients (muxed through a daemon
 * on the host).
 */

#include <string.h>

/* Rename both CMSIS HAL_OK and libhal HAL_OK to disambiguate */
#define HAL_OK CMSIS_HAL_OK
#include "stm-init.h"
#include "stm-led.h"
#include "stm-fmc.h"
#include "stm-uart.h"
#include "stm-sdram.h"
#include "task.h"

#include "mgmt-cli.h"

#undef HAL_OK
#define HAL_OK LIBHAL_OK
#include "hal.h"
#include "hal_internal.h"
#include "slip_internal.h"
#include "xdr_internal.h"
#undef HAL_OK

#ifndef NUM_RPC_TASK
#define NUM_RPC_TASK 1
#elif NUM_RPC_TASK < 1 || NUM_RPC_TASK > 10
#error invalid NUM_RPC_TASK
#endif

#ifndef TASK_STACK_SIZE
/* Define an absurdly large task stack, because some pkey operation use a
 * lot of stack variables. This has to go in SDRAM, because it exceeds the
 * total RAM on the ARM.
 */
#define TASK_STACK_SIZE 200*1024
#endif

/* Stack for the busy task. This doesn't need to be very big.
 */
#ifndef BUSY_STACK_SIZE
#define BUSY_STACK_SIZE 1*1024
#endif
static uint8_t busy_stack[BUSY_STACK_SIZE];

/* Stack for the CLI task. This needs to be big enough to accept a
 * 4096-byte block of an FPGA or bootloader image upload.
 */
#ifndef CLI_STACK_SIZE
#define CLI_STACK_SIZE 8*1024
#endif
static uint8_t cli_stack[CLI_STACK_SIZE];

#ifndef MAX_PKT_SIZE
/* An arbitrary number, more or less driven by the 4096-bit RSA
 * keygen test.
 */
#define MAX_PKT_SIZE 4096
#endif

/* RPC buffers. For each active request, there will be two - input and output.
 */
typedef struct rpc_buffer_s {
    size_t len;
    uint8_t buf[MAX_PKT_SIZE];
    struct rpc_buffer_s *next;  /* for ibuf queue linking */
} rpc_buffer_t;

/* RPC input (requst) buffers */
static rpc_buffer_t ibufs[NUM_RPC_TASK];

/* ibuf queue structure */
typedef struct {
    rpc_buffer_t *head, *tail;
    size_t len, max;            /* for reporting */
} ibufq_t;

/* ibuf queues. These correspond roughly to task states - 'waiting' is for
 * unallocated ibufs, while 'ready' is for requests that are ready to be
 * processed.
 */
static ibufq_t ibuf_waiting, ibuf_ready;

/* Get an ibuf from a queue. */
static rpc_buffer_t *ibuf_get(ibufq_t *q)
{
    hal_critical_section_start();
    rpc_buffer_t *ibuf = q->head;
    if (ibuf) {
        q->head = ibuf->next;
        if (q->head == NULL)
            q->tail = NULL;
        ibuf->next = NULL;
        --q->len;
    }
    hal_critical_section_end();
    return ibuf;
}

/* Put an ibuf on a queue. */
static void ibuf_put(ibufq_t *q, rpc_buffer_t *ibuf)
{
    hal_critical_section_start();
    if (q->tail)
        q->tail->next = ibuf;
    else
        q->head = ibuf;
    q->tail = ibuf;
    ibuf->next = NULL;
    if (++q->len > q->max)
        q->max = q->len;
    hal_critical_section_end();
}

/* Get the current length of the 'ready' queue, for reporting in the CLI. */
size_t request_queue_len(void)
{
    size_t n;

    hal_critical_section_start();
    n = ibuf_ready.len;
    hal_critical_section_end();

    return n;
}

/* Get the maximum length of the 'ready' queue, for reporting in the CLI. */
size_t request_queue_max(void)
{
    size_t n;

    hal_critical_section_start();
    n = ibuf_ready.max;
    hal_critical_section_end();

    return n;
}

static void dispatch_task(void);
static void busy_task(void);
static tcb_t *busy_tcb;

/* Select an available dispatch task. For simplicity, this doesn't try to
 * allocate tasks in a round-robin fashion, so the lowest-numbered task
 * will see the most action. OTOH, this lets us gauge the level of system
 * activity in the CLI's 'task show' command.
 */
static tcb_t *task_next_waiting(void)
{
    for (tcb_t *t = task_iterate(NULL); t; t = task_iterate(t)) {
        if (task_get_func(t) == dispatch_task &&
            task_get_state(t) == TASK_WAITING)
            return t;
    }
    return NULL;
}

static uint8_t *sdram_malloc(size_t size);

/* Callback for HAL_UART_Receive_DMA().
 */
static void RxCallback(uint8_t c)
{
    int complete;
    static rpc_buffer_t *ibuf = NULL;

    /* If we couldn't previously get an ibuf, a task may have freed one up
     * in the meantime. Otherwise, allocate one from SDRAM. In normal
     * operation, the number of ibufs will expand to the number of remote
     * clients (which we don't know and can't predict). It would take an
     * active attempt to DOS the system to exhaust SDRAM, and there are
     * easier ways to attack the device (don't release hash or pkey handles).
     */
    if (ibuf == NULL) {
        ibuf = ibuf_get(&ibuf_waiting);
        if (ibuf == NULL) {
            ibuf = (rpc_buffer_t *)sdram_malloc(sizeof(rpc_buffer_t));
            if (ibuf == NULL)
                Error_Handler();
        }
        ibuf->len = 0;
    }

    /* Process this character into the ibuf. */
    if (hal_slip_process_char(c, ibuf->buf, &ibuf->len, sizeof(ibuf->buf), &complete) != LIBHAL_OK)
        Error_Handler();

    if (complete) {
        /* Add the ibuf to the request queue, and try to get another ibuf.
         */
        ibuf_put(&ibuf_ready, ibuf);
        ibuf = ibuf_get(&ibuf_waiting);
        if (ibuf != NULL)
            ibuf->len = 0;
        /* else all ibufs are busy, try again next time */

        /* Wake a dispatch task to deal with this request, or wake the
         * busy task to re-try scheduling a dispatch task.
         */
        tcb_t *t = task_next_waiting();
        if (t)
            task_wake(t);
        else
            task_wake(busy_tcb);
    }
}

static uint8_t uart_rx[2];      /* current character received from UART */
static uint32_t uart_rx_idx = 0;

/* UART DMA half-complete and complete callbacks. With a 2-character DMA
 * buffer, one or the other of these will fire on each incoming character.
 * Under heavy load, these will sometimes fire in the wrong order, but the
 * data are in the right order in the DMA buffer, so we have a flip-flop
 * buffer index that doesn't depend on the order of the callbacks.
 */
void HAL_UART2_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    RxCallback(uart_rx[uart_rx_idx]);
    uart_rx_idx ^= 1;
}

void HAL_UART2_RxCpltCallback(UART_HandleTypeDef *huart)
{
    RxCallback(uart_rx[uart_rx_idx]);
    uart_rx_idx ^= 1;
}

/* Send one character over the UART. This is called from
 * hal_slip_send_char().
 */
hal_error_t hal_serial_send_char(uint8_t c)
{
    return (uart_send_char2(STM_UART_USER, c) == 0) ? LIBHAL_OK : HAL_ERROR_RPC_TRANSPORT;
}

/* Task entry point for the RPC request handler.
 */
static void dispatch_task(void)
{
    rpc_buffer_t obuf_s, *obuf = &obuf_s;

    while (1) {
        /* Wait for a complete RPC request */
        task_sleep();

        rpc_buffer_t *ibuf = ibuf_get(&ibuf_ready);
        if (ibuf == NULL)
            /* probably an error, but go back to sleep */
            continue;

        memset(obuf, 0, sizeof(*obuf));
        obuf->len = sizeof(obuf->buf);

        /* Process the request */
        hal_error_t ret = hal_rpc_server_dispatch(ibuf->buf, ibuf->len, obuf->buf, &obuf->len);
        ibuf_put(&ibuf_waiting, ibuf);
        if (ret == LIBHAL_OK) {
            /* Send the response */
            if (hal_rpc_sendto(obuf->buf, obuf->len, NULL) != LIBHAL_OK)
                Error_Handler();
        }
        /* Else hal_rpc_server_dispatch failed with an XDR error, which
         * probably means the request packet was garbage. In any case, we
         * have nothing to transmit.
         */
    }
}

/* Task entry point for the task-rescheduling task.
 */
static void busy_task(void)
{
    while (1) {
        /* Wake as many tasks as we have requests.
         */
        size_t n;
        for (n = request_queue_len(); n > 0; --n) {
            tcb_t *t;
            if ((t = task_next_waiting()) != NULL)
                task_wake(t);
            else
                break;
        }
        if (n == 0)
            /* flushed the queue, our work here is done */
            task_sleep();
        else
            /* more work to do, try again after some tasks have run */
            task_yield();
    }
}

/* Allocate memory from SDRAM1. There is only malloc, no free, so we don't
 * worry about fragmentation. */
static uint8_t *sdram_malloc(size_t size)
{
    /* end of variables declared with __attribute__((section(".sdram1"))) */
    extern uint8_t _esdram1 __asm ("_esdram1");
    /* end of SDRAM1 section */
    extern uint8_t __end_sdram1 __asm ("__end_sdram1");

    static uint8_t *sdram_heap = &_esdram1;
    uint8_t *p = sdram_heap;

#define pad(n) (((n) + 3) & ~3)
    size = pad(size);

    if (p + size > &__end_sdram1)
        return NULL;

    sdram_heap += size;
    return p;
}

/* Implement static memory allocation for libhal over sdram_malloc().
 * Once again, there's only alloc, not free. */

void *hal_allocate_static_memory(const size_t size)
{
    return sdram_malloc(size);
}

/* Critical section start/end - temporarily disable interrupts.
 */
void hal_critical_section_start(void)
{
    __disable_irq();
}

void hal_critical_section_end(void)
{
    __enable_irq();
}

/* A genericized public interface to task_yield(), for calling from
 * libhal.
 */
void hal_task_yield(void)
{
    task_yield();
}

/* A mutex to arbitrate concurrent access to the keystore.
 */
task_mutex_t ks_mutex = { 0 };
void hal_ks_lock(void)   { task_mutex_lock(&ks_mutex); }
void hal_ks_unlock(void) { task_mutex_unlock(&ks_mutex); }

/* The main task. This does all the setup, and the worker tasks handle
 * the rest.
 */
int main(void)
{
    stm_init();
    uart_set_default(STM_UART_MGMT);

    led_on(LED_GREEN);
    /* Prepare FMC interface. */
    fmc_init();
    sdram_init();

    if (hal_rpc_server_init() != LIBHAL_OK)
        Error_Handler();

    /* Initialize the ibuf queues. */
    memset(&ibuf_waiting, 0, sizeof(ibuf_waiting));
    memset(&ibuf_ready, 0, sizeof(ibuf_ready));
    for (int i = 0; i < sizeof(ibufs)/sizeof(ibufs[0]); ++i)
        ibuf_put(&ibuf_waiting, &ibufs[i]);

    /* Create the rpc dispatch worker tasks. */
    static char label[NUM_RPC_TASK][sizeof("dispatch0")];
    for (int i = 0; i < NUM_RPC_TASK; ++i) {
        sprintf(label[i], "dispatch%d", i);
        void *stack = (void *)sdram_malloc(TASK_STACK_SIZE);
        if (stack == NULL)
            Error_Handler();
        if (task_add(label[i], dispatch_task, &ibufs[i], stack, TASK_STACK_SIZE) == NULL)
            Error_Handler();
    }

    /* Create the busy task. */
    busy_tcb = task_add("busy", busy_task, NULL, busy_stack, sizeof(busy_stack));
    if (busy_tcb == NULL)
        Error_Handler();

    /* Start the UART receiver. */
    if (HAL_UART_Receive_DMA(&huart_user, uart_rx, 2) != CMSIS_HAL_OK)
        Error_Handler();

    /* Launch other tasks (csprng warm-up task?)
     * Wait for FPGA_DONE interrupt.
     */

    /* Create the CLI task. */
    if (task_add("cli", (funcp_t)cli_main, NULL, cli_stack, sizeof(cli_stack)) == NULL)
        Error_Handler();

    /* Start the tasker */
    task_yield();
}
