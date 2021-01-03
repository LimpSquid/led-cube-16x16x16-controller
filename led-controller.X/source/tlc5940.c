#include "../include/tlc5940.h"
#include "../include/tlc5940_config.h"
#include "../include/spi.h"
#include "../include/dma.h"
#include "../include/pwm.h"
#include "../include/sys.h"
#include "../include/kernel_task.h"
#include <stddef.h>
#include <string.h>

#define TLC5940_REG_SET(reg, mask)      (reg |= mask)
#define TLC5940_REG_CLR(reg, mask)      (reg &= ~mask)
#define TLC5940_REG_INV(reg, mask)      (reg ^= mask)

#define TLC5940_BYTES_PER_DEVICE        24
#define TLC5940_BUFFER_SIZE             (TLC5940_BYTES_PER_DEVICE * TLC5940_NUM_OF_DEVICES)

// @Todo: change to actual SPI channel and pins
#define TLC5940_SPI_CHANNEL             SPI_CHANNEL2
#define TLC5940_SDO_PPS                 RPG7R

#define TLC5940_SDO_TRIS                TRISG
#define TLC5940_SCK_TRIS                TRISG
#define TLC5940_BLANK_TRIS              TRISE
#define TLC5940_XLAT_TRIS               TRISE
#define TLC5940_VPRG_TRIS               TRISG
#define TLC5940_DCPRG_TRIS              TRISB

#define TLC5940_SDO_LAT                 LATG
#define TLC5940_SCK_LAT                 LATG
#define TLC5940_BLANK_LAT               LATE
#define TLC5940_XLAT_LAT                LATE
#define TLC5940_VPRG_LAT                LATG
#define TLC5940_DCPRG_LAT               LATB

#define TLC5940_SDO_ANSEL               ANSELG 
#define TLC5940_SCK_ANSEL               ANSELG
#define TLC5940_BLANK_ANSEL             ANSELE
#define TLC5940_XLAT_ANSEL              ANSELE
#define TLC5940_VPRG_ANSEL              ANSELG
#define TLC5940_DCPRG_ANSEL             ANSELB

#define TLC5940_SDO_PPS_WORD            0x00000006
#define TLC5940_SDO_PIN_MASK            0x00000080
#define TLC5940_SCK_PIN_MASK            0x00000040
#define TLC5940_BLANK_PIN_MASK          0x00000080
#define TLC5940_XLAT_PIN_MASK           0x00000040
#define TLC5940_VPRG_PIN_MASK           0x00000200
#define TLC5940_DCPRG_PIN_MASK          0x00000020

enum tlc5940_state
{
    TLC5940_INIT = 0,
    TLC5940_IDLE,
    TLC5940_UPDATE,
    TLC5940_UPDATE_DMA_START,
    TLC5940_UPDATE_DMA_WAIT,
    TLC5940_UPDATE_LATCH,
    TLC5940_UPDATE_CLEAR_BUFFER,
};

static int tlc5940_rtask_init(void);
static void tlc5940_rtask_execute(void);
KERN_QUICK_RTASK(tlc5940, tlc5940_rtask_init, tlc5940_rtask_execute);

static unsigned char tlc5940_front_buffer[TLC5940_BUFFER_SIZE]; // Must be made available before the DMA config
static unsigned char tlc5940_back_buffer[TLC5940_BUFFER_SIZE];
static unsigned char* tlc5940_frame_ptr = tlc5940_back_buffer;
static unsigned char* tlc5940_draw_ptr = tlc5940_front_buffer;

static const struct dma_config tlc5940_dma_config =
{
    .block_transfer_complete = NULL, // For now we just poll the DMA channel
};

static const struct spi_config tlc5940_spi_config =
{
    .spicon_flags = SPI_MSTEN | SPI_STXISEL_COMPLETE | SPI_DISSDI | SPI_MODE8 | SPI_CKP,
    .baudrate = 4000000, // @Todo: increase to 40000000
};

static const struct pwm_config tlc5940_pwm_config =
{
    .duty = 0.5,
    .frequency = 4000000, // @Todo: increase to 25MHz ish
    .cycle_count = 4096
};

static struct dma_channel* tlc5940_dma_channel = NULL;
static struct spi_module* tlc5940_spi_module = NULL;
static enum tlc5940_state tlc5940_state = TLC5940_INIT;

bool tlc5940_busy(void)
{
    return tlc5940_state != TLC5940_IDLE;
}

bool tlc5940_ready(void)
{
    return !tlc5940_busy();
}

bool tlc5940_update(void)
{
    if(tlc5940_busy())
        return false;
    
    unsigned char *frame_ptr = tlc5940_frame_ptr;
    tlc5940_frame_ptr = tlc5940_draw_ptr;
    tlc5940_draw_ptr = frame_ptr;
    tlc5940_state = TLC5940_UPDATE;
    return true;
}

static int tlc5940_rtask_init(void)
{
    // Configure PPS
    sys_unlock();
    TLC5940_SDO_PPS = TLC5940_SDO_PPS_WORD;
    sys_lock();
    
    // Configure IO
    TLC5940_REG_CLR(TLC5940_SDO_ANSEL, TLC5940_SDO_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_SCK_ANSEL, TLC5940_SCK_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_BLANK_ANSEL, TLC5940_BLANK_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_XLAT_ANSEL, TLC5940_XLAT_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_VPRG_ANSEL, TLC5940_VPRG_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_DCPRG_ANSEL, TLC5940_DCPRG_PIN_MASK);
    
    TLC5940_REG_CLR(TLC5940_SDO_TRIS, TLC5940_SDO_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_SCK_TRIS, TLC5940_SCK_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_BLANK_TRIS, TLC5940_BLANK_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_XLAT_TRIS, TLC5940_XLAT_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_VPRG_TRIS, TLC5940_VPRG_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_DCPRG_TRIS, TLC5940_DCPRG_PIN_MASK);
    
    // Define output states
    TLC5940_REG_CLR(TLC5940_BLANK_LAT, TLC5940_BLANK_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_XLAT_LAT, TLC5940_XLAT_PIN_MASK);
    TLC5940_REG_CLR(TLC5940_VPRG_LAT, TLC5940_VPRG_PIN_MASK);
    TLC5940_REG_SET(TLC5940_DCPRG_LAT, TLC5940_DCPRG_PIN_MASK);
   
    // Initialize DMA
    tlc5940_dma_channel = dma_construct(tlc5940_dma_config);
    if(NULL == tlc5940_dma_channel)
        goto deinit_dma;
    
    // Initialize SPI
    tlc5940_spi_module = spi_construct(TLC5940_SPI_CHANNEL, tlc5940_spi_config);
    if(NULL == tlc5940_spi_module)
        goto deinit_spi;
    spi_configure_dma_dst(tlc5940_spi_module, tlc5940_dma_channel); // SPI module is the destination of the dma module
    spi_enable(tlc5940_spi_module);
    
    // Initialize PWM
    pwm_configure(tlc5940_pwm_config);
    pwm_enable();
    
    return KERN_INIT_SUCCCES;
    
deinit_spi:
    spi_destruct(tlc5940_spi_module);
deinit_dma:
    dma_destruct(tlc5940_dma_channel);

    return KERN_INIT_FAILED;
}

static void tlc5940_rtask_execute(void)
{
    switch(tlc5940_state) {
        case TLC5940_INIT:
            // @Todo: do some init stuff
            //tlc5940_state = TLC5940_IDLE; // @Commit: uncomment
            
            // @Commit: this is temporary
            memset(tlc5940_frame_ptr, 0xff, TLC5940_BUFFER_SIZE);
            tlc5940_state = TLC5940_UPDATE;
            break;
        default:
        case TLC5940_IDLE:
            break;
        case TLC5940_UPDATE:
        case TLC5940_UPDATE_DMA_START:
            if(dma_ready(tlc5940_dma_channel)) {
                dma_configure_src(tlc5940_dma_channel, tlc5940_frame_ptr, TLC5940_BUFFER_SIZE);
                dma_enable_transfer(tlc5940_dma_channel);
                tlc5940_state = TLC5940_UPDATE_DMA_WAIT;
            }
            break;
        case TLC5940_UPDATE_DMA_WAIT:
            if(dma_ready(tlc5940_dma_channel))
                tlc5940_state = TLC5940_UPDATE_LATCH;
            break;
        case TLC5940_UPDATE_LATCH:
            // Clear outputs
            TLC5940_REG_SET(TLC5940_BLANK_LAT, TLC5940_BLANK_PIN_MASK);
            
            // Latch in data
            TLC5940_REG_SET(TLC5940_XLAT_LAT, TLC5940_XLAT_PIN_MASK);
            TLC5940_REG_CLR(TLC5940_XLAT_LAT, TLC5940_XLAT_PIN_MASK);
            
            // Shift diagnostic data
            spi_disable(tlc5940_spi_module);
            TLC5940_REG_INV(TLC5940_SCK_LAT, TLC5940_SCK_PIN_MASK);
            TLC5940_REG_INV(TLC5940_SCK_LAT, TLC5940_SCK_PIN_MASK);
            spi_enable(tlc5940_spi_module);
            
            // Enable outputs
            TLC5940_REG_CLR(TLC5940_BLANK_LAT, TLC5940_BLANK_PIN_MASK);
            tlc5940_state = TLC5940_UPDATE_CLEAR_BUFFER;
            break;
        case TLC5940_UPDATE_CLEAR_BUFFER:
            memset(tlc5940_frame_ptr, 0x00, TLC5940_BUFFER_SIZE);
            tlc5940_state = TLC5940_IDLE;
            break;
    }
}