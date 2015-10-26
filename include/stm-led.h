#ifndef __STM_LED_H
#define __STM_LED_H

#include "stm32f4xx_hal.h"

#define LED_PORT        GPIOJ
#define LED_RED         GPIO_PIN_1
#define LED_YELLOW      GPIO_PIN_2
#define LED_GREEN       GPIO_PIN_3
#define LED_BLUE        GPIO_PIN_4

#define led_on(pin)     HAL_GPIO_WritePin(LED_PORT,pin,SET)
#define led_off(pin)    HAL_GPIO_WritePin(LED_PORT,pin,RESET)
#define led_toggle(pin) HAL_GPIO_TogglePin(LED_PORT,pin)

#endif /* __STM_LED_H */
