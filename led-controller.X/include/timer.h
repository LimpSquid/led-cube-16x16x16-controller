#ifndef TIMER_H
#define	TIMER_H

#include <stdbool.h>

#define TIMER_TICK_INTERVAL     500 // In microseconds
#define TIMER_POOL_SIZE         5

struct timer_module;

enum
{
    TIMER_TYPE_SOFT = 0,     // A timer that keeps firing at a specific interval, handle is executed on each timeout
    TIMER_TYPE_SINGLE_SHOT,  // A timer that fires only once after it expired, handle is executed on timeout
    TIMER_TYPE_COUNTDOWN,    // A timer that simply counts down and sets the timeout flag once it has expired
    
    __TIMER_TYPE_COUNT
};

enum
{
    TIMER_TIME_UNIT_S = 0,
    TIMER_TIME_UNIT_MS,
    TIMER_TIME_UNIT_US
};

struct timer_module* timer_construct(int type, void (*execute)(struct timer_module*));
void timer_destruct(struct timer_module* timer);

void timer_set_time(struct timer_module* timer, int time, int unit);
void timer_start(struct timer_module* timer, int time, int unit);
void timer_stop(struct timer_module* timer);
void timer_restart(struct timer_module* timer);

bool timer_timed_out(const struct timer_module* timer);
bool timer_is_valid(const struct timer_module* timer);

#endif	/* TIMER_H */

