/*
 * ekermit-test.c
 * --------------
 * A test program for the E-Kermit library.
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

/* This receives one or more files from a kermit client (e.g. C-Kermit),
 * and stores them in a minimal file system on SDRAM.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "stm-init.h"
#include "stm-sdram.h"
#include "sdram-malloc.h"
#include "stm-kermit.h"

/* alignment for file allocations */
#define ALIGN 16

/* maximum length of file name */
#define NAMELEN 32

/* maximum number of files */
#define NFILE 4

typedef struct {
  char name[NAMELEN];
  uint8_t *loc;
  size_t len;
} filetab_t;
filetab_t filetab[NFILE], *curfile;
int nfile;

#ifdef DUMP
static void hexdump(uint8_t *buf, int len)
{
    for ( ; len > 0; buf += 16, len -= 16) {
        int plen = (len > 16) ? 16 : len;
        int i;
        for (i = 0; i < plen; ++i)
            printf("%02x ", buf[i]);
        for ( ; i < 16; ++i)
            printf("   ");
        printf("| ");
        for (i = 0; i < plen; ++i)
            printf("%c", isprint(buf[i]) ? buf[i] : ' ');
        printf("\n");
    }
}
#endif

/* Override weak function defintions for callouts in stm-kermit.c */

/* Called when kermit receives an F (file) packet
 */
int kermit_openfile(unsigned char *name)
{
    if (nfile == NFILE) {
        curfile = NULL;
        return -1;
    }
    curfile = &filetab[nfile];
    strncpy(curfile->name, (char *)name, sizeof(curfile->name));
    curfile->loc = sdram_malloc(0);
    if (curfile->loc == NULL) {
        curfile = NULL;
        return -1;
    }
    curfile->len = 0;
    ++nfile;
    return 0;
}

/* Called when kermit's receive buffer is full, after some number of D
 * (data) packets
 */
int kermit_writefile(unsigned char *buf, int len)
{
    if (curfile == NULL)
        return -1;
    uint8_t *writeptr = sdram_malloc(len);
    if (writeptr == NULL)
        return -1;
    memcpy(writeptr, buf, len);
    curfile->len += len;
    return 0;
}

/* Called when kermit receives a Z (EOF) packet
 */
int kermit_closefile(void)
{
    if (curfile == NULL)
        return -1;
    int pad = ALIGN - (curfile->len & (ALIGN - 1));
    if (sdram_malloc(pad) == NULL)
        return -1;
    curfile = NULL;
    return 0;
}

/* Called when kermit receives a Z (EOF) packet with a D (discard) flag
 */
int kermit_cancelfile(void)
{
    if (curfile == NULL)
        return -1;
    sdram_free(curfile->loc);
    memset(curfile, 0, sizeof(*curfile));
    --nfile;
    return 0;
}

/* The main function is pretty simple, because the kermit library does all
 * the work out of sight.
 */
int main(void)
{
    stm_init();
    sdram_init();

    while (1) {
        memset((void *)filetab, 0, sizeof(filetab));
        curfile = NULL;
        nfile = 0;
        kermit_main();
        for (int i = 0; i < nfile; ++i) {
            filetab_t *f = &filetab[i];
            printf("%d. %-16s %p %d\n", i, f->name, f->loc, f->len);
#ifdef DUMP
            hexdump(f->loc, f->len);
#endif
            sdram_free(f->loc);
        }
    }
}
