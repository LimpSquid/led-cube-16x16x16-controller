#include "../include/layer.h"
#include "../include/layer_config.h"
#include "../include/kernel_task.h"
#include "../include/tlc5940.h"
#include "../include/spi.h"
#include "../include/dma.h"
#include "../include/sys.h"
#include "../include/register.h"
#include "../include/toolbox.h"
#include <stddef.h>
#include <xc.h>

#if !defined(LAYER_REFRESH_INTERVAL)
    #error "Layer refresh interval is not specified, please define 'LAYER_REFRESH_INTERVAL'"
#endif

#define LAYER_NUM_OF_ROWS           16
#define LAYER_NUM_OF_COLS           16
#define LAYER_NUM_OF_LEDS           (LAYER_NUM_OF_ROWS * LAYER_NUM_OF_COLS)
#define LAYER_RED_OFFSET            (LAYER_NUM_OF_LEDS * 0)
#define LAYER_GREEN_OFFSET          (LAYER_NUM_OF_LEDS * 1)
#define LAYER_BLUE_OFFSET           (LAYER_NUM_OF_LEDS * 2)
#define LAYER_FRAME_DEPTH           3 // RGB
#define LAYER_FRAME_BUFFER_SIZE     (LAYER_NUM_OF_LEDS * LAYER_FRAME_DEPTH)
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

#define LAYER_SPI_CHANNEL               SPI_CHANNEL1
#define LAYER_SDI_PPS                   SDI1R
#define LAYER_SS_PPS                    SS1R

#define LAYER_SDI_TRIS                  TRISF
#define LAYER_SCK_TRIS                  TRISF
#define LAYER_SS_TRIS                   TRISB

#define LAYER_SS_ANSEL                  ANSELB 

#define LAYER_SDI_PPS_WORD              0xe
#define LAYER_SS_PPS_WORD               0x3
#define LAYER_SDI_PIN_MASK              BIT(2)
#define LAYER_SCK_PIN_MASK              BIT(6)
#define LAYER_SS_PIN_MASK               BIT(15)

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
    LAYER_RECEIVE_FRAME,
    LAYER_RECEIVE_FRAME_DMA_START,
    LAYER_RECEIVE_FRAME_DMA_WAIT,
};

static void layer_latch_callback(void);
static int layer_ttask_init(void);
static void layer_ttask_execute(void);
static void layer_ttask_configure(struct kernel_ttask_param* const param);
static int layer_rtask_init(void);
static void layer_rtask_execute(void);
KERN_TTASK(layer, layer_ttask_init, layer_ttask_execute, layer_ttask_configure, KERN_INIT_LATE);
KERN_QUICK_RTASK(layer, layer_rtask_init, layer_rtask_execute); // No init necessary

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

static const struct dma_config layer_dma_config; // No special config needed
static const struct spi_config layer_spi_config =
{
    .spicon_flags = SPI_SRXISEL_NOT_EMPTY | SPI_DISSDO | SPI_MODE8 | SPI_SSEN,
};

static unsigned char layer_front_buffer[LAYER_FRAME_BUFFER_SIZE];
static unsigned char layer_back_buffer[LAYER_FRAME_BUFFER_SIZE];
static unsigned char* layer_dma_ptr = layer_back_buffer;
static unsigned char* layer_draw_ptr = layer_front_buffer;
static const struct layer_io* layer_row_io = layer_io;
static const struct layer_io* layer_row_previous_io = &layer_io[LAYER_NUM_OF_ROWS - 1];
static struct dma_channel* layer_dma_channel = NULL;
static struct spi_module* layer_spi_module = NULL;
static enum layer_state layer_state = LAYER_IDLE;
static unsigned int layer_row_index = 0;
static unsigned int layer_red_index = 0;
static unsigned int layer_green_index = 0;
static unsigned int layer_blue_index = 0;
static unsigned int layer_offset = 0;

bool layer_busy(void)
{
    return layer_state != LAYER_IDLE;
}

bool layer_ready(void)
{
    return !layer_busy();
}

bool layer_receive_frame(void)
{
    if(layer_busy())
        return false;
    
    layer_state = LAYER_RECEIVE_FRAME;
    return true;
}

static void layer_latch_callback(void)
{
    atomic_reg_ptr_clr(layer_row_previous_io->lat, layer_row_previous_io->mask);
    atomic_reg_ptr_set(layer_row_io->lat, layer_row_io->mask);
    
    // Advance to next row
    layer_row_previous_io = layer_row_io;
    if(++layer_row_index >= LAYER_NUM_OF_ROWS) {
        layer_row_index = 0;
        layer_row_io = layer_io;
    } else
        layer_row_io++;
}

static int layer_ttask_init(void)
{
    const struct layer_io* io = NULL;
    
    // Initialize IO
    for(unsigned int i = 0; i < LAYER_NUM_OF_ROWS; ++i) {
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
    if(tlc5940_ready()) {
        layer_offset = layer_row_index * LAYER_NUM_OF_COLS;
        for(unsigned int i = 0; i < LAYER_NUM_OF_COLS; i++) {
            // Convert 8 bit to 12 bit equivalent
            layer_red_index = i + layer_offset + LAYER_RED_OFFSET;
            layer_green_index = i + layer_offset + LAYER_GREEN_OFFSET;
            layer_blue_index = i + layer_offset + LAYER_BLUE_OFFSET;
            
            tlc5940_write_grayscale(0, i, (layer_dma_ptr[layer_red_index] << 4) | (layer_dma_ptr[layer_red_index] >> 4));
            tlc5940_write_grayscale(1, i, (layer_dma_ptr[layer_green_index] << 4) | (layer_dma_ptr[layer_green_index] >> 4));
            tlc5940_write_grayscale(2, i, (layer_dma_ptr[layer_blue_index] << 4) | (layer_dma_ptr[layer_blue_index] >> 4));
        }
        tlc5940_update();
    }
}

static void layer_ttask_configure(struct kernel_ttask_param* const param)
{
    kernel_ttask_set_priority(param, KERN_TTASK_PRIORITY_HIGH);
    kernel_ttask_set_interval(param, LAYER_REFRESH_INTERVAL, KERN_TIME_UNIT_US);
}
 
static int layer_rtask_init(void)
{
    // Configure PPS
    sys_unlock();
    LAYER_SDI_PPS = LAYER_SDI_PPS_WORD;
    LAYER_SS_PPS = LAYER_SS_PPS_WORD;
    sys_lock();
    
    // Configure IO
    REG_CLR(LAYER_SS_ANSEL, LAYER_SS_PIN_MASK);
    
    REG_SET(LAYER_SDI_TRIS, LAYER_SDI_PIN_MASK);
    REG_SET(LAYER_SCK_TRIS, LAYER_SCK_PIN_MASK);
    REG_SET(LAYER_SS_TRIS, LAYER_SS_PIN_MASK);

    // Initialize DMA
    layer_dma_channel = dma_construct(layer_dma_config);
    if(NULL == layer_dma_channel)
        goto deinit_dma;
    
    // Initialize SPI
    layer_spi_module = spi_construct(LAYER_SPI_CHANNEL, layer_spi_config);
    if(NULL == layer_spi_module)
        goto deinit_spi;
    spi_configure_dma_src(layer_spi_module, layer_dma_channel); // SPI module is the source of the dma module
    spi_enable(layer_spi_module);
   
    return KERN_INIT_SUCCCES;
    
deinit_spi:
    spi_destruct(layer_spi_module);
deinit_dma:
    dma_destruct(layer_dma_channel);

    return KERN_INIT_FAILED;
}

static void layer_rtask_execute(void)
{
    switch(layer_state) {
        default:
        case LAYER_IDLE:
            break;
        case LAYER_RECEIVE_FRAME:
        case LAYER_RECEIVE_FRAME_DMA_START:
            // @Todo: add timeout timer
            if(dma_ready(layer_dma_channel)) {
                dma_configure_dst(layer_dma_channel, layer_dma_ptr, LAYER_FRAME_BUFFER_SIZE);
                dma_enable_transfer(layer_dma_channel);
                layer_state = LAYER_RECEIVE_FRAME_DMA_WAIT;
            }
            break;
        case LAYER_RECEIVE_FRAME_DMA_WAIT:
            if(dma_ready(layer_dma_channel))
                layer_state = LAYER_IDLE;
            break;
    }
}