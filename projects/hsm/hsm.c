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
#include "cmsis_os.h"

#include "stm-init.h"
#include "stm-led.h"
#include "stm-fmc.h"
#include "stm-uart.h"
#include "stm-sdram.h"

#include "mgmt-cli.h"

#undef HAL_OK
#define HAL_OK LIBHAL_OK
#include "hal.h"
#include "hal_internal.h"
#include "slip_internal.h"
#include "xdr_internal.h"
#undef HAL_OK

#ifndef NUM_RPC_TASK
/* Just one RPC task for now. More will require active resource management
 * of at least the FPGA cores.
 */
#define NUM_RPC_TASK 1
#endif

#ifndef TASK_STACK_SIZE
/* Define an absurdly large task stack, because some pkey operation use a
 * lot of stack variables. This has to go in SDRAM, because it exceeds the
 * total RAM on the ARM.
 */
#define TASK_STACK_SIZE 200*1024
#endif

#ifndef MAX_PKT_SIZE
/* An arbitrary number, more or less driven by the 4096-bit RSA
 * keygen test.
 */
#define MAX_PKT_SIZE 4096
#endif

/* RPC buffers. For each active RPC, there will be two - input and output.
 */
typedef struct {
    size_t len;
    uint8_t buf[MAX_PKT_SIZE];
} rpc_buffer_t;

/* A mail queue (memory pool + message queue) for RPC request messages.
 */
osMailQId  ibuf_queue;
osMailQDef(ibuf_queue, NUM_RPC_TASK + 2, rpc_buffer_t);

#if NUM_RPC_TASK > 1
/* A mutex to arbitrate concurrent UART transmits, from RPC responses.
 */
osMutexId  uart_mutex;
osMutexDef(uart_mutex);
static inline void uart_lock(void)   { osMutexWait(uart_mutex, osWaitForever); }
static inline void uart_unlock(void) { osMutexRelease(uart_mutex); }
#else
static inline void uart_lock(void)   { }
static inline void uart_unlock(void) { }
#endif

#if NUM_RPC_TASK > 1
/* A mutex to arbitrate concurrent access to the keystore.
 */
osMutexId  ks_mutex;
osMutexDef(ks_mutex);
void hal_ks_lock(void)   { osMutexWait(ks_mutex, osWaitForever); }
void hal_ks_unlock(void) { osMutexRelease(ks_mutex); }
#endif

/* Sleep for specified number of seconds.
 */
void hal_sleep(const unsigned seconds) { osDelay(seconds * 1000); }

/* A ring buffer for the UART DMA receiver. In theory, it should get at most
 * 92 characters per 1ms tick, but we're going to up-size it for safety.
 */
#ifndef RPC_UART_RECVBUF_SIZE
#define RPC_UART_RECVBUF_SIZE  256  /* must be a power of 2 */
#endif
#define RPC_UART_RECVBUF_MASK  (RPC_UART_RECVBUF_SIZE - 1)

typedef struct {
    uint32_t ridx;
    uint8_t buf[RPC_UART_RECVBUF_SIZE];
} uart_ringbuf_t;

volatile uart_ringbuf_t uart_ringbuf = {0, {0}};

#define RINGBUF_RIDX(rb)       (rb.ridx & RPC_UART_RECVBUF_MASK)
#define RINGBUF_WIDX(rb)       (sizeof(rb.buf) - __HAL_DMA_GET_COUNTER(huart_user.hdmarx))
#define RINGBUF_COUNT(rb)      ((unsigned)(RINGBUF_WIDX(rb) - RINGBUF_RIDX(rb)))
#define RINGBUF_READ(rb, dst)  {dst = rb.buf[RINGBUF_RIDX(rb)]; rb.ridx++;}

/* Thread entry point for the UART DMA monitor.
 */
void uart_rx_thread(void const *args)
{
    /* current RPC input buffer */
    rpc_buffer_t *ibuf = NULL;

    /* I wanted to call osThreadYield(), but the documentation is misleading,
     * and it only yields to the next ready thread of the same priority, so
     * this high-priority thread wouldn't let anything else run. osDelay(1)
     * reschedules this thread for the next tick, which is what we want.
     */
    for ( ; ; osDelay(1)) {
        if (ibuf == NULL) {
            if ((ibuf = (rpc_buffer_t *)osMailAlloc(ibuf_queue, 1)) == NULL)
                /* This could happen if all dispatch threads are busy, and
                 * there are NUM_RPC_TASK requests already queued. We could
                 * send a "server busy" error, or we could just try again on
                 * the next tick.
                 */
                Error_Handler();
            ibuf->len = 0;
        }

        while (RINGBUF_COUNT(uart_ringbuf)) {
            uint8_t c;
            int complete;

            RINGBUF_READ(uart_ringbuf, c);
            if (hal_slip_process_char(c, ibuf->buf, &ibuf->len, sizeof(ibuf->buf), &complete) != LIBHAL_OK)
                Error_Handler();

            if (complete) {
                if (osMailPut(ibuf_queue, (void *)ibuf) != osOK)
                    Error_Handler();
                ibuf = NULL;
                /* Yield, to allow one of the dispatch threads to pick up this
                 * new request.
                 */
                break;
            }
        }
    }
}
osThreadDef(uart_rx_thread, osPriorityHigh, DEFAULT_STACK_SIZE);

hal_error_t hal_serial_send_char(uint8_t c)
{
    return (uart_send_char2(STM_UART_USER, c) == 0) ? LIBHAL_OK : HAL_ERROR_RPC_TRANSPORT;
}

/* Thread entry point for the RPC request handler.
 */
void dispatch_thread(void const *args)
{
    rpc_buffer_t obuf_s, *obuf = &obuf_s, *ibuf;

    while (1) {
        memset(obuf, 0, sizeof(*obuf));
        obuf->len = sizeof(obuf->buf);

        /* Wait for a complete RPC request */
        osEvent evt = osMailGet(ibuf_queue, osWaitForever);
        if (evt.status != osEventMail)
            continue;
        ibuf = (rpc_buffer_t *)evt.value.p;

        /* Process the request */
	hal_error_t ret = hal_rpc_server_dispatch(ibuf->buf, ibuf->len, obuf->buf, &obuf->len);
        osMailFree(ibuf_queue, (void *)ibuf);
        if (ret != LIBHAL_OK) {
            /* If hal_rpc_server_dispatch failed with an XDR error, it
             * probably means the request packet was garbage. In any case, we
             * have nothing to transmit.
             */
            continue;
	}

        /* Send the response */
        uart_lock();
        ret = hal_rpc_sendto(obuf->buf, obuf->len, NULL);
        uart_unlock();
        if (ret != LIBHAL_OK)
            Error_Handler();
    }
}
osThreadDef_t thread_def[NUM_RPC_TASK];

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

#if NUM_RPC_TASK > 1
/* Critical section start/end, currently used just for hal_core_alloc/_free.
 */
void hal_critical_section_start(void)
{
    __disable_irq();
}

void hal_critical_section_end(void)
{
    __enable_irq();
}
#endif

/* The main thread. This does all the setup, and the worker threads handle
 * the rest.
 */
int main()
{
    stm_init();
    uart_set_default(STM_UART_MGMT);

    led_on(LED_GREEN);
    /* Prepare FMC interface. */
    fmc_init();
    sdram_init();

    if ((ibuf_queue = osMailCreate(osMailQ(ibuf_queue), NULL)) == NULL)
        Error_Handler();

#if NUM_RPC_TASK > 1
    if ((uart_mutex = osMutexCreate(osMutex(uart_mutex))) == NULL)
	Error_Handler();
    if ((ks_mutex = osMutexCreate(osMutex(ks_mutex))) == NULL)
	Error_Handler();
#endif

    if (hal_rpc_server_init() != LIBHAL_OK)
	Error_Handler();

    /* Create the rpc dispatch worker threads. */
    for (int i = 0; i < NUM_RPC_TASK; ++i) {
        osThreadDef_t *ot = &thread_def[i];
        ot->pthread = dispatch_thread;
        ot->tpriority = osPriorityNormal;
        ot->stacksize = TASK_STACK_SIZE;
        ot->stack_pointer = (uint32_t *)(sdram_malloc(TASK_STACK_SIZE));
        if (ot->stack_pointer == NULL)
            Error_Handler();
        if (osThreadCreate(ot, (void *)i) == NULL)
            Error_Handler();
    }

    /* Start the UART receiver. */
    if (HAL_UART_Receive_DMA(&huart_user, (uint8_t *) uart_ringbuf.buf, sizeof(uart_ringbuf.buf)) != CMSIS_HAL_OK)
        Error_Handler();
    if (osThreadCreate(osThread(uart_rx_thread), NULL) == NULL)
        Error_Handler();

    /* Launch other threads (csprng warm-up thread?)
     * Wait for FPGA_DONE interrupt.
     */

    return cli_main();
}
