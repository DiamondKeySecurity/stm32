/*
 * stm-init.h
 * ----------
 * Functions to set up the stm32 peripherals.
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

#ifndef __STM_INIT_H
#define __STM_INIT_H

#include "cmsis_os.h"
#include "stm32f4xx_hal.h"

/* Macros used to make GPIO pin setup (in stm-init.c) easier */
#define gpio_output(output_port, output_pins, output_level)	\
    /* Configure GPIO pin Output Level */			\
    HAL_GPIO_WritePin(output_port, output_pins, output_level);	\
    /* Configure pin as output */ 				\
    GPIO_InitStruct.Pin = output_pins; 				\
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP; 		\
    GPIO_InitStruct.Pull = GPIO_NOPULL; 			\
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW; 			\
    HAL_GPIO_Init(output_port, &GPIO_InitStruct)

#define gpio_input(input_port, input_pin, input_pull)	\
    GPIO_InitStruct.Pin = input_pin;			\
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;		\
    GPIO_InitStruct.Pull = input_pull;			\
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;		\
    HAL_GPIO_Init(input_port, &GPIO_InitStruct)


extern void stm_init(void);
extern void Error_Handler(void);

#define HAL_Delay osDelay

#endif /* __STM_INIT_H */
