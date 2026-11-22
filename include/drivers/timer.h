#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void     timer_init(uint32_t frequency);
uint32_t timer_get_ticks(void);
uint32_t timer_get_uptime_sec(void);
void     timer_sleep_ms(uint32_t ms);

#endif
