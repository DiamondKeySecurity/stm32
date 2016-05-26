/*
 * Bootloader to either install new firmware received from the MGMT UART,
 * or jump to previously installed firmware.
 *
 */
#include "stm32f4xx_hal.h"
#include "stm-init.h"
#include "stm-led.h"
#include "stm-uart.h"

/* Magic bytes to signal the bootloader it should jump to the firmware
 * instead of trying to receive a new firmware using the MGMT UART.
 */
#define HARDWARE_EARLY_DFU_JUMP   0xBADABADA

/* symbols defined in the linker script (STM32F429BI.ld) */
extern uint32_t CRYPTECH_FIRMWARE_START;
extern uint32_t CRYPTECH_FIRMWARE_END;
extern uint32_t CRYPTECH_DFU_CONTROL;

/* Linker symbols are strange in C. Make regular pointers for sanity. */
__IO uint32_t *dfu_control = &CRYPTECH_DFU_CONTROL;
__IO uint32_t *dfu_firmware = &CRYPTECH_FIRMWARE_START;
/* The first word in the firmware is an address to the stack (msp) */
__IO uint32_t *dfu_msp_ptr = &CRYPTECH_FIRMWARE_START;
/* The second word in the firmware is a pointer to the code
 * (points at the Reset_Handler from the linker script).
 */
__IO uint32_t *dfu_code_ptr = &CRYPTECH_FIRMWARE_START + 1;

typedef  void (*pFunction)(void);

/* This is it's own function to make it more convenient to set a breakpoint at it in gdb */
void do_early_dfu_jump(void)
{
    pFunction loaded_app = (pFunction) *dfu_code_ptr;
    /* Set the stack pointer to the correct one for the firmware */
    __set_MSP(*dfu_msp_ptr);
    /* Set the Vector Table Offset Register */
    SCB->VTOR = (uint32_t) dfu_firmware;
    loaded_app();
    while (1);
}

int
main()
{
    int i;

    /* Check if we've just rebooted in order to jump to the firmware. */
    if (*dfu_control == HARDWARE_EARLY_DFU_JUMP) {
	*dfu_control = 0;
	do_early_dfu_jump();
    }

    stm_init();

    uart_send_string2(STM_UART_MGMT, (char *) "This is the bootloader speaking...");

    /* This is where uploading of new firmware over UART could happen */

    led_on(LED_BLUE);
    for (i = 0; i < 10; i++) {
	HAL_Delay(100);
	led_toggle(LED_BLUE);
    }

    /* Set dfu_control to the magic value that will cause the us to call do_early_dfu_jump
     * after rebooting back into this main() function.
     */
    *dfu_control = HARDWARE_EARLY_DFU_JUMP;

    uart_send_string2(STM_UART_MGMT, (char *) "loading firmware\r\n");

    /* De-initialize hardware by rebooting */
    HAL_NVIC_SystemReset();
    while (1) {};
}
