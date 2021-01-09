#include "../include/timer.h"
#include "../include/kernel_task.h"
#include "../include/assert.h"
#include <stddef.h>
#include <limits.h>

struct timer_module
{
    unsigned long interval;
    unsigned long ticks;
    void(*execute)(struct timer_module* timer);
    
    struct 
    {
        unsigned char type        :3;
        unsigned char assigned    :1;
        unsigned char suspended   :1;
        unsigned char timedout    :1;
        unsigned char reserved    :2;
    } opt;
};

static unsigned long timer_compute_ticks(int time, int unit);

static int timer_ttask_init(void);
static void timer_ttask_execute(void);
static void timer_ttask_configure(struct kernel_ttask_param* const param);
KERN_TTASK(timer, timer_ttask_init, timer_ttask_execute, timer_ttask_configure, KERN_INIT_EARLY);

#ifdef TIMER_POOL_SIZE
    #if (TIMER_POOL_SIZE < 1)
        #error "Timer pool size must be a non negative integer with a minimum of 1"
    #else
        static struct timer_module timer_pool[TIMER_POOL_SIZE];
    #endif
#else
    #error "Define timer pool size 'TIMER_POOL_SIZE' with a number >= 1"
#endif
        
struct timer_module* timer_construct(int type, void (*execute)(struct timer_module*))
{
    struct timer_module* timer = NULL;
    if(type < 0 || type >= __TIMER_TYPE_COUNT)
        return timer;
    
    // Search for an unused timer
    for(unsigned int i = 0; i < TIMER_POOL_SIZE; ++i) {
        if(!timer_pool[i].opt.assigned) {
            timer = &timer_pool[i];
            break;
        }
    }
    
    // Configure timer, if found
    if(NULL != timer) {
        timer->interval = 0;
        timer->ticks = 0;
        timer->execute = execute;
        timer->opt.type = type;
        timer->opt.suspended = true;
        timer->opt.timedout = false;
        timer->opt.assigned = true;
    }
    return timer;
}

void timer_destruct(struct timer_module* timer)
{
    ASSERT(NULL != timer);
    
    timer->opt.assigned = false;
}

void timer_set_time(struct timer_module* timer, int time, int unit)
{
    ASSERT(NULL != timer);
    
    timer->interval = timer_compute_ticks(time, unit);
    timer->ticks = timer->interval;
}

void timer_start(struct timer_module* timer, int time, int unit)
{
    ASSERT(NULL != timer);
    
    timer->interval = timer_compute_ticks(time, unit);
    timer->ticks = timer->interval;
    timer->opt.timedout = false;
    timer->opt.suspended = false;
}

void timer_stop(struct timer_module* timer)
{
    ASSERT(NULL != timer);
    
    timer->opt.suspended = true;
}

void timer_restart(struct timer_module* timer)
{
    ASSERT(NULL != timer);
    
    timer->ticks = timer->interval;
    timer->opt.timedout = false;
    timer->opt.suspended = false;
}

bool timer_timed_out(const struct timer_module* timer)
{
    ASSERT(NULL != timer);
    
    return timer->opt.timedout;
}

bool timer_is_valid(const struct timer_module* timer)
{
    ASSERT(NULL != timer);
    
    return timer->opt.assigned;
}

static unsigned long timer_compute_ticks(int time, int unit)
{
    unsigned long ticks;
    switch(unit) {
        default: // Default to seconds
        case TIMER_TIME_UNIT_S:
            //@Todo: Currently limited to 12 bits, fixme?
            if(time > 4096)
                time = 4096;
            ticks = (time * 1000000LU) / TIMER_TICK_INTERVAL;
            break;
        case TIMER_TIME_UNIT_MS:
            ticks = (time * 1000LU) / TIMER_TICK_INTERVAL;
            break;
        case TIMER_TIME_UNIT_US:
            ticks = time / TIMER_TICK_INTERVAL; 
            break;
    }
    return ticks;
}

static int timer_ttask_init(void)
{
    for(unsigned int i = 0; i < TIMER_POOL_SIZE; ++i)
        timer_pool[i].opt.assigned = false;
    
    return KERN_INIT_SUCCCES;
}

static void timer_ttask_execute(void)
{
    void (*execute)(struct timer_module*) = NULL;

    struct timer_module* timer = timer_pool;
    for(unsigned int i = 0; i < TIMER_POOL_SIZE; ++i) {
        if(timer->opt.assigned && !timer->opt.suspended) {
            
            // Decrement tick count
            if(timer->ticks > 0)
                timer->opt.timedout = !!!(--timer->ticks); // Determine if timer timed out after decrement.
            else 
                timer->opt.timedout = true;
            
            if(timer->opt.timedout) {
                switch(timer->opt.type) {
                    case TIMER_TYPE_SOFT:
                        if(NULL == execute) { // Yay, we can execute this timer's handle
                            timer->ticks = timer->interval; // Reset tick count
                            timer->opt.timedout = false;
                            execute = timer->execute; 
                        }
                        break;
                    case TIMER_TYPE_SINGLE_SHOT:
                        if(NULL == execute) {
                            timer->opt.suspended = true;
                            execute = timer->execute;
                        } 
                        break;
                    case TIMER_TYPE_COUNTDOWN:
                        timer->opt.suspended = true;
                        break;
                    default:
                        break;
                }
            }
        }
        timer++; // Advance to next timer
    }
    
    if(NULL != execute)
        (*execute)(timer);
}

static void timer_ttask_configure(struct kernel_ttask_param* const param)
{
    kernel_ttask_set_priority(param, KERN_TTASK_PRIORITY_HIGH);
    kernel_ttask_set_interval(param, TIMER_TICK_INTERVAL, KERN_TIME_UNIT_US);
}