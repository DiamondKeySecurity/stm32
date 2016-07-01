/*
 * rpc_server.c
 * ------------
 * Remote procedure call server-side private API implementation.
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
 * This is the main RPC server moddule. It creates a new thread to deal
 * with each request, to prevent a long-running request (e.g. RSA keygen)
 * from blocking independent requests from other clients. This has a
 * number of consequences. We can't do a blocking receive in the main
 * thread, because that prevents the dispatch thread from transmitting the
 * response (because they both want to lock the UART - see
 * stm32f4xx_hal_uart.c). So we have to do a non-blocking receive with a
 * callback routine. But we can't create a thread from the callback
 * routine, because it's in the context of an ISR, so we raise a semaphore
 * for the main thread to create the dispatch thread.
 */

#include <string.h>

#include "cmsis_os.h"

#include "stm-init.h"
#include "stm-led.h"
#include "stm-fmc.h"
#include "stm-uart.h"
#include "stm-sdram.h"

#include "mgmt-cli.h"

/* stm32f4xx_hal_def.h and hal.h both define HAL_OK as an enum value */
#define HAL_OK HAL_OKAY

#include "hal.h"
#include "hal_internal.h"
#include "slip_internal.h"
#include "xdr_internal.h"

/* RPC buffers. For each active RPC, there will be two - input and output.
 */

#ifndef NUM_RPC_TASK
/* An arbitrary number, but we don't expect to have more than 8 concurrent
 * RPC requests.
 */
#define NUM_RPC_TASK 8
#endif

#ifndef TASK_STACK_SIZE
/* Define an absurdly large task stack, because some pkey operation use a
 * lot of stack variables.
 */
#define TASK_STACK_SIZE 200*1024
#endif

#ifndef MAX_PKT_SIZE
/* Another arbitrary number, more or less driven by the 4096-bit RSA
 * keygen test.
 */
#define MAX_PKT_SIZE 4096
#endif

/* The thread entry point takes a single void* argument, so we bundle the
 * packet buffer and length arguments together.
 */
typedef struct {
    size_t len;
    uint8_t buf[MAX_PKT_SIZE];
} rpc_buffer_t;

/* A mutex to arbitrate concurrent UART transmits, from RPC responses.
 */
osMutexId  uart_mutex;
osMutexDef(uart_mutex);

/* A mutex so only one dispatch thread can receive requests.
 */
osMutexId  dispatch_mutex;
osMutexDef(dispatch_mutex);

/* Semaphore to inform the dispatch thread that there's a new RPC request.
 */
osSemaphoreId  rpc_sem;
osSemaphoreDef(rpc_sem);

static volatile uint8_t uart_rx;	/* current character received from UART */
static rpc_buffer_t * volatile rbuf;	/* current RPC input buffer */

/* Callback for HAL_UART_Receive_IT().
 */
void HAL_UART2_RxCpltCallback(UART_HandleTypeDef *huart)
{
    int complete;
    hal_slip_recv_char(rbuf->buf, &rbuf->len, sizeof(rbuf->buf), &complete);
    if (complete)
	osSemaphoreRelease(rpc_sem);

    HAL_UART_Receive_IT(huart, (uint8_t *)&uart_rx, 1);
}

hal_error_t hal_serial_send_char(uint8_t c)
{
    return (uart_send_char(c) == 0) ? HAL_OK : HAL_ERROR_RPC_TRANSPORT;
}

hal_error_t hal_serial_recv_char(uint8_t *cp)
{
    /* return the character from HAL_UART_Receive_IT */
    *cp = uart_rx;
    return HAL_OK;
}

/* Thread entry point for the RPC request handler.
 */
static void dispatch_thread(void const *args)
{
    rpc_buffer_t ibuf, obuf;

    while (1) {
        memset(&ibuf, 0, sizeof(ibuf));
        memset(&obuf, 0, sizeof(obuf));
        obuf.len = sizeof(obuf.buf);

        /* Wait for access to the uart */
        osMutexWait(dispatch_mutex, osWaitForever);

        /* Wait for the complete rpc request */
        rbuf = &ibuf;
        osSemaphoreWait(rpc_sem, osWaitForever);

        /* Let the next thread handle the next request */
        osMutexRelease(dispatch_mutex);
        /* Let the next thread take the mutex */
        osThreadYield();

        /* Process the request */
        hal_rpc_server_dispatch(ibuf.buf, ibuf.len, obuf.buf, &obuf.len);

        /* Send the response */
        osMutexWait(uart_mutex, osWaitForever);
        hal_error_t ret = hal_rpc_sendto(obuf.buf, obuf.len, NULL);
        osMutexRelease(uart_mutex);
        if (ret != HAL_OK)
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

    led_on(LED_GREEN);
    /* Prepare FMC interface. */
    fmc_init();
    sdram_init();

    /* Haaaack. probe_cores() calls malloc(), which works from the main
     * thread, but not from a spawned thread. It would be better to
     * rewrite it to use static memory, but for now, just force it to
     * probe early.
     */
    hal_core_iterate(NULL);

    uart_mutex = osMutexCreate(osMutex(uart_mutex));
    dispatch_mutex = osMutexCreate(osMutex(dispatch_mutex));
    rpc_sem = osSemaphoreCreate(osSemaphore(rpc_sem), 0);

    if (hal_rpc_server_init() != HAL_OK)
	Error_Handler();

    /* Create the rpc dispatch threads */
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

    /* Start the non-blocking receive */
    HAL_UART_Receive_IT(&huart_user, (uint8_t *)&uart_rx, 1);

    /* Launch other threads:
     * - csprng warm-up thread?
     */

    return cli_main();
}
