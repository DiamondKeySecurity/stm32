/*
 * usart3_avrboot.h
 * ------------------
 * Functions and defines for accessing SPI flash with part number n25q128.
 *
 * The Alpha board has two of these SPI flash memorys, the FPGA config memory
 * and the keystore memory.
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

#ifndef __STM32_USART3_AVRBOOT_H
#define __STM32_USART3_AVRBOOT_H

#include "stm32f4xx_hal.h"

#define AVRBOOT_STK_OK              0x10
#define AVRBOOT_STK_FAILED          0x11  // Not used
#define AVRBOOT_STK_UNKNOWN         0x12  // Not used
#define AVRBOOT_STK_NODEVICE        0x13  // Not used
#define AVRBOOT_STK_INSYNC          0x14  // ' '
#define AVRBOOT_STK_NOSYNC          0x15  // Not used
#define AVRBOOT_STK_GET_PARAMETER 	0x41
#define AVRBOOT_STK_LOAD_ADDRESS 	0x55
#define AVRBOOT_STK_ENTER_PROGMODE  0x50  // 'P'
#define AVRBOOT_STK_LEAVE_PROGMODE  0x51  // 'Q'
#define AVRBOOT_STK_PROG_PAGE 		0x64
#define AVRBOOT_STK_READ_PAGE       0x74  // 't'
#define AVRBOOT_STK_SW_MAJOR 		0x81
#define AVRBOOT_STK_SW_MINOR 		0x82


#define AVRBOOT_PAGE_SIZE		0x40		// 64
#define AVRBOOT_NUM_PAGES		0x70		// 112


extern void avrboot_start_tamper(void);
extern HAL_StatusTypeDef avrboot_write_page(const uint8_t *page_buffer);


#endif /* __STM32_USART3_AVRBOOT_H */
