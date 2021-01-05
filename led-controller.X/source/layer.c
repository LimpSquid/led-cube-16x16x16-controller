#include "../include/layer.h"
#include "../include/layer_config.h"
#include "../include/kernel_task.h"
#include "../include/tlc5940.h"
#include "../include/register.h"
#include "../include/toolbox.h"
#include <stddef.h>
#include <xc.h>

#ifndef LAYER_REFRESH_INTERVAL
    #error "Layer refresh interval is not specified, please define 'LAYER_REFRESH_INTERVAL'"
#endif

#define LAYER_NUM_OF_ROWS           16
#define LAYER_IO(pin, bank) \
    { \
        .ansel = NULL, \
        .tris = atomic_reg_ptr_cast(&TRIS##bank), \
        .lat = atomic_reg_ptr_cast(&LAT##bank), \
        .mask = BIT(pin) \
    }
#define LAYER_ANSEL_IO(pin, bank) \
    { \
        .ansel = atomic_reg_ptr_cast(&ANSEL##bank), \
        .tris = atomic_reg_ptr_cast(&TRIS##bank), \
        .lat = atomic_reg_ptr_cast(&LAT##bank), \
        .mask = BIT(pin) \
    }
   
struct layer_io
{
    atomic_reg_ptr(ansel);
    atomic_reg_ptr(tris);
    atomic_reg_ptr(lat);
    
    unsigned int mask;
};

enum layer_state
{
    LAYER_IDLE = 0,
};

static void layer_latch_callback(void);
static int layer_ttask_init(void);
static void layer_ttask_execute(void);
static void layer_ttask_configure(struct kernel_ttask_param* const param);
static void layer_rtask_execute(void);
KERN_TTASK(layer, layer_ttask_init, layer_ttask_execute, layer_ttask_configure, KERN_INIT_LATE);
KERN_QUICK_RTASK(layer, NULL, layer_rtask_execute); // No init necessary

static const struct layer_io layer_io[LAYER_NUM_OF_ROWS] =
{
    LAYER_IO(7, D), // Row 0
    LAYER_IO(6, D), // Row 1
    LAYER_IO(5, D), // ...
    LAYER_IO(4, D),
    LAYER_ANSEL_IO(3, D),
    LAYER_ANSEL_IO(2, D),
    LAYER_ANSEL_IO(1, D),
    LAYER_IO(0, D),
    LAYER_IO(3, E),
    LAYER_ANSEL_IO(2, E),
    LAYER_IO(1, E),
    LAYER_IO(0, E),
    LAYER_IO(11, D),
    LAYER_IO(10, D),
    LAYER_IO(9, D),
    LAYER_IO(8, D),
};

static const struct layer_io* layer_row_io = layer_io;
static const struct layer_io* layer_row_previous_io = &layer_io[LAYER_NUM_OF_ROWS - 1];
static enum layer_state layer_state = LAYER_IDLE;
static unsigned int layer_row_index = 0;

static void layer_latch_callback(void)
{
    atomic_reg_ptr_clr(layer_row_previous_io->lat, layer_row_previous_io->mask);
    atomic_reg_ptr_set(layer_row_io->lat, layer_row_io->mask);
    
    // Advance to next row
    layer_row_previous_io = layer_row_io;
    if(layer_row_index++ >= LAYER_NUM_OF_ROWS) {
        layer_row_index = 0;
        layer_row_io = layer_io;
    } else
        layer_row_io++;
}

static int layer_ttask_init(void)
{
    const struct layer_io* io = NULL;
    
    // Initialize IO
    for(int i = 0; i < LAYER_NUM_OF_ROWS; ++i) {
        io = &layer_io[i];
        if(NULL != io->ansel)
            atomic_reg_ptr_clr(io->ansel, io->mask);
        atomic_reg_ptr_clr(io->tris, io->mask);
        atomic_reg_ptr_clr(io->lat, io->mask);
    }
    
    // Initialize TLC5940
    tlc5940_set_latch_callback(layer_latch_callback);
    
    return KERN_INIT_SUCCCES;
}

static void layer_ttask_execute(void)
{
    if(tlc5940_ready())
        tlc5940_update();
}

static void layer_ttask_configure(struct kernel_ttask_param* const param)
{
    kernel_ttask_set_priority(param, KERN_TTASK_PRIORITY_HIGH);
    kernel_ttask_set_interval(param, LAYER_REFRESH_INTERVAL, KERN_TIME_UNIT_US);
}
 
static void layer_rtask_execute(void)
{
    switch(layer_state) {
        default:
        case LAYER_IDLE:
            break;
    }
}