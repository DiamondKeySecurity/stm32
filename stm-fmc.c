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
    _fmc_fpga_inst.Init.WaitSignalActive = FMC_WAIT_TIMING_DURING_WS;

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
    fmc_timing.CLKDivision = 2;

    // stm is too slow to work with min allowed 2-cycle latency
    fmc_timing.DataLatency = 3;

    // don't care in sync mode
    fmc_timing.AccessMode = FMC_ACCESS_MODE_A;

    // initialize fmc
    HAL_SRAM_Init(&_fmc_fpga_inst, &fmc_timing, NULL);
}


static int _fmc_nwait_idle(void)
{
    int cnt;

    // poll NWAIT (number of iterations is limited)
    for (cnt=0; cnt<FMC_FPGA_NWAIT_MAX_POLL_TICKS; cnt++)
    {
        // read pin state
        if (HAL_GPIO_ReadPin(FMC_GPIO_PORT_NWAIT, FMC_GPIO_PIN_NWAIT) == FMC_NWAIT_IDLE)
            return 0;
    }

    return -1;
}

int fmc_write_32(uint32_t addr, uint32_t *data)
{
    // calculate target fpga address
    uint32_t ptr = FMC_FPGA_BASE_ADDR + (addr & FMC_FPGA_ADDR_MASK);

    __disable_irq();

    int status =
        // write data to fpga
        (HAL_SRAM_Write_32b(&_fmc_fpga_inst, (uint32_t *)ptr, data, 1) != HAL_OK) ||
        // wait for transaction to complete
        _fmc_nwait_idle();

    __enable_irq();

    return status;
}

static inline int _fmc_read_32(uint32_t *ptr, uint32_t *data)
{
    return
        // read data from fpga
        (HAL_SRAM_Read_32b(&_fmc_fpga_inst, (uint32_t *)ptr, data, 1) != HAL_OK) ||
        // wait for transaction to complete
        _fmc_nwait_idle();

}

int fmc_read_32(uint32_t addr, uint32_t *data)
{
    // calculate target fpga address
    uint32_t ptr = FMC_FPGA_BASE_ADDR + (addr & FMC_FPGA_ADDR_MASK);

    /* Pavel says:
     * The short story is like, on one hand STM32 has a dedicated FMC_NWAIT
     * pin, that can be used in variable-latency data transfer mode. On the
     * other hand STM32 also has a very nasty hardware bug associated with
     * FMC_WAIT, that causes processor to freeze under certain conditions.
     * Because of this FMC_NWAIT cannot be used and FPGA can't properly signal
     * to STM32, when data transfer is done. Because of that we have to read
     * two times.
     */

    /* Add some level of reentrancy protection. When running under a
     * preemptive multitasker, with two threads banging on the fpga, we appear
     * to sometimes read the wrong value. I think this is because the second
     * read counts on the first read to put the correct value on the address
     * bus.
     */
    __disable_irq();

    int status =
        _fmc_read_32((uint32_t *)ptr, data) ||
        _fmc_read_32((uint32_t *)ptr, data);

    __enable_irq();

    return status;
}
