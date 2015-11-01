#include <stdint.h>

#include "stm32f4xx_hal.h"

/* Don't #include <sys/time.h> because of conflicting prototype in newlib. */

/* from the manpage: */
struct timeval {
    time_t      tv_sec;     /* seconds */
    suseconds_t tv_usec;    /* microseconds */
};

struct timezone {
    int tz_minuteswest;     /* minutes west of Greenwich */
    int tz_dsttime;         /* type of DST correction */
};

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    uint32_t tick = HAL_GetTick();      /* uptime in ms */

    tv->tv_sec = tick / 1000;
    tv->tv_usec = (tick % 1000) * 1000;

    return 0;
}
