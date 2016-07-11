/*
 * mgmt-misc.c
 * -----------
 * Miscellaneous CLI functions.
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

/* Rename both CMSIS HAL_OK and libhal HAL_OK to disambiguate */
#define HAL_OK CMSIS_HAL_OK
#include "stm-init.h"
#include "stm-uart.h"
#include "sdram-malloc.h"

#include "mgmt-cli.h"
#include "mgmt-misc.h"

#undef HAL_OK
#define HAL_OK LIBHAL_OK
#include "hal.h"
#include "hal_internal.h"
#include "cryptech_signing_key.h"
#undef HAL_OK

#include <string.h>

#define pad(n, sz) (((n) + (sz-1)) & ~(sz-1))

static hal_error_t verify_signature(const uint8_t * const data,
                                    const size_t data_len,
                                    const uint8_t * const signature,
                                    const size_t signature_len)
{
    hal_error_t err;
    const hal_core_t *core = NULL;
    const hal_hash_descriptor_t * const descriptor = hal_hash_sha256;
    hal_hash_state_t *state = NULL;
    uint8_t statebuf[descriptor->hash_state_length];
    uint8_t digest[signature_len];

#if 1
    /* HACK - find the second sha256 core, to avoid interfering with rpc.
     * If there isn't a second one, this will set core to NULL, and
     * hal_hash_initialize will find the first one.
     */
    core = hal_core_find(descriptor->core_name, core);
    core = hal_core_find(descriptor->core_name, core);
#endif

    if ((err = hal_hash_initialize(core, descriptor, &state, statebuf, sizeof(statebuf))) != LIBHAL_OK ||
        (err = hal_hash_update(state, data, data_len)) != LIBHAL_OK ||
        (err = hal_hash_finalize(state, digest, sizeof(digest))) != LIBHAL_OK)
        return err;

    const uint8_t name[] = "Cryptech signing key";
    const size_t name_len = sizeof(name);
    uint8_t der[65];	/* ?? */
    int ks_hint;

    if ((err = hal_ks_fetch(HAL_KEY_TYPE_EC_PUBLIC, name, name_len, NULL, NULL, der, NULL, sizeof(der), &ks_hint)) != LIBHAL_OK)
        /* get the built-in signing key */
        memcpy(der, cryptech_signing_key, sizeof(der));
    
    uint8_t keybuf[hal_ecdsa_key_t_size];
    hal_ecdsa_key_t *key = NULL;

    if ((err = hal_ecdsa_public_key_from_der(&key, keybuf, sizeof(keybuf), der, sizeof(der))) != LIBHAL_OK ||
        (err = hal_ecdsa_verify(NULL, key, digest, sizeof(digest), signature, signature_len))         != LIBHAL_OK)
        return err;

    return LIBHAL_OK;
}

int cli_receive_data(struct cli_def *cli, uint8_t **filebuf, size_t *file_len, size_t chunksize)
{
    uint32_t filesize = 0, counter = 0;
    uint8_t *writeptr;
    size_t n = chunksize;

    if (! control_mgmt_uart_dma_rx(DMA_RX_STOP)) {
	cli_print(cli, "Failed stopping DMA");
	return CLI_OK;
    }

    cli_print(cli, "OK, write size (4 bytes), data in %li byte chunks", (uint32_t) n);

    if (uart_receive_bytes(STM_UART_MGMT, (void *) &filesize, 4, 1000) != CMSIS_HAL_OK) {
	cli_print(cli, "Receive timed out");
	return CLI_ERROR;
    }

    *file_len = (size_t)(pad(filesize, chunksize));
    if ((*filebuf = sdram_malloc(*file_len)) == NULL) {
        cli_print(cli, "File buffer allocation failed");
        return CLI_ERROR;
    }

    /* By initializing buf to the same value that erased flash has (0xff), we don't
     * have to try and be smart when writing the last page of data to a flash memory.
     */
    memset(*filebuf, 0xff, *file_len);
    writeptr = *filebuf;

    cli_print(cli, "Send %li bytes of data", filesize);

    while (filesize) {

	if (filesize < n) n = filesize;

	if (uart_receive_bytes(STM_UART_MGMT, (void *) writeptr, n, 1000) != CMSIS_HAL_OK) {
	    cli_print(cli, "Receive timed out");
            goto errout;
	}
        writeptr += n;
	filesize -= n;

	counter++;
	uart_send_bytes(STM_UART_MGMT, (void *) &counter, 4);
    }

    uint32_t sigsize;
    cli_print(cli, "Send signature size (4 bytes)");
    if (uart_receive_bytes(STM_UART_MGMT, (void *) &sigsize, 4, 1000) != CMSIS_HAL_OK) {
	cli_print(cli, "Receive timed out");
	goto errout;
    }

    if (sigsize < 70 || sigsize > 72) {
        cli_print(cli, "Unexpected signature size %d, expected 70-72", (int)sigsize);
        goto errout;
    }
    uint8_t sigbuf[72];
    cli_print(cli, "Send %li bytes of data", sigsize);
    if (uart_receive_bytes(STM_UART_MGMT, (void *) sigbuf, sigsize, 1000) != CMSIS_HAL_OK) {
        cli_print(cli, "Receive timed out");
        goto errout;
    }
    /* Signature is in DER format: sequence of 2 integers. Decode into
     * what ecdsa expects: an octet string consisting of concatenated
     * values for r and s, each of which occupies half of the octet string.
     */
    /* XXX should use the asn1 decoder, but just going to hack this for now */
    int i = 0;
    #define ASN1_SEQUENCE 0x30
    #define ASN1_INTEGER  0x02
    if (sigbuf[i++] != ASN1_SEQUENCE) {
        cli_print(cli, "Error parsing signature");
        goto errout;
    }
    i++;
    if (sigbuf[i++] != ASN1_INTEGER) {
        cli_print(cli, "Error parsing signature");
        goto errout;
    }
    if (sigbuf[i++] == 0x21) i++;
    memmove(&sigbuf[0], &sigbuf[i], 32);
    i += 32;
    if (sigbuf[i++] != ASN1_INTEGER) {
        cli_print(cli, "Error parsing signature");
        goto errout;
    }
    if (sigbuf[i++] == 0x21) i++;
    memmove(&sigbuf[32], &sigbuf[i], 32);

    if (verify_signature(*filebuf, *file_len, sigbuf, 64) != LIBHAL_OK) {
        cli_print(cli, "Signature verification FAILED");
        goto errout;
    }
    cli_print(cli, "Signature verification SUCCEEDED");
    return CLI_OK;

errout:
    sdram_free(*filebuf);
    *filebuf = NULL;
    *file_len = 0;
    return CLI_ERROR;
}

static int cmd_reboot(struct cli_def *cli, const char *command, char *argv[], int argc)
{
    cli_print(cli, "\n\n\nRebooting\n\n\n");
    HAL_NVIC_SystemReset();

    /*NOTREACHED*/
    return CLI_OK;
}

void configure_cli_misc(struct cli_def *cli)
{
    /* reboot */
    cli_command_root_node(reboot, "Reboot the STM32");
}

