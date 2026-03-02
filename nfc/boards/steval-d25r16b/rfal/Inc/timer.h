//
// Created by Maty Martan on 02.03.2026.
//

#ifndef LIONKEY_TIMER_H
#define LIONKEY_TIMER_H

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t platformTimer_t;

/* ms is relative duration */
platformTimer_t timerCalculateTimer(uint32_t ms);

/* deadline is absolute timestamp (as returned by timerCalculateTimer) */
bool timerIsExpired(platformTimer_t deadline);

/* Remaining time until deadline in ms (0 if expired) */
uint32_t timerGetRemaining(platformTimer_t deadline);

#endif //LIONKEY_TIMER_H