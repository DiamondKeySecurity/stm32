/**
******************************************************************************
* @file    stm32f4xx_it.c
* @brief   Interrupt Service Routines.
******************************************************************************
*
* COPYRIGHT(c) 2015 STMicroelectronics
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*   1. Redistributions of source code must retain the above copyright notice,
*      this list of conditions and the following disclaimer.
*   2. Redistributions in binary form must reproduce the above copyright notice,
*      this list of conditions and the following disclaimer in the documentation
*      and/or other materials provided with the distribution.
*   3. Neither the name of STMicroelectronics nor the names of its contributors
*      may be used to endorse or promote products derived from this software
*      without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
******************************************************************************
*/
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "stm32f4xx.h"
#include "stm32f4xx_it.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M4 Processor Interruption and Exception Handlers         */
/******************************************************************************/

/**
 * @brief This function handles System tick timer.
 */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f4xx.s).                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */

void hard_fault_handler_c (unsigned int * hardfault_args)
{
    volatile unsigned int stacked_r0;
    volatile unsigned int stacked_r1;
    volatile unsigned int stacked_r2;
    volatile unsigned int stacked_r3;
    volatile unsigned int stacked_r12;
    volatile unsigned int stacked_lr;
    volatile unsigned int stacked_pc;
    volatile unsigned int stacked_psr;

    stacked_r0 = hardfault_args[0];
    stacked_r1 = hardfault_args[1];
    stacked_r2 = hardfault_args[2];
    stacked_r3 = hardfault_args[3];

    stacked_r12 = hardfault_args[4];
    stacked_lr = hardfault_args[5];
    stacked_pc = hardfault_args[6];
    stacked_psr = hardfault_args[7];

    printf ("\n[Hard fault handler]\n");
    printf ("R0       = %08x\n", stacked_r0);
    printf ("R1       = %08x\n", stacked_r1);
    printf ("R2       = %08x\n", stacked_r2);
    printf ("R3       = %08x\n", stacked_r3);
    printf ("R12      = %08x\n", stacked_r12);
    printf ("LR [R14] = %08x  subroutine call return address\n", stacked_lr);
    printf ("PC [R15] = %08x  program counter\n", stacked_pc);
    printf ("PSR      = %08x\n", stacked_psr);
    printf ("BFAR     = %08x\n", (*((volatile unsigned int *)(0xE000ED38))));
    printf ("CFSR     = %08x\n", (*((volatile unsigned int *)(0xE000ED28))));
    printf ("HFSR     = %08x\n", (*((volatile unsigned int *)(0xE000ED2C))));
    printf ("DFSR     = %08x\n", (*((volatile unsigned int *)(0xE000ED30))));
    printf ("AFSR     = %08x\n", (*((volatile unsigned int *)(0xE000ED3C))));

    while (1);
}

void HardFault_Handler(void) __attribute__( ( naked ) );
void HardFault_Handler(void)
{
    __asm volatile (
	" tst lr, #4                                                \n"
	" ite eq                                                    \n"
	" mrseq r0, msp                                             \n"
	" mrsne r0, psp                                             \n"
	" b hard_fault_handler_c                                    \n"
	" ldr r1, [r0, #24]                                         \n"
	" ldr r2, handler2_address_const                            \n"
	" bx r2                                                     \n"
	" handler2_address_const: .word hard_fault_handler_c        \n"
	);
}

/* USER CODE END 1 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
