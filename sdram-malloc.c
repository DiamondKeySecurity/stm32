/*
 * sdram_malloc.c
 * --------------
 * Use SDRAM for a very limited sort of heap.
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

/* Allocate memory from SDRAM1. This is not a general allocator, and
 * should only be used with the greatest of caution and care.
 *
 * This is intended for allocating memory that will either never be freed
 * (e.g. thread stack buffers), or will be freed as soon as the requesting
 * function is done with it (e.g. file upload buffers).
 *
 * Memory should be freed in reverse order of allocation (most
 * recent first).
 *
 * There are no memory block control fields, so no protection to ensure
 * that memory that is freed is actually the memory that was allocated.
 */

#include <stddef.h>
#include <stdint.h>

/* end of variables declared with __attribute__((section(".sdram1"))) */
extern uint8_t _esdram1 __asm ("_esdram1");
static uint8_t * const sdram_heap_base = &_esdram1;

/* end of sdram1 section */
extern uint8_t __end_sdram1 __asm ("__end_sdram1");
static uint8_t * const sdram_heap_end = &__end_sdram1;

/* current heap top */
static uint8_t *sdram_heap = &_esdram1;

/* Allocate some memory. Allocations are not padded, because file upload will
 * call repeatedly to write blocks of data to a "file". OTOH, it's a good
 * idea to allocate padding after the last write, so the next allocation
 * will be aligned.
 */
uint8_t *sdram_malloc(size_t size)
{
    uint8_t *p = sdram_heap;

    if (p + size > sdram_heap_end)
        return NULL;

    sdram_heap += size;
    return p;
}

/* Free some memory allocated from sdram. This does only the most basic of
 * sanity checks.
 */
void sdram_free(uint8_t *s)
{
    if (s < sdram_heap_base || s >= sdram_heap_end)
        return;
    if (s < sdram_heap)
        sdram_heap = s;
}
