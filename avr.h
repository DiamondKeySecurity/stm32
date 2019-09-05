#ifndef __STM32_AVR_H
#define __STM32_AVR_H

#include "stm32f4xx_hal.h"

#define AVRBOOT_STK_INSYNC          0x14  // ' '
#define AVRBOOT_STK_NOSYNC          0x15  // Not used
#define AVR_CHK_TAMPER				0x4b

extern uint8_t tamp_chk(void);

#endif
