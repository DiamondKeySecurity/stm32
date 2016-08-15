/*
 * test-fmc.c
 * -----------
 * FPGA communication bus (FMC) tests.
 *
 * Copyright (c) 2016, NORDUnet A/S All rights reserved.
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

/*
  This requires a special bitstream with a special test register.
  See core/platform/novena/fmc/rtl/novena_fmc_top.v, sections marked
  `ifdef test:
  //----------------------------------------------------------------
  // Dummy Register
  //
  // General-purpose register to test FMC interface using STM32
  // demo program instead of core selector logic.
  //
  // This register is a bit tricky, but it allows testing of both
  // data and address buses. Reading from FPGA will always return
  // value, which is currently stored in the test register,
  // regardless of read transaction address. Writing to FPGA has
  // two variants: a) writing to address 0 will store output data
  // data value in the test register, b) writing to any non-zero
  // address will store _address_ of write transaction in the test
  // register.
  //
  // To test data bus, write some different patterns to address 0,
  // then readback from any address and compare.
  //
  // To test address bus, write anything to some different non-zero
  // addresses, then readback from any address and compare returned
  // value with previously written address.
  //
  //----------------------------------------------------------------
  */

#include "stm-init.h"
#include "stm-fmc.h"
#include "stm-uart.h"

#include "test-fmc.h"

static RNG_HandleTypeDef rng_inst;

/* These are some interesting-to-look-at-in-the-debugger values that are declared
 * volatile so that the compiler wouldn't optimize/obscure them.
 */
volatile uint32_t data_diff = 0;
volatile uint32_t addr_diff = 0;


int test_fpga_data_bus(struct cli_def *cli, uint32_t test_rounds)
{
    int c, ok;
    uint32_t rnd, buf;
    HAL_StatusTypeDef hal_result;

    /* initialize stm32 rng */
    rng_inst.Instance = RNG;
    HAL_RNG_Init(&rng_inst);

    /* run some rounds of data bus test */
    for (c = 0; c < test_rounds; c++) {
	data_diff = 0;
	/* try to generate "random" number */
	hal_result = HAL_RNG_GenerateRandomNumber(&rng_inst, &rnd);
	if (hal_result != HAL_OK) {
	    cli_print(cli, "STM32 RNG failed");
	    break;
	}

	/* write value to fpga at address 0 */
	ok = fmc_write_32(0, &rnd);
	if (ok != 0) {
	    cli_print(cli, "FMC write failed: 0x%x", ok);
	    break;
	}

	/* read value from fpga */
	ok = fmc_read_32(0, &buf);
	if (ok != 0) {
	    cli_print(cli, "FMC read failed: 0x%x", ok);
	    break;
	}

	/* compare (abort testing in case of error) */
	data_diff = buf ^ rnd;
	if (data_diff) {
	    cli_print(cli, "Data bus FAIL: expected %lx got %lx", rnd, buf);
	    uart_send_string2(STM_UART_MGMT, (char *) "Binary diff: ");
	    uart_send_number2(STM_UART_MGMT, data_diff, 32, 2);
	    uart_send_string2(STM_UART_MGMT, "\r\n");

	    break;
	}
    }

    if (! data_diff) {
	cli_print(cli, "Sample of data bus test data: expected 0x%lx got 0x%lx", rnd, buf);
    }

    /* return number of successful tests */
    return c;
}

int test_fpga_address_bus(struct cli_def *cli, uint32_t test_rounds)
{
    int c, ok;
    uint32_t rnd, buf;
    HAL_StatusTypeDef hal_result;

    /* initialize stm32 rng */
    rng_inst.Instance = RNG;
    HAL_RNG_Init(&rng_inst);

    /* run some rounds of address bus test */
    for (c = 0; c < test_rounds; c++) {
	addr_diff = 0;
	/* try to generate "random" number */
	hal_result = HAL_RNG_GenerateRandomNumber(&rng_inst, &rnd);
	if (hal_result != HAL_OK) break;

	/* there are 26 physicaly connected address lines on the alpha,
	   but "only" 24 usable for now (the top two ones are used by FMC
	   to choose bank, and we only have one bank set up currently)
	*/
	rnd &= 0x3fffffc;

	/* don't test zero addresses (fpga will store data, not address) */
	if (rnd == 0) continue;

	/* write dummy value to fpga at some non-zero address */
	ok = fmc_write_32(rnd, &buf);
	if (ok != 0) {
	    cli_print(cli, "FMC write failed: 0x%x", ok);
	    break;
	}

	/* read value from fpga */
	ok = fmc_read_32(0, &buf);
	if (ok != 0) {
	    cli_print(cli, "FMC read failed: 0x%x", ok);
	    break;
	}

	/* fpga receives address of 32-bit word, while we need
	   byte address here to compare
	*/
	buf <<= 2;

	/* compare (abort testing in case of error) */
	addr_diff = buf ^ rnd;
	if (addr_diff) {
	    cli_print(cli, "Address bus FAIL: expected 0x%lx got 0x%lx", rnd, buf);
	    uart_send_string2(STM_UART_MGMT, (char *) "Binary diff: ");
	    uart_send_number2(STM_UART_MGMT, addr_diff, 32, 2);
	    uart_send_string2(STM_UART_MGMT, "\r\n");

	    break;
	}
    }

    if (! addr_diff) {
	cli_print(cli, "Sample of address bus test data: expected 0x%lx got 0x%lx", rnd, buf);
    }

    /* return number of successful tests */
    return c;
}

