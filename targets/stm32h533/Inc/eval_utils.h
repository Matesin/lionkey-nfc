//
// Created by Maty Martan on 14.04.2026.
//

#ifndef LIONKEY_EVAL_UTILS_H
#define LIONKEY_EVAL_UTILS_H
#include <stdbool.h>
#include <stdint.h>

#define CREATE_TIMER_STATIC \
{\
    .active = false,\
    .begin_timestamp = 0,\
    .start = timer_start,\
    .stop = timer_stop,\
    .reset = timer_reset,\
    .elapsed_ms = timer_get_elapsed_ms\
}\

typedef struct eval_timer
{
    uint32_t begin_timestamp;
    uint32_t last_timestamp;
    bool active;
    void(*start) (struct eval_timer *timer);
    uint32_t(*elapsed_ms) (struct eval_timer *timer);
    void (*stop) (struct eval_timer *timer);
    void (*reset) (struct eval_timer *timer);
} eval_timer_t;

void timer_start(eval_timer_t *timer);
uint32_t timer_get_elapsed_ms(eval_timer_t *timer);
void timer_stop(eval_timer_t *timer);
void timer_reset(eval_timer_t *timer);

eval_timer_t create_timer(void);

#endif //LIONKEY_EVAL_UTILS_H