/*
 * ringbuf.h
 * ---------
 * Ring buffers for more reliable UART receives.
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

#ifndef RINGBUF_SIZE
#define RINGBUF_SIZE  256
#endif

typedef struct {
    int ridx;
    volatile int widx;
    uint8_t buf[RINGBUF_SIZE];
} ringbuf_t;

inline void ringbuf_init(ringbuf_t *rb)
{
    memset(rb, 0, sizeof(*rb));
}

inline int ringbuf_read_char(ringbuf_t *rb, uint8_t *c)
{
    if (rb->ridx != rb->widx) {
        *c = rb->buf[rb->ridx];
        if (++rb->ridx >= sizeof(rb->buf))
            rb->ridx = 0;
        return 1;
    }
    return 0;
}

inline void ringbuf_write_char(ringbuf_t *rb, uint8_t c)
{
    rb->buf[rb->widx] = c;
    if (++rb->widx >= sizeof(rb->buf))
        rb->widx = 0;
}

inline int ringbuf_empty(ringbuf_t *rb)
{
    return (rb->ridx == rb->widx);
}

inline int ringbuf_count(ringbuf_t *rb)
{
    int len = rb->widx - rb->ridx;
    if (len < 0)
        len += sizeof(rb->buf);
    return len;
}
