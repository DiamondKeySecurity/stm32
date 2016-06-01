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

#include "cmsis_os.h"

#include "stm-init.h"
#include "stm-led.h"
#include "stm-fmc.h"
#include "stm-uart.h"

/* stm32f4xx_hal_def.h and hal.h both define HAL_OK as an enum value */
#define HAL_OK HAL_OKAY

#include "hal.h"
#include "hal_internal.h"	/* hal_rpc_sendto, hal_rpc_recvfrom */
#include "xdr_internal.h"	/* hal_xdr_encode_int */

/* RPC buffers. For each active RPC, there will be two - input and output.
 */

#ifndef NUM_RPC_BUFFER
/* An arbitrary number, but we don't expect to have more than 8 concurrent
 * RPC requests.
 */
#define NUM_RPC_BUFFER 16
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

osPoolDef(rpc_buffer_pool, NUM_RPC_BUFFER, rpc_buffer_t);
osPoolId  rpc_buffer_pool;

static rpc_buffer_t *rpc_buffer_alloc(void)
{
    return (rpc_buffer_t *)osPoolCAlloc(rpc_buffer_pool);
}

/* A mutex to arbitrate concurrent UART transmits, from RPC responses.
 */
osMutexId  uart_mutex;
osMutexDef(uart_mutex);

/* Borrowed from xdr.c. We know the target architecture is little-endian,
 * but we pretend for the sake of appearances.
 */
#ifdef __ARMEL__                /* little endian */
static inline uint32_t htonl(uint32_t w)
{
  return
    ((w & 0x000000ff) << 24) +
    ((w & 0x0000ff00) << 8) +
    ((w & 0x00ff0000) >> 8) +
    ((w & 0xff000000) >> 24);
}
#else
#define htonl(x) (x)
#endif

/* Thread entry point for the RPC request handler.
 */
static void dispatch_thread(void const *args)
{
    rpc_buffer_t *ibuf = (rpc_buffer_t *)args;
    rpc_buffer_t *obuf = rpc_buffer_alloc();
    if (obuf == NULL) {
        uint32_t err = htonl(HAL_ERROR_ALLOCATION_FAILURE);
        osMutexWait(uart_mutex, osWaitForever);
        hal_rpc_sendto((uint8_t *)&err, 4, NULL);
        osMutexRelease(uart_mutex);
        osPoolFree(rpc_buffer_pool, ibuf);
        Error_Handler();
    }
    obuf->len = sizeof(obuf->buf);
    hal_rpc_server_dispatch(ibuf->buf, ibuf->len, obuf->buf, &obuf->len);
    osPoolFree(rpc_buffer_pool, ibuf);
    osMutexWait(uart_mutex, osWaitForever);
    hal_error_t ret = hal_rpc_sendto(obuf->buf, obuf->len, NULL);
    osMutexRelease(uart_mutex);
    osPoolFree(rpc_buffer_pool, obuf);
    if (ret != HAL_OK)
        Error_Handler();
}
osThreadDef(dispatch_thread, osPriorityNormal, DEFAULT_STACK_SIZE);

/* Semaphore to inform the main thread that there's a new RPC request.
 */
osSemaphoreId rpc_sem;
osSemaphoreDef(rpc_sem);

static uint8_t c;		/* current character received from UART */
static rpc_buffer_t *ibuf;	/* current RPC input buffer */

/* Add a byte to the input buffer.
 */
static inline void ibuf_push(uint8_t c) {
    if (ibuf->len < MAX_PKT_SIZE)
        ibuf->buf[ibuf->len++] = c;
}

/* Callback for HAL_UART_Receive_IT().
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
/* SLIP special characters */
#define END             0300    /* indicates end of packet */
#define ESC             0333    /* indicates byte stuffing */
#define ESC_END         0334    /* ESC ESC_END means END data byte */
#define ESC_ESC         0335    /* ESC ESC_ESC means ESC data byte */

    static int esc_flag = 0;	/* previous char was ESC */

    /* got 1 char - un-SLIP it */
    switch (c) {
    case END:
        if (ibuf->len)
            osSemaphoreRelease(rpc_sem);
        break;
    case ESC:
        esc_flag = 1;
        break;
    default:
        if (esc_flag) {
            esc_flag = 0;
            switch (c) {
            case ESC_END:
                ibuf_push(END);
                break;
            case ESC_ESC:
                ibuf_push(ESC);
                break;
            default:
                ibuf_push(c);
            }
        }
        else {
            ibuf_push(c);
        }
        break;
    }

    HAL_UART_Receive_IT(huart, &c, 1);
}

/* The main thread. After the system setup, it waits for the RPC-request
 * semaphore from HAL_UART_RxCpltCallback, and spawns a dispatch thread.
 */
int main()
{
    stm_init();

#ifdef TARGET_CRYPTECH_DEV_BRIDGE
    /* Wait six seconds to not upset the Novena at boot. */
    led_on(LED_BLUE);
    for (int i = 0; i < 12; i++) {
	osDelay(500);
	led_toggle(LED_BLUE);
    }
    led_off(LED_BLUE);
    led_on(LED_GREEN);
#endif
    /* Prepare FMC interface. */
    fmc_init();

    /* Haaaack. probe_cores() calls malloc(), which works from the main
     * thread, but not from a spawned thread. It would be better to
     * rewrite it to use static memory, but for now, just force it to
     * probe early.
     */
    hal_core_iterate(NULL);

    rpc_buffer_pool = osPoolCreate(osPool(rpc_buffer_pool));
    uart_mutex = osMutexCreate(osMutex(uart_mutex));
    rpc_sem = osSemaphoreCreate(osSemaphore(rpc_sem), 0);

#ifdef TARGET_CRYPTECH_ALPHA
    /* Launch other threads:
     * - admin thread on USART1
     * - csprng warm-up thread?
     */
#endif

    if (hal_rpc_server_init() != HAL_OK)
	Error_Handler();

    ibuf = rpc_buffer_alloc();
    if (ibuf == NULL)
        /* Something is badly wrong. */
        Error_Handler();

    /* Start the non-blocking receive */
    HAL_UART_Receive_IT(&huart_user, &c, 1);

    while (1) {
        osSemaphoreWait(rpc_sem, osWaitForever);
        if (osThreadCreate(osThread(dispatch_thread), (void *)ibuf) == NULL)
            Error_Handler();
        while ((ibuf = rpc_buffer_alloc()) == NULL);
        /* XXX There's a potential race condition, where another request
         * could write into the old ibuf, or into the null pointer if
         * we're out of ibufs.
         */
    }
}
