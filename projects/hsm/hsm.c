/*
 * hsm.c
 * ----------------
 * Main module for the HSM project.
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

#if NUM_RPC_TASK > 1
/* A mutex to arbitrate concurrent UART transmits, from RPC responses.
 */
osMutexId  uart_mutex;
osMutexDef(uart_mutex);

/* A mutex so only one dispatch thread can receive requests.
 */
osMutexId  dispatch_mutex;
osMutexDef(dispatch_mutex);
#endif

/* Semaphore to inform the dispatch thread that there's a new RPC request.
 */
osSemaphoreId  rpc_sem;
osSemaphoreDef(rpc_sem);

static volatile uint8_t uart_rx;	/* current character received from UART */
static rpc_buffer_t * volatile rbuf;	/* current RPC input buffer */
static volatile int reenable_recv = 1;

/* Callback for HAL_UART_Receive_IT().
 * With multiple worker threads, we can't do a blocking receive, because
 * that prevents other threads from sending RPC responses (because they
 * both want to lock the UART - see stm32f4xx_hal_uart.c). So we have to
 * do a non-blocking receive with a callback routine.
 * Even with only one worker thread, context-switching to the CLI thread
 * causes HAL_UART_Receive to miss input.
 */
void HAL_UART2_RxCpltCallback(UART_HandleTypeDef *huart)
{
    int complete;

    if (hal_slip_recv_char(rbuf->buf, &rbuf->len, sizeof(rbuf->buf), &complete) != LIBHAL_OK)
        Error_Handler();

    if (complete)
	if (osSemaphoreRelease(rpc_sem) != osOK)
            Error_Handler();

    if (HAL_UART_Receive_IT(huart, (uint8_t *)&uart_rx, 1) != CMSIS_HAL_OK)
        /* We may have collided with a transmit.
         * Signal the dispatch_thread to try again.
         */
        reenable_recv = 1;
}

hal_error_t hal_serial_recv_char(uint8_t *cp)
{
    /* return the character from HAL_UART_Receive_IT */
    *cp = uart_rx;
    return LIBHAL_OK;
}

hal_error_t hal_serial_send_char(uint8_t c)
{
    return (uart_send_char2(STM_UART_USER, c) == 0) ? LIBHAL_OK : HAL_ERROR_RPC_TRANSPORT;
}

/* Thread entry point for the RPC request handler.
 */
void dispatch_thread(void const *args)
{
    rpc_buffer_t ibuf, obuf;

    while (1) {
        memset(&ibuf, 0, sizeof(ibuf));
        memset(&obuf, 0, sizeof(obuf));
        obuf.len = sizeof(obuf.buf);

#if NUM_RPC_TASK > 1
        /* Wait for access to the uart */
        osMutexWait(dispatch_mutex, osWaitForever);
#endif

        /* Start receiving, or re-enable after failing in the callback. */
        if (reenable_recv) {
            if (HAL_UART_Receive_IT(&huart_user, (uint8_t *)&uart_rx, 1) != CMSIS_HAL_OK)
                Error_Handler();
            reenable_recv = 0;
        }

        /* Wait for the complete rpc request */
        rbuf = &ibuf;
        osSemaphoreWait(rpc_sem, osWaitForever);

#if NUM_RPC_TASK > 1
        /* Let the next thread handle the next request */
        osMutexRelease(dispatch_mutex);
        /* Let the next thread take the mutex */
        osThreadYield();
#endif

        /* Process the request */
        if (hal_rpc_server_dispatch(ibuf.buf, ibuf.len, obuf.buf, &obuf.len) != LIBHAL_OK)
            /* If hal_rpc_server_dispatch failed with an XDR error, it
             * probably means the request packet was garbage. In any case, we
             * have nothing to transmit.
             */
            continue;

        /* Send the response */
#if NUM_RPC_TASK > 1
        osMutexWait(uart_mutex, osWaitForever);
#endif
        hal_error_t ret = hal_rpc_sendto(obuf.buf, obuf.len, NULL);
#if NUM_RPC_TASK > 1
        osMutexRelease(uart_mutex);
#endif
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

#if NUM_RPC_TASK > 1
    if ((uart_mutex = osMutexCreate(osMutex(uart_mutex))) == NULL ||
        (dispatch_mutex = osMutexCreate(osMutex(dispatch_mutex)) == NULL)
	Error_Handler();
#endif
    if ((rpc_sem = osSemaphoreCreate(osSemaphore(rpc_sem), 0)) == NULL)
	Error_Handler();

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

    /* Launch other threads (csprng warm-up thread?)
     * Wait for FPGA_DONE interrupt.
     */

    return cli_main();
}
