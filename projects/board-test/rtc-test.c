/*
 * Test code for the RTC.
 *
 * Dumps the SRAM and EEPROM on startup, then enables the oscillator.
 * After that, the clock registers are read once a second - the first byte
 * is seconds (and some control bits, so dont expect 0..59).
 *
 */
#include <string.h>

#include "stm32f4xx_hal.h"
#include "stm-init.h"
#include "stm-led.h"
#include "stm-uart.h"
#include "stm-rtc.h"

#define DELAY() HAL_Delay(1000)


uint8_t buf[1024];
uint32_t i;


uint32_t device_ready(uint16_t i2c_addr)
{
    uart_send_string2(STM_UART_MGMT, "Checking readiness of 0x");
    uart_send_number2(STM_UART_MGMT, i2c_addr, 4, 16);
    uart_send_string2(STM_UART_MGMT, "...");

    if (rtc_device_ready(i2c_addr) == HAL_OK) {
	uart_send_string2(STM_UART_MGMT, "OK\r\n");
	return 1;
    }

    uart_send_string2(STM_UART_MGMT, "Not ready (0x");
    uart_send_number2(STM_UART_MGMT, i, 4, 16);
    uart_send_string2(STM_UART_MGMT, ")\r\n");

    return 0;
}


void send_byte(const uint16_t i2c_addr, const uint8_t value)
{
    uint8_t ch = value;

    uart_send_string2(STM_UART_MGMT, "Sending ");
    uart_send_number2(STM_UART_MGMT, ch, 2, 16);
    uart_send_string2(STM_UART_MGMT, " to 0x");
    uart_send_number2(STM_UART_MGMT, i2c_addr, 4, 16);
    uart_send_string2(STM_UART_MGMT, "...");

    if (rtc_send_byte(i2c_addr, ch, 1000) != HAL_OK) {
	uart_send_string2(STM_UART_MGMT, "Timeout\r\n");
	Error_Handler();
    }

    uart_send_string2(STM_UART_MGMT, "OK\r\n");
}

void read_bytes (uint8_t *buf, const uint16_t i2c_addr, const uint8_t len)
{
    uart_send_string2(STM_UART_MGMT, "Reading ");
    uart_send_number2(STM_UART_MGMT, len, 3, 10);
    uart_send_string2(STM_UART_MGMT, " bytes from 0x");
    uart_send_number2(STM_UART_MGMT, i2c_addr, 4, 16);
    uart_send_string2(STM_UART_MGMT, "...");

    if (rtc_read_bytes(i2c_addr, buf, len, 1000) != HAL_OK) {
	uart_send_string2(STM_UART_MGMT, "Timeout\r\n");
	Error_Handler();
    }

    uart_send_string2(STM_UART_MGMT, "OK\r\n");
}

void request_data(uint8_t *buf, const uint16_t i2c_addr, const uint8_t offset, const uint8_t bytes)
{
    send_byte(i2c_addr, offset);
    read_bytes(buf, i2c_addr, bytes);
}

void print_time()
{
    request_data(buf, RTC_RTC_ADDR, RTC_TIME_OFFSET, RTC_TIME_BYTES);

    for (i = 0; i < RTC_TIME_BYTES; i++) {
	uart_send_number2(STM_UART_MGMT, buf[i], 2, 16);
	uart_send_string2(STM_UART_MGMT, " ");
    }
}

void dump_sram()
{
    request_data(buf, RTC_RTC_ADDR, 0x0, RTC_SRAM_TOTAL_BYTES);

    uart_send_string2(STM_UART_MGMT, "SRAM contents:\r\n");
    uart_send_hexdump(STM_UART_MGMT, buf, 0, RTC_SRAM_TOTAL_BYTES);

    uart_send_string2(STM_UART_MGMT, "\r\n");
}

void dump_eeprom()
{
    request_data(buf, RTC_EEPROM_ADDR, 0x0, RTC_EEPROM_TOTAL_BYTES);

    uart_send_string2(STM_UART_MGMT, "EEPROM contents:\r\n");
    uart_send_hexdump(STM_UART_MGMT, buf, 0, RTC_EEPROM_TOTAL_BYTES);
    uart_send_string2(STM_UART_MGMT, "\r\n");

    request_data(buf, RTC_EEPROM_ADDR, RTC_EEPROM_EUI48_OFFSET, RTC_EEPROM_EUI48_BYTES);
    uart_send_string2(STM_UART_MGMT, "EEPROM EUI-48:\r\n");
    uart_send_hexdump(STM_UART_MGMT, buf, RTC_EEPROM_EUI48_OFFSET, RTC_EEPROM_EUI48_BYTES);

    uart_send_string2(STM_UART_MGMT, "\r\n");
}

void enable_oscillator()
{
    uart_send_string2(STM_UART_MGMT, "Enabling oscillator...\r\n");

    if (rtc_enable_oscillator() != HAL_OK) {
	uart_send_string2(STM_UART_MGMT, "Timeout\r\n");
	Error_Handler();
    }

    uart_send_string2(STM_UART_MGMT, "OK\r\n");
}


int
main()
{
    stm_init();
    uart_send_string2(STM_UART_MGMT, "\r\n\r\n*** Init done\r\n");

    dump_sram();
    dump_eeprom();

    enable_oscillator();

    while (1)  {
	memset(buf, 0, sizeof(buf));

	if (! device_ready(RTC_RTC_ADDR)) {
	    goto fail;
	}

	print_time(buf);

	uart_send_string2(STM_UART_MGMT, "\r\n\r\n");

	HAL_GPIO_TogglePin(LED_PORT, LED_GREEN);
	DELAY();
	continue;

    fail:
	HAL_GPIO_TogglePin(LED_PORT, LED_RED);
	DELAY();
    }
}
