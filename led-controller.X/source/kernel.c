#include "../include/kernel.h"
#include "../include/kernel_task.h"
#include "../include/kernel_config.h"
#include <xc.h>
#include <stddef.h>

#if defined(KERN_TMR_CLK_FREQ)
    #define SYSTEM_TICK ((1000000.0F / KERN_TMR_CLK_FREQ) * KERN_TMR_PRESCALER)
#else
    #error "System tick could not be calculated, please define the KERN_TMR_CLK_FREQ."
#endif

#define kernel_restore_rtask_iterator()                                 \
            kernel_rtask_iterator = &__kernel_rstack_begin
#define kernel_restore_ttask_iterator()                                 \
            kernel_ttask_iterator = &__kernel_tstack_begin
#define kernel_restore_ttask_sorted_iterator()                          \
            kernel_ttask_sorted_iterator = kernel_ttask_sorted_begin

#define kernel_rtask_size()                                             \
            (int)(&__kernel_rstack_end - &__kernel_rstack_begin)

#define kernel_ttask_size()                                             \
            (int)(&__kernel_tstack_end - &__kernel_tstack_begin)

static void kernel_init_configure_ttask(void);
static void kernel_init_configure_rtask(void);
static void kernel_init_ttask_call_sequence(void);
static void kernel_init_task_init(void);
inline static void __attribute__((always_inline)) kernel_execute_ttask_rtask(void);
inline static void __attribute__((always_inline)) kernel_execute_ttask(void);
inline static void __attribute__((always_inline)) kernel_execute_rtask(void);
inline static void __attribute__((always_inline)) kernel_execute_no_task(void);
static int kernel_compute_sys_ticks(int time, int unit);

extern const struct kernel_rtask __kernel_rstack_begin;
extern const struct kernel_rtask __kernel_rstack_end;
extern const struct kernel_ttask __kernel_tstack_begin;
extern const struct kernel_ttask __kernel_tstack_end;

static const struct kernel_rtask* kernel_rtask_iterator = &__kernel_rstack_begin;
static const struct kernel_rtask* const kernel_rtask_end = &__kernel_rstack_end;
static const struct kernel_ttask* kernel_ttask_iterator = &__kernel_tstack_begin;
static const struct kernel_ttask* const kernel_ttask_end = &__kernel_tstack_end;

static const struct kernel_ttask* kernel_ttask_sorted_iterator = NULL;
static const struct kernel_ttask* kernel_ttask_sorted_begin = NULL;
static const struct kernel_ttask* kernel_ttask_sorted_end = NULL;

static void (*kernel_exec_func)(void) = NULL;

static unsigned int elapsed_ticks = 0;
static unsigned int previous_ticks = 0;

void kernel_init(void)
{
    kernel_init_configure_ttask();
    kernel_init_configure_rtask();
    kernel_init_ttask_call_sequence();
    kernel_init_task_init();
    
    if(kernel_rtask_size() != 0 && kernel_ttask_size() != 0)
        kernel_exec_func = kernel_execute_ttask_rtask;
    else if(kernel_rtask_size() != 0)
        kernel_exec_func = kernel_execute_rtask;
    else if(kernel_ttask_size() != 0)
        kernel_exec_func = kernel_execute_ttask;
    else
        kernel_exec_func = kernel_execute_no_task;
    
    // Configure timer
    if(kernel_ttask_size() != 0) {
        KERN_TMR_CFG_REG &= ~KERN_TMR_EN_BIT;
        KERN_TMR_CFG_REG = KERN_TMR_CFG_WORD;
        KERN_TMR_CFG_REG |= KERN_TMR_EN_BIT;
    }
}

void kernel_execute(void)
{    
    (*kernel_exec_func)();
}

void kernel_ttask_set_priority(struct kernel_ttask_param* const ttask_param, int priority)
{
    if(NULL != ttask_param) {
        if(priority < 0 || priority >= __KERN_TTASK_PRIORITY_COUNT)
            priority = KERN_TTASK_PRIORITY_NORMAL;
        ttask_param->priority = priority;
    }
}

void kernel_ttask_set_interval(struct kernel_ttask_param* const ttask_param, int time, int unit)
{
    if(NULL != ttask_param)
        ttask_param->interval = kernel_compute_sys_ticks(time, unit);
}

static void kernel_init_configure_ttask(void)
{
    while(kernel_ttask_iterator != kernel_ttask_end) {
        if(NULL != kernel_ttask_iterator->configure)
            (*kernel_ttask_iterator->configure)(kernel_ttask_iterator->param);
        kernel_ttask_iterator++;
    }
    kernel_restore_ttask_iterator();
}

static void kernel_init_configure_rtask(void)
{
    while(kernel_rtask_iterator != kernel_rtask_end) {
        if(NULL != kernel_rtask_iterator->configure)
            (*kernel_rtask_iterator->configure)(kernel_rtask_iterator->param);
        kernel_rtask_iterator++;
    }
    kernel_restore_rtask_iterator();
}

static void kernel_init_ttask_call_sequence(void)
{
    int priority = KERN_TTASK_PRIORITY_HIGH;
    const struct kernel_ttask** ttask = NULL;
    
    do {
        while(kernel_ttask_iterator != kernel_ttask_end) {
            // Find task with current priority, and check if it is not assigned already
            if(kernel_ttask_iterator->param->priority == priority && NULL == kernel_ttask_iterator->param->next) {
                // Ignore the previously found ttask, because we still have to assign the next task 
                if(&kernel_ttask_iterator->param->next != ttask) {
                    // First ttask in call sequence
                    if(NULL == ttask)
                        kernel_ttask_sorted_begin = kernel_ttask_iterator;
                    else
                        *ttask = kernel_ttask_iterator;
                    ttask = &kernel_ttask_iterator->param->next;
                    kernel_ttask_sorted_end = kernel_ttask_iterator;
                    break;
                }
            }
            kernel_ttask_iterator++;
        }
        
        if(kernel_ttask_sorted_end != kernel_ttask_iterator)
            priority--;
        
        kernel_restore_ttask_iterator();
    } while(priority >= 0);
    
    // Create circular dependency
    if(NULL != ttask)
        *ttask = kernel_ttask_sorted_begin;
    kernel_restore_ttask_sorted_iterator();
}

static void kernel_init_task_init(void)
{
    int init_level = KERN_INIT_EARLY;
    do {
        int (*init)(void) = NULL;
        bool* init_done = NULL;
        
        // Find init function with current init_level in robin tasks
        while(kernel_rtask_iterator != kernel_rtask_end) {
            if(kernel_rtask_iterator->init_level == init_level && !kernel_rtask_iterator->param->init_done) {
                init = kernel_rtask_iterator->init;
                init_done = &kernel_rtask_iterator->param->init_done;
                break;
            }
            kernel_rtask_iterator++;
        }
        
        // Find init function with current init_level in timed tasks, these will get precedence
        while(kernel_ttask_iterator != kernel_ttask_end) {
            if(kernel_ttask_iterator->init_level == init_level && !kernel_ttask_iterator->param->init_done) {
                init = kernel_ttask_iterator->init;
                init_done = &kernel_ttask_iterator->param->init_done;
                break;
            }
            kernel_ttask_iterator++;
        }
        
        // Check if we have found a init function to process...
        if(NULL != init_done) {
            if(NULL != init)
                (*init)();
            *init_done = true;
        } else // If not, we go to the next init_level
            init_level--;
        
        kernel_restore_rtask_iterator();
        kernel_restore_ttask_iterator();
    } while(init_level >= 0);
}

static void kernel_execute_ttask_rtask(void)
{
    struct kernel_ttask_param* param;
    
    elapsed_ticks = KERN_TMR_REG - previous_ticks;
    previous_ticks += elapsed_ticks;
    
    do {
        param = kernel_ttask_sorted_iterator->param;
        if(param->ticks <= elapsed_ticks) {
            param->ticks = param->interval;
            kernel_ttask_sorted_iterator->exec();
        } else
            param->ticks -= elapsed_ticks;
        
        kernel_ttask_sorted_iterator = param->next;
    } while(kernel_ttask_sorted_iterator != kernel_ttask_sorted_begin);
    kernel_restore_ttask_sorted_iterator();
    
    kernel_rtask_iterator->exec();
    if(++kernel_rtask_iterator == kernel_rtask_end)
        kernel_restore_rtask_iterator();
}

static void kernel_execute_ttask(void)
{
    struct kernel_ttask_param* param;
    
    elapsed_ticks = KERN_TMR_REG - previous_ticks;
    previous_ticks += elapsed_ticks;
    
    do {
        param = kernel_ttask_sorted_iterator->param;
        if(param->ticks <= elapsed_ticks) {
            param->ticks = param->interval;
            kernel_ttask_sorted_iterator->exec();
        } else
            param->ticks -= elapsed_ticks;
        
        kernel_ttask_sorted_iterator = param->next;
    } while(kernel_ttask_sorted_iterator != kernel_ttask_sorted_begin);
    kernel_restore_ttask_sorted_iterator();
}

static void kernel_execute_rtask(void)
{
    kernel_rtask_iterator->exec();
    if(++kernel_rtask_iterator == kernel_rtask_end)
        kernel_restore_rtask_iterator();
}

static void kernel_execute_no_task(void)
{
    Nop();
}

static int kernel_compute_sys_ticks(int time, int unit)
{
    int ticks;
    switch(unit) {
        default: // Default to seconds
        case KERN_TIME_UNIT_S:  ticks = (time * 1000000LU) / SYSTEM_TICK;   break;
        case KERN_TIME_UNIT_MS: ticks = (time * 1000LU) / SYSTEM_TICK;      break;
        case KERN_TIME_UNIT_US: ticks = time / SYSTEM_TICK;                 break;
    }
    return ticks;
}