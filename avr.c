#include "stm-uart.h"
#include "avr.h"


uint8_t tamp_chk(void){
	uint8_t resp = 0;
	uart_send_char_tamper(&huart_tmpr, AVR_CHK_TAMPER);
	HAL_Delay(5);
	HAL_UART_Receive(&huart_tmpr, &resp, 1, 1000);
	HAL_Delay(200);

	// the arguments have been parsed and validated
	if (resp == AVRBOOT_STK_INSYNC) {
		return AVRBOOT_STK_INSYNC;
	}
	else if (resp == AVRBOOT_STK_NOSYNC){
		return AVRBOOT_STK_NOSYNC;
	}
	else {
		return resp;
	}

}
