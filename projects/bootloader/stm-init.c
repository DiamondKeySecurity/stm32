/* Disable modules that the bootloader doesn't need. */

#include "stm32f4xx_hal.h"

#undef HAL_I2C_MODULE_ENABLED
#undef HAL_SPI_MODULE_ENABLED

#include "../../stm-init.c"
