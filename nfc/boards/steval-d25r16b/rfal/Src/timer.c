//
// Created by Maty Martan on 02.03.2026.
//
#include "timer.h"
#include "stm32h5xx_hal.h"

/*
 * Deadline = now + ms, in HAL ticks (1 tick = 1 ms if SysTick configured that way).
 * HAL_GetTick() is uint32_t and wraps around naturally.
 */
platformTimer_t timerCalculateTimer(uint32_t ms)
{
    return (platformTimer_t)(HAL_GetTick() + ms);
}

/*
 * Wrap-around safe expiry check:
 * If (now - deadline) >= 0 in signed arithmetic, deadline is reached/passed.
 */

bool timerIsExpired(platformTimer_t deadline)
{
    uint32_t now = HAL_GetTick();
    return ((int32_t)(now - deadline) >= 0);
}

uint32_t timerGetRemaining(platformTimer_t deadline)
{
    uint32_t now = HAL_GetTick();

    if ((int32_t)(now - deadline) >= 0) {
        return 0U;
    }
    return (uint32_t)(deadline - now);
}