/**
  ******************************************************************************
  * File Name          : main.c
  * Date               : 08/07/2015 17:46:00
  * Description        : Main program body
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
#include "stm-init.h"
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm-led.h"
#endif
#ifdef HAL_SRAM_MODULE_ENABLED
#include "stm-fmc.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm-uart.h"
#endif
#ifdef HAL_I2C_MODULE_ENABLED
#include "stm-rtc.h"
#endif

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
#ifdef HAL_GPIO_MODULE_ENABLED
static void MX_GPIO_Init(void);
#endif
#ifdef HAL_UART_MODULE_ENABLED
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
#endif
#ifdef HAL_I2C_MODULE_ENABLED
static void MX_I2C2_Init(void);
#endif

void stm_init(void)
{

  /* MCU Configuration----------------------------------------------------------*/

  /* System interrupt init*/
  /* Sets the priority grouping field */
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_0);
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

  /* Initialize all configured peripherals */
#ifdef HAL_GPIO_MODULE_ENABLED
  MX_GPIO_Init();
#endif
#ifdef HAL_UART_MODULE_ENABLED
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
#endif
#ifdef HAL_I2C_MODULE_ENABLED
  MX_I2C2_Init();
#endif
}


#ifdef HAL_UART_MODULE_ENABLED
/* USART1 init function */
static void MX_USART1_UART_Init(void)
{
  huart_mgmt.Instance = USART1;
  huart_mgmt.Init.BaudRate = USART_MGMT_BAUD_RATE;
  huart_mgmt.Init.WordLength = UART_WORDLENGTH_8B;
  huart_mgmt.Init.StopBits = UART_STOPBITS_1;
  huart_mgmt.Init.Parity = UART_PARITY_NONE;
  huart_mgmt.Init.Mode = UART_MODE_TX_RX;
  huart_mgmt.Init.HwFlowCtl = UART_HWCONTROL_RTS_CTS; //UART_HWCONTROL_NONE;
  huart_mgmt.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart_mgmt) != HAL_OK) {
    /* Initialization Error */
    Error_Handler();
  }
}
/* USART2 init function */
static void MX_USART2_UART_Init(void)
{
  huart_user.Instance = USART2;
  huart_user.Init.BaudRate = USART_USER_BAUD_RATE;
  huart_user.Init.WordLength = UART_WORDLENGTH_8B;
  huart_user.Init.StopBits = UART_STOPBITS_1;
  huart_user.Init.Parity = UART_PARITY_NONE;
  huart_user.Init.Mode = UART_MODE_TX_RX;
  huart_user.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart_user.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart_user) != HAL_OK) {
    /* Initialization Error */
    Error_Handler();
  }
}
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __GPIOK_CLK_ENABLE();

  /* Configure LED GPIO pins PJ1==red, PJ2==yellow, PJ3==green, PJ4==blue */
  GPIO_InitStruct.Pin = LED_RED | LED_YELLOW | LED_GREEN | LED_BLUE;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
}
#endif

#ifdef HAL_I2C_MODULE_ENABLED
/* I2C2 init function (external RTC chip) */
void MX_I2C2_Init(void)
{
    hi2c_rtc.Instance = I2C2;
    hi2c_rtc.Init.ClockSpeed = 10000;
    hi2c_rtc.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c_rtc.Init.OwnAddress1 = 0;  /* Will operate as Master */
    hi2c_rtc.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c_rtc.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
    hi2c_rtc.Init.OwnAddress2 = 0;
    hi2c_rtc.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
    hi2c_rtc.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;

    if (HAL_I2C_Init(&hi2c_rtc) != HAL_OK) {
	Error_Handler();
    }
}
#endif

/**
 * @brief  This function is executed in case of error occurrence.
 * @param  None
 * @retval None
 */
void Error_Handler(void)
{
#ifdef HAL_GPIO_MODULE_ENABLED
  HAL_GPIO_WritePin(LED_PORT, LED_RED, GPIO_PIN_SET);
#endif
  while (1) { ; }
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */

/**
  * @}
*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
