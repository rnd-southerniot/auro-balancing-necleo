/**
 * @file microros_time.c
 * @brief POSIX clock_gettime implementation for micro-ROS on STM32.
 *
 * micro-ROS requires clock_gettime(CLOCK_REALTIME/CLOCK_MONOTONIC).
 * We provide it using HAL_GetTick() (1ms resolution from SysTick).
 */

#include "stm32f4xx_hal.h"
#include <time.h>
#include <errno.h>

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    (void)clk_id;
    if (tp == NULL) {
        errno = EINVAL;
        return -1;
    }

    uint32_t ms = HAL_GetTick();
    tp->tv_sec  = (time_t)(ms / 1000U);
    tp->tv_nsec = (long)((ms % 1000U) * 1000000UL);
    return 0;
}
