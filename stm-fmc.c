/*
 * stm-fmc.c
 * ---------
 * Functions to set up and use the FMC bus.
 *
 * Copyright (c) 2015, NORDUnet A/S All rights reserved.
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
#include "stm-init.h"
#include "stm-fmc.h"


static SRAM_HandleTypeDef _fmc_fpga_inst;

void fmc_init(void)
{
    static int initialized = 0;
    if (initialized) return;
    initialized = 1;

    // configure fmc pins

    GPIO_InitTypeDef GPIO_InitStruct;

    // enable fmc clock
    __HAL_RCC_FMC_CLK_ENABLE();

    fmc_af_gpio(GPIOB, GPIO_PIN_7);
    fmc_af_gpio(GPIOD, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4
		| GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9
		| GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13
		| GPIO_PIN_14 | GPIO_PIN_15);

    /*
     * When FMC is working with fixed latency, NWAIT pin (PD6) must not be
     * configured in AF mode, according to STM32F429 errata.
     */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    fmc_af_gpio(GPIOE, GPIO_PIN_2
		| GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7
		| GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11
		| GPIO_PIN_12 | GPIO_PIN_13 |GPIO_PIN_14 | GPIO_PIN_15);
    fmc_af_gpio(GPIOF, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
		| GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_12 | GPIO_PIN_13
		| GPIO_PIN_14 | GPIO_PIN_15);
    fmc_af_gpio(GPIOG, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
		| GPIO_PIN_4 | GPIO_PIN_5);
    fmc_af_gpio(GPIOH, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11
		| GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    fmc_af_gpio(GPIOI, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
		| GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_9 | GPIO_PIN_10);

    // configure fmc registers

    /*
     * fill internal fields
     */
    _fmc_fpga_inst.Instance = FMC_NORSRAM_DEVICE;
    _fmc_fpga_inst.Extended = FMC_NORSRAM_EXTENDED_DEVICE;


    /*
     * configure fmc interface settings
     */

    // use the first bank and corresponding chip select
    _fmc_fpga_inst.Init.NSBank = FMC_NORSRAM_BANK1;

    // data and address buses are separate
    _fmc_fpga_inst.Init.DataAddressMux = FMC_DATA_ADDRESS_MUX_DISABLE;

    // fpga mimics psram-type memory
    _fmc_fpga_inst.Init.MemoryType = FMC_MEMORY_TYPE_PSRAM;

    // data bus is 32-bit
    _fmc_fpga_inst.Init.MemoryDataWidth = FMC_NORSRAM_MEM_BUS_WIDTH_32;

    // read transaction is sync
    _fmc_fpga_inst.Init.BurstAccessMode = FMC_BURST_ACCESS_MODE_ENABLE;

    // this _must_ be configured to high, according to errata, otherwise
    // the processor may hang after trying to access fpga via fmc
    _fmc_fpga_inst.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_HIGH;

    // wrap mode is not supported
    _fmc_fpga_inst.Init.WrapMode = FMC_WRAP_MODE_DISABLE;

    // don't care in fixed latency mode
    _fmc_fpga_inst.Init.WaitSignalActive = FMC_WAIT_TIMING_BEFORE_WS;

    // allow write access to fpga
    _fmc_fpga_inst.Init.WriteOperation = FMC_WRITE_OPERATION_ENABLE;

    // use fixed latency mode (ignore wait signal)
    _fmc_fpga_inst.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE;

    // write and read have same timing
    _fmc_fpga_inst.Init.ExtendedMode = FMC_EXTENDED_MODE_DISABLE;

    // don't care in sync mode
    _fmc_fpga_inst.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;

    // write transaction is sync
    _fmc_fpga_inst.Init.WriteBurst = FMC_WRITE_BURST_ENABLE;

    // keep clock always active
    _fmc_fpga_inst.Init.ContinuousClock = FMC_CONTINUOUS_CLOCK_SYNC_ASYNC;

    /*
     * configure fmc timing parameters
     */
    FMC_NORSRAM_TimingTypeDef fmc_timing;

    // don't care in sync mode
    fmc_timing.AddressSetupTime = 15;

    // don't care in sync mode
    fmc_timing.AddressHoldTime = 15;

    // don't care in sync mode
    fmc_timing.DataSetupTime = 255;

    // not needed, since nwait will be polled manually
    fmc_timing.BusTurnAroundDuration = 0;

    // use smallest allowed divisor for best performance
    //
    // FMC_CLK = HCLK / CLKDivision, HCLK is 180 MHz
    //
    // Allowed values for CLKDivision are integers >= 2.
    //
    // Division == 2: FMC_CLK = 180 / 2 = 90 MHz (highest allowed frequency)
    // Division == 3: FMC_CLK = 180 / 3 = 60 MHz (one step below)
    // ...
    //

//  fmc_timing.CLKDivision = 2;     // 90 MHz
    fmc_timing.CLKDivision = 3;     // 60 MHz

    // use min suitable for fastest transfer
    fmc_timing.DataLatency = 4;

    // don't care in sync mode
    fmc_timing.AccessMode = FMC_ACCESS_MODE_A;

    // initialize fmc
    HAL_SRAM_Init(&_fmc_fpga_inst, &fmc_timing, NULL);
		
    // STM32 only enables FMC clock right before the very first read/write
    // access. FPGA takes certain time (<= 100 us) to lock its PLL to this frequency,
    // so a certain number of initial FMC transactions may be missed. One read transaction
    // takes ~0.1 us (9 ticks @ 90 MHz), so doing 1000 dummy reads will make sure, that FPGA
    // has already locked its PLL and is ready. Another way around is to repeatedly read
    // some register that is guaranteed to have known value until reading starts returning
    // correct data.
		
    // to prevent compiler from optimizing this away, we pretent we're calculating sum
    int cyc;
    uint32_t sum;
    volatile uint32_t part;

    for (cyc = 0; cyc < 1000; cyc++) {
      part = *(__IO uint32_t *)FMC_FPGA_BASE_ADDR;
      sum += part;
    }
}
