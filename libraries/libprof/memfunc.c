#include <stdint.h>
#include <string.h>

/*
 * Profilable substitutes for mem*(), lacking libc_p.a
 *
 * This code was written with reference to newlib, and was recently
 * brought closer into line with newlib, to make profiling more accurate.
 *
 * Newlib is maintained by Cygwin, which is Red Hat. There is no copyright
 * statement in the corresponding newlib source files, nor is there a
 * COPYING file in newlib/libc/string or newlib/libc. Consider this file
 * to be covered under one or more of the 50 copyright notices in
 * newlib/COPYING, most of which are BSD. In any case, this file is only
 * used for profiling, and is not used in production builds.
 */

#define is_word_aligned(x) (((size_t)(x) & 3) == 0)

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d8 = (uint8_t *)dst;
    uint8_t *s8 = (uint8_t *)src;

    if (n >= sizeof(uint32_t) && is_word_aligned(src) && is_word_aligned(dst)) {
        uint32_t *d32 = (uint32_t *)dst;
        uint32_t *s32 = (uint32_t *)src;
        while (n >= 4 * sizeof(uint32_t)) {
            *d32++ = *s32++;
            *d32++ = *s32++;
            *d32++ = *s32++;
            *d32++ = *s32++;
            n -= 4 * sizeof(uint32_t);
        }
        while (n >= sizeof(uint32_t)) {
            *d32++ = *s32++;
            n -= sizeof(uint32_t);
        }
        d8 = (uint8_t *)d32;
        s8 = (uint8_t *)s32;
    }
    while (n-- > 0) {
        *d8++ = *s8++;
    }

    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    uint8_t *d8 = (uint8_t *)dst;
    uint8_t c8 = (uint8_t)c;

    while (!is_word_aligned(d8)) {
        if (n--)
            *d8++ = c8;
        else
            return dst;
    }
    if (n >= sizeof(uint32_t)) {
        uint32_t *d32 = (uint32_t *)d8;
        uint32_t c32 = (c8 << 24) | (c8 << 16) | (c8 << 8) | (c8);
        while (n >= 4 * sizeof(uint32_t)) {
            *d32++ = c32;
            *d32++ = c32;
            *d32++ = c32;
            *d32++ = c32;
            n -= 4 * sizeof(uint32_t);
        }
        while (n >= sizeof(uint32_t)) {
            *d32++ = c32;
            n -= sizeof(uint32_t);
        }
        d8 = (uint8_t *)d32;
    }
    while (n-- > 0) {
        *d8++ = c8;
    }

    return dst;
}

int memcmp(const void *dst, const void *src, size_t n)
{
    uint8_t *d8 = (uint8_t *)dst;
    uint8_t *s8 = (uint8_t *)src;

    if (n >= sizeof(uint32_t) && is_word_aligned(src) && is_word_aligned(dst)) {
        uint32_t *d32 = (uint32_t *)dst;
        uint32_t *s32 = (uint32_t *)src;
        while (n >= sizeof(uint32_t)) {
            if (*d32 != *s32)
                break;
            d32++;
            s32++;
            n -= sizeof(uint32_t);
        }
        d8 = (uint8_t *)d32;
        s8 = (uint8_t *)s32;
    }
    while (n-- > 0) {
        if (*d8 != *s8)
            return (*d8 - *s8);
        d8++;
        s8++;
    }

    return 0;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d8 = (uint8_t *)dst;
    uint8_t *s8 = (uint8_t *)src;

    if ((s8 < d8) && (d8 < s8 + n)) {
        /* Destructive overlap...have to copy backwards */
        s8 += n;
        d8 += n;
        while (n-- > 0) {
            *--d8 = *--s8;
	}
        return dst;
    }

    return memcpy(dst, src, n);
}
