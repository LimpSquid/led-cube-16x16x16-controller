#include "../include/tlc5940.h"
#include "../include/tlc5940_config.h"
#include "../include/spi.h"
#include "../include/dma.h"
#include "../include/sys.h"
#include "../include/kernel_task.h"
#include <stddef.h>
#include <string.h>

#define TLC5940_SET_REG(reg, mask)      (reg |= mask)
#define TLC5940_CLR_REG(reg, mask)      (reg &= ~mask)

#define TLC5940_BYTES_PER_DEVICE        24
#define TLC5940_GRAYSCALE_BUFFER_SIZE   (TLC5940_BYTES_PER_DEVICE * TLC5940_NUM_OF_DEVICES)

// @Todo: change to actual channel and pins
#define SPI_CHANNEL                     SC_CHANNEL1
#define SPI_SDO_PPS                     RPF3R
#define SPI_SDO_TRIS                    TRISF
#define SPI_SCK_TRIS                    TRISF
#define SPI_SDO_PPS_WORD                0x00000008
#define SPI_SDO_TRIS_MASK               0x00000008
#define SPI_SCK_TRIS_MASK               0x00000040

enum tlc5940_state
{
    TLC5940_INIT = 0,
    TLC5940_IDLE,
    TLC5940_UPDATE,
    TLC5940_UPDATE_DMA_START,
    TLC5940_UPDATE_DMA_WAIT,
    TLC5940_UPDATE_CLEAR_BUFFER,
};

static int tlc5940_rtask_init(void);
static void tlc5940_rtask_execute(void);
KERN_QUICK_RTASK(tlc5940, tlc5940_rtask_init, tlc5940_rtask_execute);

static unsigned char tlc5940_grayscale_buffer[TLC5940_GRAYSCALE_BUFFER_SIZE]; // Must be made available before the DMA config

static const struct dma_config tlc5940_dma_config =
{
    .block_transfer_complete = NULL, // For now we just poll the DMA channel
    .src_mem = tlc5940_grayscale_buffer,
    .src_size = TLC5940_GRAYSCALE_BUFFER_SIZE,
};

static const struct spi_config tlc5940_spi_config =
{
    .spicon_flags = SF_MSTEN | SF_STXISEL_COMPLETE | SF_DISSDI | SF_MODE8 | SF_CKP,
    .baudrate = 4000000LU, // @Todo: increase to 40000000LU
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
    
    tlc5940_state = TLC5940_UPDATE;
    return true;
}

static int tlc5940_rtask_init(void)
{
    // Initialize IO
    sys_unlock();
    SPI_SDO_PPS = SPI_SDO_PPS_WORD;
    TLC5940_SET_REG(SPI_SDO_TRIS, SPI_SDO_TRIS_MASK);
    TLC5940_SET_REG(SPI_SCK_TRIS, SPI_SCK_TRIS_MASK);
    sys_lock();
        
    // Initialize DMA
    tlc5940_dma_channel = dma_construct(tlc5940_dma_config);
    if(NULL == tlc5940_dma_channel)
        goto deinit_dma;
    
    // Initialize SPI
    tlc5940_spi_module = spi_construct(SPI_CHANNEL, tlc5940_spi_config);
    if(NULL == tlc5940_spi_module)
        goto deinit_spi;
    spi_configure_dma_dst(tlc5940_spi_module, tlc5940_dma_channel); // SPI module is the destination of the dma module
    spi_enable(tlc5940_spi_module);
    
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
            tlc5940_state = TLC5940_IDLE;
            break;
        default:
        case TLC5940_IDLE:
            break;
        case TLC5940_UPDATE:
        case TLC5940_UPDATE_DMA_START:
            if(dma_ready(tlc5940_dma_channel)) {
                dma_enable_transfer(tlc5940_dma_channel);
                tlc5940_state = TLC5940_UPDATE_DMA_WAIT;
            }
            break;
        case TLC5940_UPDATE_DMA_WAIT:
            if(dma_ready(tlc5940_dma_channel))
                tlc5940_state = TLC5940_UPDATE_CLEAR_BUFFER;
            break;
        case TLC5940_UPDATE_CLEAR_BUFFER:
            memset(tlc5940_grayscale_buffer, 0x00, TLC5940_GRAYSCALE_BUFFER_SIZE);
            tlc5940_state = TLC5940_IDLE;
            break;
    }
}