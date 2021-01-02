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

enum work_state
{
    WS_INIT = 0,
    WS_IDLE,
    WS_UPDATE,
    WS_UPDATE_DMA_START,
    WS_UPDATE_DMA_WAIT,
    WS_UPDATE_CLEAR_BUFFER,
};

static int rtask_init(void);
static void rtask_execute(void);
KERN_QUICK_RTASK(tlc5940, rtask_init, rtask_execute);

static unsigned char grayscale_buffer[TLC5940_GRAYSCALE_BUFFER_SIZE]; // Must be made available before the DMA config

static const struct dma_config dma_config =
{
    .block_transfer_complete = NULL, // For now we just poll the DMA channel
    .src_mem = grayscale_buffer,
    .src_size = TLC5940_GRAYSCALE_BUFFER_SIZE,
};

static const struct spi_config spi_config =
{
    .spicon_flags = SF_MSTEN | SF_STXISEL_COMPLETE | SF_DISSDI | SF_MODE8 | SF_CKP,
    .baudrate = 4000000LU, // @Todo: increase to 40000000LU
};

static struct dma_channel* dma_channel = NULL;
static struct spi_module* spi_module = NULL;
static enum work_state work_state = WS_INIT;

bool tlc5940_busy(void)
{
    return work_state != WS_IDLE;
}

bool tlc5940_ready(void)
{
    return !tlc5940_busy();
}

bool tlc5940_update(void)
{
    if(tlc5940_busy())
        return false;
    
    work_state = WS_UPDATE;
    return true;
}

static int rtask_init(void)
{
    // Initialize IO
    sys_unlock();
    SPI_SDO_PPS = SPI_SDO_PPS_WORD;
    TLC5940_SET_REG(SPI_SDO_TRIS, SPI_SDO_TRIS_MASK);
    TLC5940_SET_REG(SPI_SCK_TRIS, SPI_SCK_TRIS_MASK);
    sys_lock();
        
    // Initialize DMA
    dma_channel = dma_construct(dma_config);
    if(NULL == dma_channel)
        goto deinit_dma;
    
    // Initialize SPI
    spi_module = spi_construct(SPI_CHANNEL, spi_config);
    if(NULL == spi_module)
        goto deinit_spi;
    spi_configure_dma_dst(spi_module, dma_channel); // SPI module is the destination of the dma module
    spi_enable(spi_module);
    
    return KERN_INIT_SUCCCES;
    
deinit_spi:
    spi_destruct(spi_module);
deinit_dma:
    dma_destruct(dma_channel);

    return KERN_INIT_FAILED;
}

static void rtask_execute(void)
{
    switch(work_state) {
        case WS_INIT:
            // @Todo: do some init stuff
            work_state = WS_IDLE;
            break;
        default:
        case WS_IDLE:
            break;
        case WS_UPDATE:
        case WS_UPDATE_DMA_START:
            if(dma_ready(dma_channel)) {
                dma_enable_transfer(dma_channel);
                work_state = WS_UPDATE_DMA_WAIT;
            }
            break;
        case WS_UPDATE_DMA_WAIT:
            if(dma_ready(dma_channel))
                work_state = WS_UPDATE_CLEAR_BUFFER;
            break;
        case WS_UPDATE_CLEAR_BUFFER:
            memset(grayscale_buffer, 0x00, TLC5940_GRAYSCALE_BUFFER_SIZE);
            work_state = WS_IDLE;
            break;
    }
}