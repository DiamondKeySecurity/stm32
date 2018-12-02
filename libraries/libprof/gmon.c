/*-
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file is taken from Cygwin distribution. Please keep it in sync.
 * The differences should be within __MINGW32__ guard.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "gmon.h"

#define bzero(ptr,size) memset (ptr, 0, size);
#define ERR(s) write(2, s, sizeof(s))

/* profiling frequency.  (No larger than 1000) */
/* Note this doesn't set the frequency, but merely describes it. */
#define PROF_HZ                 1000

struct gmonparam _gmonparam = { off, NULL, 0, NULL, 0, NULL, 0, 0, 0, 0, 0};

void monstartup(size_t lowpc, size_t highpc)
{
  static char already_setup = 0;
  struct gmonparam *p = &_gmonparam;

  if (already_setup) {
    /* reinitialize counters and arcs */
    bzero(p->kcount, p->kcountsize);
    bzero(p->froms, p->fromssize);
    bzero(p->tos, p->tossize);
    p->state = on;
    return;
  }
  already_setup = 1;

  /* enable semihosting, for eventual output */
  extern void initialise_monitor_handles(void);
  initialise_monitor_handles();

  /*
   * round lowpc and highpc to multiples of the density we're using
   * so the rest of the scaling (here and in gprof) stays in ints.
   */
  p->lowpc = ROUNDDOWN(lowpc, HISTFRACTION * sizeof(HISTCOUNTER));
  p->highpc = ROUNDUP(highpc, HISTFRACTION * sizeof(HISTCOUNTER));
  p->textsize = p->highpc - p->lowpc;
  p->kcountsize = p->textsize / HISTFRACTION;
  p->fromssize = p->textsize / HASHFRACTION;
  p->tolimit = p->textsize * ARCDENSITY / 100;
  if (p->tolimit < MINARCS) {
    p->tolimit = MINARCS;
  } else if (p->tolimit > MAXARCS) {
    p->tolimit = MAXARCS;
  }
  p->tossize = p->tolimit * sizeof(struct tostruct);

  extern void *hal_allocate_static_memory(const size_t size);
  void *cp = hal_allocate_static_memory(p->kcountsize + p->fromssize + p->tossize);
  if (cp == NULL) {
    ERR("monstartup: out of memory\n");
    return;
  }

  bzero(cp, p->kcountsize + p->fromssize + p->tossize);
  p->kcount = (unsigned short *)cp; cp += p->kcountsize;
  p->froms = (unsigned short *)cp;  cp += p->fromssize;
  p->tos = (struct tostruct *)cp;

  p->state = on;
}

void _mcleanup(void)
{
  static const char gmon_out[] = "gmon.out";
  int fd;
  struct gmonparam *p = &_gmonparam;

  if (p->state == err) {
    ERR("_mcleanup: tos overflow\n");
  }

  fd = open(gmon_out , O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0666);
  if (fd < 0) {
    perror( gmon_out );
    return;
  }

  struct gmonhdr hdr = {
    .lpc = p->lowpc,
    .hpc = p->highpc,
    .ncnt = p->kcountsize + sizeof(struct gmonhdr),
    .version = GMONVERSION,
    .profrate = PROF_HZ
  };
  write(fd, &hdr, sizeof(hdr));

  write(fd, p->kcount, p->kcountsize);

  for (size_t fromindex = 0; fromindex < p->fromssize / sizeof(*p->froms); fromindex++) {
    size_t frompc = p->lowpc + fromindex * HASHFRACTION * sizeof(*p->froms);
    for (size_t toindex = p->froms[fromindex]; toindex != 0; toindex = p->tos[toindex].next) {
      struct rawarc arc = {
        .frompc = frompc,
        .selfpc = p->tos[toindex].selfpc,
        .count = p->tos[toindex].count
      };
      write(fd, &arc, sizeof(arc));
    }
  }

  close(fd);
}

void _mcount_internal(size_t frompc, size_t selfpc)
{
  register unsigned short       *fromptr;
  register struct tostruct      *top;
  register unsigned short       toindex;
  struct gmonparam *p = &_gmonparam;

  /*
   *    check that we are profiling and that we aren't recursively invoked.
   *    check that frompc is a reasonable pc value.
   */
  if (p->state != on || (frompc -= p->lowpc) > p->textsize) {
    return;
  }

  fromptr = &p->froms[frompc / (HASHFRACTION * sizeof(*p->froms))];
  toindex = *fromptr;           /* get froms[] value */

  if (toindex == 0) {           /* we haven't seen this caller before */
    toindex = ++p->tos[0].next; /* index of the last used record in the array */
    if (toindex >= p->tolimit) { /* more tos[] entries than we can handle! */
    overflow:
      p->state = err; /* halt further profiling */
#define TOLIMIT "mcount: tos overflow\n"
      write (2, TOLIMIT, sizeof(TOLIMIT));
      return;
    }
    *fromptr = toindex;         /* store new 'to' value into froms[] */
    top = &p->tos[toindex];
    top->selfpc = selfpc;
    top->count = 1;
    top->next = 0;
  }

  else {                        /* we've seen this caller before */
    top = &p->tos[toindex];
    if (top->selfpc == selfpc) {
      /*
       *  arc at front of chain; usual case.
       */
      top->count++;
    }

    else {
      /*
       *  have to go looking down chain for it.
       *  top points to what we are looking at,
       *  prevtop points to previous top.
       *  we know it is not at the head of the chain.
       */
      while (1) {
        if (top->next == 0) {
          /*
           *    top is end of the chain and none of the chain
           *    had top->selfpc == selfpc.
           *    so we allocate a new tostruct
           *    and put it at the head of the chain.
           */
          toindex = ++p->tos[0].next;
          if (toindex >= p->tolimit) {
            goto overflow;
          }
          top = &p->tos[toindex];
          top->selfpc = selfpc;
          top->count = 1;
          top->next = *fromptr;
          *fromptr = toindex;
          break;
        }

        else {
          /*
           *    otherwise, check the next arc on the chain.
           */
          register struct tostruct *prevtop = top;
          top = &p->tos[top->next];
          if (top->selfpc == selfpc) {
            /*
             *  there it is.
             *  increment its count
             *  move it to the head of the chain.
             */
            top->count++;
            toindex = prevtop->next;
            prevtop->next = top->next;
            top->next = *fromptr;
            *fromptr = toindex;
            break;
          }
        }
      }
    }
  }

  return;               /* normal return restores saved registers */
}

#include <stdint.h>
#include "stm32f4xx_hal.h"      /* __get_MSP */

/* called from the SysTick handler */
void profil_callback(void)
{
  struct gmonparam *p = &_gmonparam;

  if (p->state == on) {
    /* The interrupt mechanism pushes xPSR, PC, LR, R12, and R3-R0 onto the
     * stack, so PC is the 6th word from the top at that point. However, the
     * normal function entry code pushes registers as well, so the stack
     * offset right now depends on the call tree that got us here.
     */
    size_t pc = (size_t)((uint32_t *)__get_MSP())[6 + 6];
    if ((pc -= p->lowpc) < p->textsize) {
      size_t idx = pc / (HISTFRACTION * sizeof(*p->kcount));
      p->kcount[idx]++;
    }
  }
}
