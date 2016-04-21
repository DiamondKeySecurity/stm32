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

#include "cmsis_os.h"

#include "stm-init.h"
#include "stm-led.h"
#include "stm-fmc.h"
#include "stm-uart.h"

/* stm32f4xx_hal_def.h and hal.h both define HAL_OK as an enum value */
#define HAL_OK HAL_OKAY

#include "hal.h"

/* declared in hal_internal.h */
extern hal_error_t hal_rpc_sendto(const uint8_t * const buf, const size_t len, void *opaque);
extern hal_error_t hal_rpc_recvfrom(uint8_t * const buf, size_t * const len, void **opaque);

#ifndef MAX_PKT_SIZE
#define MAX_PKT_SIZE 4096
#endif

typedef struct {
    void *opaque;
    size_t len;
    uint8_t buf[MAX_PKT_SIZE];
} rpc_buffer_t;

osPoolDef(rpc_buffer_pool, 16, rpc_buffer_t);
osPoolId  rpc_buffer_pool;

rpc_buffer_t *rpc_buffer_alloc(void)
{
    rpc_buffer_t *rbuf = (rpc_buffer_t *)osPoolCAlloc(rpc_buffer_pool);
    if (rbuf)
	rbuf->len = sizeof(rbuf->buf);
    return rbuf;
}

osMutexId  uart_mutex;
osMutexDef(uart_mutex);

void dispatch_thread(void const *args)
{
    rpc_buffer_t *ibuf = (rpc_buffer_t *)args;
    rpc_buffer_t *obuf = rpc_buffer_alloc();	// NULL check
    obuf->opaque = ibuf->opaque;
    hal_rpc_server_dispatch(ibuf->buf, ibuf->len, obuf->buf, &obuf->len);
    osPoolFree(rpc_buffer_pool, ibuf);
    osMutexWait(uart_mutex, osWaitForever);
    hal_rpc_sendto(obuf->buf, obuf->len, obuf->opaque);
    osMutexRelease(uart_mutex);
    osPoolFree(rpc_buffer_pool, obuf);
}
osThreadDef(dispatch_thread, osPriorityNormal, DEFAULT_STACK_SIZE);

void rpc_server_main(void)
{
    hal_error_t ret;
    
    while (1) {
	rpc_buffer_t *ibuf = rpc_buffer_alloc();	// NULL check
	// separate allocations for struct and block of memory?
	ret = hal_rpc_recvfrom(ibuf->buf, &ibuf->len, &ibuf->opaque);
        if (ret == HAL_OK) {
	    osThreadCreate(osThread(dispatch_thread), (void *)ibuf);
        }
    }
}

int main()
{
    stm_init();

#ifdef TARGET_CRYPTECH_DEV_BRIDGE
    // Blink blue LED for six seconds to not upset the Novena at boot.
    led_on(LED_BLUE);
    for (int i = 0; i < 12; i++) {
	osDelay(500);
	led_toggle(LED_BLUE);
    }
#endif
    // prepare fmc interface
    fmc_init();

    rpc_buffer_pool = osPoolCreate(osPool(rpc_buffer_pool));
    uart_mutex = osMutexCreate(osMutex(uart_mutex));

#ifdef TARGET_CRYPTECH_ALPHA
    // Launch other threads:
    // - admin thread on USART1
    // - csprng warm-up thread?
#endif

    if (hal_rpc_server_init() != HAL_OK)
	return 1;
    rpc_server_main();
}
