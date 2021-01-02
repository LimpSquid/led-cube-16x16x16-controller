#ifndef KERNEL_TASK_H
#define	KERNEL_TASK_H

#include <stdbool.h>

#define KERN_RTASK(name, init_func, exec_func, config_func, level)              \
            static struct kernel_rtask_param                                    \
            __attribute__ ((__used__)) __rtask_param_##name =                   \
            {                                                                   \
                .init_done = false                                              \
            };                                                                  \
            static const struct kernel_rtask __rtask_##name                     \
            __attribute__ ((section(".kernel_rstack"), __used__)) =             \
            {                                                                   \
                .init = init_func,                                              \
                .exec = exec_func,                                              \
                .configure = config_func,                                       \
                .param = &__rtask_param_##name,                                 \
                .init_level = level                                             \
            };

#define KERN_QUICK_RTASK(name, init_func, exec_func)                            \
            KERN_RTASK(name, init_func, exec_func, ((void*)0), KERN_INIT_LATE)

#define KERN_TTASK(name, init_func, exec_func, config_func, level)              \
            static struct kernel_ttask_param                                    \
            __attribute__ ((__used__)) __ttask_param_##name =                   \
            {                                                                   \
                .init_done = false,                                             \
                .priority = KERN_TTASK_PRIORITY_NORMAL,                         \
                .ticks = 0,                                                     \
                .interval = 0x7fffffffL,                                        \
                .next = ((void*)0)                                              \
            };                                                                  \
            static const struct kernel_ttask __ttask_##name                     \
                __attribute__ ((section(".kernel_tstack"), __used__)) =         \
            {                                                                   \
                .init = init_func,                                              \
                .exec = exec_func,                                              \
                .configure = config_func,                                       \
                .param = &__ttask_param_##name,                                 \
                .init_level = level                                             \
            };

#define KERN_QUICK_TTASK(name, init_func, exec_func)                            \
            KERN_TTASK(name, init_func, exec_func, ((void*)0), KERN_INIT_LATE)

enum 
{
    KERN_INIT_LATE = 0,
    KERN_INIT_CORE,
    KERN_INIT_EARLY,
    
    __KERN_INIT_COUNT
};

/* @Todo: implement */
enum
{
    KERN_INIT_SUCCCES = 0,
    KERN_INIT_FAILED,
};

enum 
{
    KERN_TTASK_PRIORITY_LOW = 0,
    KERN_TTASK_PRIORITY_NORMAL,
    KERN_TTASK_PRIORITY_HIGH,
    
    __KERN_TTASK_PRIORITY_COUNT
};

enum
{
    KERN_TIME_UNIT_S = 0,
    KERN_TIME_UNIT_MS,
    KERN_TIME_UNIT_US
};

struct kernel_rtask_param
{
    bool init_done;
};

struct kernel_rtask
{
    int (*init)(void);
    void (*exec)(void);
    void (*configure)(struct kernel_rtask_param* const);

    struct kernel_rtask_param* const param;
    const int init_level;
};

struct kernel_ttask_param
{
    bool init_done;
    int priority;
    int ticks;
    int interval;
    
    const struct kernel_ttask* next;
};

struct kernel_ttask
{
    int (*init)(void);
    void (*exec)(void); 
    void (*configure)(struct kernel_ttask_param* const);
    
    struct kernel_ttask_param* const param;
    const int init_level;
};

void kernel_ttask_set_priority(struct kernel_ttask_param* const ttask_param, int priority);
void kernel_ttask_set_interval(struct kernel_ttask_param* const ttask_param, int time, int unit);

#endif	/* KERNEL_TASK_H */

