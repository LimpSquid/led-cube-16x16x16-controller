#include "../include/dma.h"
#include "../include/assert.h"
#include "../include/register.h"
#include <xc.h>

#define DMA_SET_REG(reg, mask)      (reg |= mask)
#define DMA_CLR_REG(reg, mask)      (reg &= ~mask)
#define DMA_PHY_ADDR(virt)          ((int)virt < 0 ? ((int)virt & 0x1FFFFFFFL) : (unsigned int)((unsigned char*)virt + 0x40000000L))

#define DMA_NUMBER_OF_CHANNELS      (sizeof(dma_channels) / sizeof(dma_channels[0]))

#define DMA_INTERRUPT_REG           
#define DMA_DMACON_REG              DMACON
#define DMA_DMASTAT_REG             DMASTAT
#define DMA_DMAADDR_REG             DMAADDR

#define DMA_DMACON_WORD             0x00000000
#define DMA_DMASTAT_WORD            0x00000000
#define DMA_DMAADDR_WORD            0x00000000

#define DMA_ENABLE_MASK             0x00008000
#define DCHCON_CHEN_MASK            0x00000080
#define DCHECON_CHAIRQ_MASK         0x00ff0000
#define DCHECON_CHSIRQ_MASK         0x0000ff00
#define DCHECON_AIRQEN_MASK         0x00000008
#define DCHECON_SIRQEN_MASK         0x00000010
#define DCHINT_CHBCIE_MASK          0x00080000
#define DCHINT_INTERRUPTS_MASK      0xffff0000

#define DCHECON_CHAIRQ_SHIFT        16
#define DCHECON_CHSIRQ_SHIFT        8

struct dma_register_map
{
    atomic_reg(dchcon);
    atomic_reg(dchecon);
    atomic_reg(dchint);
    atomic_reg(dchssa);
    atomic_reg(dchdsa);
    atomic_reg(dchssiz);
    atomic_reg(dchdsiz);
    atomic_reg(dchsptr);
    atomic_reg(dchdptr);
    atomic_reg(dchcsiz);
    atomic_reg(dchcptr);
    atomic_reg(dchdat);
};

struct dma_interrupt_map
{
    atomic_reg_ptr(ifs);
    atomic_reg_ptr(iec);
    atomic_reg_ptr(ipc);
    
    const uint32_t flag_mask;
    const uint32_t enable_mask;
};

struct dma_channel
{
    const struct dma_register_map* const dma_reg;
    const struct dma_interrupt_map* const dma_int;
    
    bool assigned;
};

static const struct dma_interrupt_map dma_channel_interrupts[] =
{
    { 
        .ifs = atomic_reg_ptr_cast(&IFS2),
        .iec = atomic_reg_ptr_cast(&IEC2),
        .ipc = atomic_reg_ptr_cast(&IPC10),
        .flag_mask = 0x00000100,
        .enable_mask = 0x00000100,
     },
     { 
        .ifs = atomic_reg_ptr_cast(&IFS2),
        .iec = atomic_reg_ptr_cast(&IEC2), 
        .ipc = atomic_reg_ptr_cast(&IPC10),
        .flag_mask = 0x00000200,
        .enable_mask = 0x00000200,
     },
     { 
        .ifs = atomic_reg_ptr_cast(&IFS2),
        .iec = atomic_reg_ptr_cast(&IEC2),
        .ipc = atomic_reg_ptr_cast(&IPC11),
        .flag_mask = 0x00000400,
        .enable_mask = 0x00000400,
     },
     { 
        .ifs = atomic_reg_ptr_cast(&IFS2),
        .iec = atomic_reg_ptr_cast(&IEC2),
        .ipc = atomic_reg_ptr_cast(&IPC11),
        .flag_mask = 0x00000800,
        .enable_mask = 0x00000800,
     },
};

struct dma_channel dma_channels[] =
{
    {
        .dma_reg = (const struct dma_register_map* const)(&DCH0CON),
        .dma_int = &dma_channel_interrupts[0],
        .assigned = false
    }, 
    {
        .dma_reg = (const struct dma_register_map* const)(&DCH1CON),
        .dma_int = &dma_channel_interrupts[1],
        .assigned = false
    },
    {
        .dma_reg = (const struct dma_register_map* const)(&DCH2CON),
        .dma_int = &dma_channel_interrupts[2],
        .assigned = false
    },
    {
        .dma_reg = (const struct dma_register_map* const)(&DCH3CON),
        .dma_int = &dma_channel_interrupts[3],
        .assigned = false
    }
};

void dma_init(void)
{
    // Disable DMA modules
    DMA_CLR_REG(DMA_DMACON_REG, DMA_ENABLE_MASK);
    
    // Disable interrupt on each channel
    for(int i = 0; i < DMA_NUMBER_OF_CHANNELS; ++i)
        atomic_reg_ptr_clr(dma_channels[i].dma_int->iec, dma_channels[i].dma_int->enable_mask);
    
    // Configure DMA
    DMA_DMACON_REG = DMA_DMACON_WORD;
    
    // Enable DMA module
    DMA_SET_REG(DMA_DMACON_REG, DMA_DMACON_WORD);
}

struct dma_channel* dma_construct(struct dma_config config)
{
    struct dma_channel* channel = NULL;
    
    // Search for an unused channel
    for(int i = 0; i < DMA_NUMBER_OF_CHANNELS; ++i) {
        if(!dma_channels[i].assigned) {
            channel = &dma_channels[i];
            break;
        }
    }
    
    // Configure channel, if found
    if(channel != NULL)
        dma_configure(channel, config);
    return channel;
}

void dma_destruct(struct dma_channel* channel)
{
    ASSERT(channel != NULL);
    
    atomic_reg_clr(channel->dma_reg->dchcon, DCHCON_CHEN_MASK);
    channel->assigned = false;
}

void dma_configure(struct dma_channel* channel, struct dma_config config)
{
    ASSERT(channel != NULL);
    const struct dma_register_map* const dma_reg = channel->dma_reg;
    const struct dma_interrupt_map* const dma_int = channel->dma_int;
    
    // Disable channel first
    atomic_reg_clr(dma_reg->dchcon, DCHCON_CHEN_MASK);
    
    // Configure DMA
    atomic_reg_value(dma_reg->dchssa) = DMA_PHY_ADDR(config.src_mem);
    atomic_reg_value(dma_reg->dchdsa) = DMA_PHY_ADDR(config.dst_mem);
    atomic_reg_value(dma_reg->dchssiz) = config.src_size;
    atomic_reg_value(dma_reg->dchdsiz) = config.dst_size;
    atomic_reg_value(dma_reg->dchcsiz) = config.cell_size;
    
    // Configure events    
    atomic_reg_clr(dma_reg->dchecon, DCHECON_AIRQEN_MASK);
    atomic_reg_clr(dma_reg->dchecon, DCHECON_SIRQEN_MASK);
    
    if(config.start_event.enable) {
        atomic_reg_set(dma_reg->dchecon, ((config.start_event.irq_vector << DCHECON_CHSIRQ_SHIFT) & DCHECON_CHSIRQ_MASK));
        atomic_reg_set(dma_reg->dchecon, DCHECON_SIRQEN_MASK);
    }
    
    if(config.abort_event.enable) {
        atomic_reg_set(dma_reg->dchecon, ((config.abort_event.irq_vector << DCHECON_CHAIRQ_SHIFT) & DCHECON_CHAIRQ_MASK));
        atomic_reg_set(dma_reg->dchecon, DCHECON_AIRQEN_MASK);
    }
    
    // Configure interrupts
    atomic_reg_ptr_clr(dma_int->iec, channel->dma_int->enable_mask);
    atomic_reg_ptr_clr(dma_int->ifs, channel->dma_int->flag_mask);
    atomic_reg_clr(dma_reg->dchint, DCHINT_CHBCIE_MASK);
    
    if(NULL != config.block_transfer_complete)
        atomic_reg_set(dma_reg->dchint, DCHINT_CHBCIE_MASK);

    // Has interrupts enabled?
    if(atomic_reg_value(dma_reg->dchint) & DCHINT_INTERRUPTS_MASK)
        atomic_reg_ptr_set(dma_int->iec, channel->dma_int->enable_mask);
}

void dma_enable_transfer(struct dma_channel* channel)
{
    ASSERT(channel != NULL);
    
    atomic_reg_set(channel->dma_reg->dchcon, DCHCON_CHEN_MASK);
}

bool dma_transferring(struct dma_channel *channel)
{
    ASSERT(channel != NULL);
    
    return atomic_reg_value(channel->dma_reg->dchcon) & DCHCON_CHEN_MASK;
}