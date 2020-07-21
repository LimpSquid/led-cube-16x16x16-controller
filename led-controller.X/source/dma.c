#include "../include/dma.h"
#include <stdint.h>
#include <stdbool.h>
#include <xc.h>

#define DMA_SET_REG(reg, mask)      (reg |= mask)
#define DMA_CLR_REG(reg, mask)      (reg &= ~mask)

#define DMA_BASE_ADDR               0xbf883060
#define DMA_CHANNEL0                0x00000000
#define DMA_CHANNEL1                0x000000c0
#define DMA_CHANNEL2                0x00000180
#define DMA_CHANNEL3                0x00000240

#define DMA_NUMBER_OF_CHANNELS      (sizeof(dma_channels) / sizeof(dma_channels[0]))

#define DMA_INTERRUPT_REG           
#define DMA_DMACON_REG              DMACON
#define DMA_DMASTAT_REG             DMASTAT
#define DMA_DMAADDR_REG             DMAADDR

#define DMA_DMACON_WORD             0x0000
#define DMA_DMASTAT_WORD            0x0000
#define DMA_DMAADDR_WORD            0x0000

#define DMA_ENABLE_MASK             0x8000

struct dma_register_map
{
    volatile uint32_t dchcon;
    volatile uint32_t dchecon;
    volatile uint32_t dchint;
    volatile uint32_t dchssa;
    volatile uint32_t dchdsa;
    volatile uint32_t dchssiz;
    volatile uint32_t dchdsiz;
    volatile uint32_t dchsptr;
    volatile uint32_t dchdptr;
    volatile uint32_t dchcsiz;
    volatile uint32_t dchcptr;
    volatile uint32_t dchdat;
};

struct dma_interrupt_map
{
    volatile uint32_t* const ifs;
    volatile uint32_t* const iec;
    volatile uint32_t* const ipc;
    
    const uint32_t flag_mask;
    const uint32_t enable_mask;
    const uint32_t priority_mask;
    const uint32_t sub_priority_mask;
};

struct dma_channel
{
    struct dma_register_map* const dma_reg;
    struct dma_interrupt_map* const dma_int;
    
    bool assigned;
};

struct dma_interrupt_map dma_channel_interrupts[] =
{
    { 
        .ifs = &IFS2,
        .iec = &IEC2,
        .ipc = &IPC10,
        .flag_mask = 0x00000100,
        .enable_mask = 0x00000100,
        .priority_mask = 0x001c0000,
        .sub_priority_mask = 0x00030000 
     },
     { 
        .ifs = &IFS2,
        .iec = &IEC2, 
        .ipc = &IPC10,
        .flag_mask = 0x00000200,
        .enable_mask = 0x00000200,
        .priority_mask = 0x1c000200,
        .sub_priority_mask = 0x03000000
     },
     { 
        .ifs = &IFS2,
        .iec = &IEC2,
        .ipc = &IPC11,
        .flag_mask = 0x00000400,
        .enable_mask = 0x00000400,
        .priority_mask = 0x0000001c,
        .sub_priority_mask = 0x00000003
     },
     { 
        .ifs = &IFS2,
        .iec = &IEC2,
        .ipc = &IPC11,
        .flag_mask = 0x00000800,
        .enable_mask = 0x00000800,
        .priority_mask = 0x00001c00,
        .sub_priority_mask = 0x00000300
     },
};

struct dma_channel dma_channels[] =
{
    {
        .dma_reg = (struct dma_register_map* const) (DMA_BASE_ADDR + DMA_CHANNEL0),
        .dma_int = &dma_channel_interrupts[0],
        .assigned = false
    }, 
    {
        .dma_reg = (struct dma_register_map* const) (DMA_BASE_ADDR + DMA_CHANNEL1),
        .dma_int = &dma_channel_interrupts[1],
        .assigned = false
    },
    {
        .dma_reg = (struct dma_register_map* const) (DMA_BASE_ADDR + DMA_CHANNEL2),
        .dma_int = &dma_channel_interrupts[2],
        .assigned = false
    },
    {
        .dma_reg = (struct dma_register_map* const) (DMA_BASE_ADDR + DMA_CHANNEL3),
        .dma_int = &dma_channel_interrupts[3],
        .assigned = false
    }
};

void dma_init(void)
{
    // Disable DMA modules
    DMA_CLR_REG(DMA_DMACON_REG, DMA_ENABLE_MASK);
    
    // Disable interrupt on each channel
    for(int i = 0; i < DMA_NUMBER_OF_CHANNELS; ++i) {
        volatile uint32_t* reg = dma_channels[i].dma_int->iec;
        uint32_t mask = dma_channels[i].dma_int->enable_mask;
        DMA_CLR_REG(*reg, mask);
    }
    
    // Configure DMA
    DMA_DMACON_REG = DMA_DMACON_WORD;
    
    // Enable DMA module
    DMA_SET_REG(DMA_DMACON_REG, DMA_DMACON_WORD);
}