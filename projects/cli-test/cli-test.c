/*
 * Test code with a small CLI on the management interface
 *
 */
#include "stm32f4xx_hal.h"
#include "stm-init.h"
#include "stm-led.h"
#include "stm-uart.h"

#include <string.h>
#include <libcli.h>

#define DELAY() HAL_Delay(250)

int cmd_show_cpuspeed(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    volatile uint32_t hclk;

    hclk = HAL_RCC_GetHCLKFreq();
    cli_print(cli, "HCLK: %li (%i MHz)", hclk, (int) hclk / 1000 / 1000);
    return CLI_OK;
}

void uart_cli_print(struct cli_def *cli __attribute__ ((unused)), const char *buf)
{
    char crlf[] = "\r\n";
    uart_send_string2(STM_UART_MGMT, buf);
    uart_send_string2(STM_UART_MGMT, crlf);
}

int uart_cli_read(struct cli_def *cli __attribute__ ((unused)), void *buf, size_t count)
{
    if (uart_recv_char2(STM_UART_MGMT, buf, count) != HAL_OK) {
	return -1;
    }
    return 1;
}

int uart_cli_write(struct cli_def *cli __attribute__ ((unused)), const void *buf, size_t count)
{
    uart_send_bytes(STM_UART_MGMT, (uint8_t *) buf, count);
    return (int) count;
}

int embedded_cli_loop(struct cli_def *cli)
{
    unsigned char c;
    int n = 0;
    static struct cli_loop_ctx ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.insertmode = 1;

    cli->state = CLI_STATE_LOGIN;

    /* start off in unprivileged mode */
    cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
    cli_set_configmode(cli, MODE_EXEC, NULL);

    cli_error(cli, "%s", cli->banner);

    while (1) {
	cli_loop_start_new_command(cli, &ctx);
	HAL_GPIO_TogglePin(LED_PORT, LED_YELLOW);

	while (1) {
	    HAL_GPIO_TogglePin(LED_PORT, LED_BLUE);

	    cli_loop_show_prompt(cli, &ctx);

	    n = cli_loop_read_next_char(cli, &ctx, &c);
	    //cli_print(cli, "Next char: '%c' (n == %i)", c, n);
	    if (n == CLI_LOOP_CTRL_BREAK)
		break;
	    if (n == CLI_LOOP_CTRL_CONTINUE)
		continue;

	    n = cli_loop_process_char(cli, &ctx, c);
	    if (n == CLI_LOOP_CTRL_BREAK)
		break;
	    if (n == CLI_LOOP_CTRL_CONTINUE)
		continue;
	}

	if (ctx.l < 0) break;

	//cli_print(cli, "Process command: '%s'", ctx.cmd);
	n = cli_loop_process_cmd(cli, &ctx);
	if (n == CLI_LOOP_CTRL_BREAK)
	    break;
    }

    return CLI_OK;
}

int check_auth(const char *username, const char *password)
{
    if (strcasecmp(username, "ct") != 0)
	return CLI_ERROR;
    if (strcasecmp(password, "ct") != 0)
	return CLI_ERROR;
    return CLI_OK;
}

int
main()
{
    struct cli_command cmd_show_s = {(char *) "show", NULL, 0, NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};
    struct cli_command cmd_show_cpuspeed_s = {(char *) "cpuspeed", cmd_show_cpuspeed, 0,
                                             (char *) "Show the speed at which the CPU currently operates",
                                             PRIVILEGE_UNPRIVILEGED, MODE_EXEC, NULL, NULL, NULL};

    char crlf[] = "\r\n";
    uint8_t tx = 'A';
    uint8_t rx = 0;
    uint8_t upper = 0;
    struct cli_def cli;

    memset(&cli, 0, sizeof(cli));

    stm_init();

    HAL_GPIO_WritePin(LED_PORT, LED_RED, GPIO_PIN_SET);

    cli_init(&cli);
    cli_read_callback(&cli, uart_cli_read);
    cli_write_callback(&cli, uart_cli_write);
    cli_print_callback(&cli, uart_cli_print);
    cli_set_banner(&cli, "libcli on an STM32");
    cli_set_hostname(&cli, "cryptech");
    cli_set_auth_callback(&cli, check_auth);
    cli_telnet_protocol(&cli, 0);

    cli_register_command2(&cli, &cmd_show_s, NULL);
    cli_register_command2(&cli, &cmd_show_cpuspeed_s, &cmd_show_s);

    HAL_GPIO_WritePin(LED_PORT, LED_RED, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_PORT, LED_GREEN, GPIO_PIN_SET);

    embedded_cli_loop(&cli);

    while (1) {
	led_toggle(LED_GREEN);

	uart_send_char2(STM_UART_USER, tx + upper);
	uart_send_char2(STM_UART_MGMT, tx + upper);
	DELAY();

	if (uart_recv_char2(STM_UART_USER, &rx, 0) == HAL_OK ||
	    uart_recv_char2(STM_UART_MGMT, &rx, 0) == HAL_OK) {
	    led_toggle(LED_YELLOW);
	    if (rx == '\r') {
		upper = upper == 0 ? ('a' - 'A'):0;
	    }
	}

	if (tx++ == 'Z') {
	    /* linefeed after each alphabet */
	    uart_send_string2(STM_UART_USER, crlf);
	    uart_send_string2(STM_UART_MGMT, crlf);
	    tx = 'A';
	    led_toggle(LED_BLUE);
	}
    }
}
