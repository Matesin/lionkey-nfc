//
// Created by Maty Martan on 14.04.2026.
//
#include "eval_utils.h"

#include <sys/types.h>

#include "stm32h5xx_hal.h"

void timer_start(eval_timer_t *timer)
{
    timer->active = true;
    timer->begin_timestamp = HAL_GetTick();
    timer->last_timestamp = HAL_GetTick();
}
uint32_t timer_get_elapsed_ms(eval_timer_t *timer)
{
    if (!timer->active)
    {
        return timer->last_timestamp - timer->begin_timestamp;
    }
    uint32_t current_timestamp = HAL_GetTick();
    return current_timestamp - timer->begin_timestamp;
}
void timer_stop(eval_timer_t *timer)
{
    timer->last_timestamp = HAL_GetTick();
    timer->active = false;
}
void timer_reset(eval_timer_t *timer)
{
    timer->begin_timestamp = HAL_GetTick();
    timer->last_timestamp = HAL_GetTick();
    timer->active = false;
}

eval_timer_t create_timer(void)
{
    eval_timer_t timer;
    timer.active = false;
    timer.begin_timestamp = 0;
    timer.last_timestamp = 0;
    timer.start = timer_start;
    timer.stop = timer_stop;
    timer.reset = timer_reset;
    timer.elapsed_ms = timer_get_elapsed_ms;
    return timer;
}