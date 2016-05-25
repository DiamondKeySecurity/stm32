/*
 * stm-fmc.h
 * ---------
 * Functions to set up and use the FMC bus.
 *
 * Copyright (c) 2015, NORDUnet A/S All rights reserved.
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

#ifndef __STM_FMC_H
#define __STM_FMC_H

#include "stm32f4xx_hal.h"

#define FMC_FPGA_BASE_ADDR              0x60000000
#define FMC_FPGA_ADDR_MASK              0x03FFFFFC  // there are 26 physical lines, but "only" 24 usable for now
#define FMC_FPGA_NWAIT_MAX_POLL_TICKS   10

#define FMC_GPIO_PORT_NWAIT             GPIOD
#define FMC_GPIO_PIN_NWAIT              GPIO_PIN_6

#define FMC_NWAIT_IDLE                  GPIO_PIN_SET

#define fmc_af_gpio(port, pins)			       \
    GPIO_InitStruct.Pin = pins;			       \
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;	       \
    GPIO_InitStruct.Pull = GPIO_NOPULL;		       \
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; \
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;	       \
    __HAL_RCC_##port##_CLK_ENABLE();		       \
    HAL_GPIO_Init(port, &GPIO_InitStruct)


extern HAL_StatusTypeDef fmc_init(void);
extern void fmc_init_gpio(void);

extern int fmc_write_32(uint32_t addr, uint32_t *data);
extern int fmc_read_32(uint32_t addr, uint32_t *data);

#endif /* __STM_FMC_H */
