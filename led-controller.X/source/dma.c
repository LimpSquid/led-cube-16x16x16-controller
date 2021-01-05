#include "../include/dma.h"
#include "../include/assert.h"
#include "../include/register.h"
#include "../include/toolbox.h"
#include <xc.h>
#include <sys/attribs.h>

#define DMA_PHY_ADDR(virt)              ((int)virt < 0 ? ((int)virt & 0x1FFFFFFFL) : (unsigned int)((unsigned char*)virt + 0x40000000L))
#define DMA_NUMBER_OF_CHANNELS          (sizeof(dma_channels) / sizeof(dma_channels[0]))
#define DMA_INTERRUPT_PRIORITY          0x3
         
#define DMA_DMACON_REG                  DMACON

#define DMA_DMACON_WORD                 BIT(15)

#define DMA_DCHCON_CHEN_MASK            BIT(7)
#define DMA_DCHCON_CHBUSY_MASK          BIT(15)
#define DMA_DCHECON_CHAIRQ_MASK         MASK(0xff, 16)
#define DMA_DCHECON_CHSIRQ_MASK         MASK(0xff, 8)
#define DMA_DCHECON_AIRQEN_MASK         BIT(3)
#define DMA_DCHECON_SIRQEN_MASK         BIT(4)
#define DMA_DCHINT_CHBCIE_MASK          BIT(19)
#define DMA_DCHINT_CHBCIF_MASK          BIT(3)
#define DMA_DCHINT_ENABLE_BITS_MASK     MASK(0xffff, 8)

#define DMA_DCHECON_CHAIRQ_SHIFT        16
#define DMA_DCHECON_CHSIRQ_SHIFT        8

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
    
    unsigned int mask;
    unsigned int priority_mask;
    unsigned char priority_shift;
};

struct dma_channel
{
    const struct dma_register_map* const dma_reg;
    const struct dma_interrupt_map* const dma_int;
    
    void (*block_transfer_complete)(struct dma_channel*);
    bool assigned;
};

static void dma_handle_interrupt(struct dma_channel* channel);

static const struct dma_interrupt_map dma_channel_interrupts[] =
{
    { 
        .ifs = atomic_reg_ptr_cast(&IFS2),
        .iec = atomic_reg_ptr_cast(&IEC2),
        .ipc = atomic_reg_ptr_cast(&IPC10),
        .mask = BIT(8),
        .priority_mask = MASK(0x7, 18),
        .priority_shift = 18,
    },
    { 
        .ifs = atomic_reg_ptr_cast(&IFS2),
        .iec = atomic_reg_ptr_cast(&IEC2),
        .ipc = atomic_reg_ptr_cast(&IPC10),
        .mask = BIT(9),
        .priority_mask = MASK(0x7, 26),
        .priority_shift = 26,
    },
    { 
        .ifs = atomic_reg_ptr_cast(&IFS2),
        .iec = atomic_reg_ptr_cast(&IEC2),
        .ipc = atomic_reg_ptr_cast(&IPC11),
        .mask = BIT(10),
        .priority_mask = MASK(0x7, 2),
        .priority_shift = 2,
    },
    { 
        .ifs = atomic_reg_ptr_cast(&IFS2),
        .iec = atomic_reg_ptr_cast(&IEC2),
        .ipc = atomic_reg_ptr_cast(&IPC11),
        .mask = BIT(11),
        .priority_mask = MASK(0x7, 10),
        .priority_shift = 10,
    },
};

static struct dma_channel dma_channels[] =
{
    {
        .dma_reg = (const struct dma_register_map* const)(&DCH0CON),
        .dma_int = &dma_channel_interrupts[0],
        .assigned = false,
    }, 
    {
        .dma_reg = (const struct dma_register_map* const)(&DCH1CON),
        .dma_int = &dma_channel_interrupts[1],
        .assigned = false,
    },
    {
        .dma_reg = (const struct dma_register_map* const)(&DCH2CON),
        .dma_int = &dma_channel_interrupts[2],
        .assigned = false,
    },
    {
        .dma_reg = (const struct dma_register_map* const)(&DCH3CON),
        .dma_int = &dma_channel_interrupts[3],
        .assigned = false,
    }
};

void dma_init(void)
{
    // Disable interrupt on each channel
    for(int i = 0; i < DMA_NUMBER_OF_CHANNELS; ++i)
        atomic_reg_ptr_clr(dma_channels[i].dma_int->iec, dma_channels[i].dma_int->mask);
    
    // Configure DMA
    DMA_DMACON_REG = DMA_DMACON_WORD;
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
    if(NULL != channel)
        dma_configure(channel, config);
    return channel;
}

void dma_destruct(struct dma_channel* channel)
{
    ASSERT(NULL != channel);
    
    atomic_reg_clr(channel->dma_reg->dchcon, DMA_DCHCON_CHEN_MASK);
    channel->assigned = false;
}

void dma_configure(struct dma_channel* channel, struct dma_config config)
{    
    ASSERT(NULL != channel);
    const struct dma_register_map* const dma_reg = channel->dma_reg;
    const struct dma_interrupt_map* const dma_int = channel->dma_int;
    
    // Disable channel first
    atomic_reg_clr(dma_reg->dchcon, DMA_DCHCON_CHEN_MASK);
    
    // Configure DMA
    atomic_reg_value(dma_reg->dchssa) = DMA_PHY_ADDR(config.src_mem);
    atomic_reg_value(dma_reg->dchdsa) = DMA_PHY_ADDR(config.dst_mem);
    atomic_reg_value(dma_reg->dchssiz) = config.src_size;
    atomic_reg_value(dma_reg->dchdsiz) = config.dst_size;
    atomic_reg_value(dma_reg->dchcsiz) = config.cell_size;
    
    // Configure events
    dma_configure_start_event(channel, config.start_event);
    dma_configure_abort_event(channel, config.abort_event);
    
    // Configure interrupts
    atomic_reg_ptr_clr(dma_int->iec, dma_int->mask);
    atomic_reg_ptr_clr(dma_int->ifs, dma_int->mask);
    atomic_reg_ptr_clr(dma_int->ipc, dma_int->priority_mask);
    atomic_reg_clr(dma_reg->dchint, DMA_DCHINT_CHBCIE_MASK);
    
    if(NULL != config.block_transfer_complete) {
        channel->block_transfer_complete = config.block_transfer_complete;
        atomic_reg_ptr_set(dma_int->ipc, MASK(DMA_INTERRUPT_PRIORITY, dma_int->priority_shift) & dma_int->priority_mask);
        atomic_reg_set(dma_reg->dchint, DMA_DCHINT_CHBCIE_MASK);
    }

    // Has interrupts enabled?
    if(atomic_reg_value(dma_reg->dchint) & DMA_DCHINT_ENABLE_BITS_MASK)
        atomic_reg_ptr_set(dma_int->iec, dma_int->mask);
}
void dma_configure_src(struct dma_channel* channel, const void* mem, unsigned short size)
{
    ASSERT(NULL != channel);
    const struct dma_register_map* const dma_reg = channel->dma_reg;
    
    atomic_reg_value(dma_reg->dchssa) = DMA_PHY_ADDR(mem);
    atomic_reg_value(dma_reg->dchssiz) = size;
}

void dma_configure_dst(struct dma_channel* channel, const void* mem, unsigned short size)
{
    ASSERT(NULL != channel);
    const struct dma_register_map* const dma_reg = channel->dma_reg;
    
    atomic_reg_value(dma_reg->dchdsa) = DMA_PHY_ADDR(mem);
    atomic_reg_value(dma_reg->dchdsiz) = size;
}

void dma_configure_cell(struct dma_channel* channel, unsigned short size)
{
    ASSERT(NULL != channel);

    atomic_reg_value(channel->dma_reg->dchcsiz) = size;
}

void dma_configure_start_event(struct dma_channel* channel, struct dma_event event)
{
    ASSERT(NULL != channel);
    const struct dma_register_map* const dma_reg = channel->dma_reg;
    
    atomic_reg_clr(dma_reg->dchecon, DMA_DCHECON_CHSIRQ_MASK);
    atomic_reg_clr(dma_reg->dchecon, DMA_DCHECON_SIRQEN_MASK);
    if(event.enable) {
        atomic_reg_set(dma_reg->dchecon, MASK_SHIFT(event.irq_vector, DMA_DCHECON_CHSIRQ_SHIFT) & DMA_DCHECON_CHSIRQ_MASK);
        atomic_reg_set(dma_reg->dchecon, DMA_DCHECON_SIRQEN_MASK);
    }
}

void dma_configure_abort_event(struct dma_channel* channel, struct dma_event event)
{
    ASSERT(NULL != channel);
    const struct dma_register_map* const dma_reg = channel->dma_reg;
    
    atomic_reg_clr(dma_reg->dchecon, DMA_DCHECON_CHAIRQ_MASK);
    atomic_reg_clr(dma_reg->dchecon, DMA_DCHECON_AIRQEN_MASK);
    if(event.enable) {
        atomic_reg_set(dma_reg->dchecon, MASK_SHIFT(event.irq_vector, DMA_DCHECON_CHAIRQ_SHIFT) & DMA_DCHECON_CHAIRQ_MASK);
        atomic_reg_set(dma_reg->dchecon, DMA_DCHECON_AIRQEN_MASK);
    }    
}

void dma_enable_transfer(struct dma_channel* channel)
{
    ASSERT(NULL != channel);
    
    atomic_reg_set(channel->dma_reg->dchcon, DMA_DCHCON_CHEN_MASK);
}

bool dma_busy(struct dma_channel* channel)
{
    ASSERT(NULL != channel);
    
    return atomic_reg_value(channel->dma_reg->dchcon) & DMA_DCHCON_CHBUSY_MASK;
}

bool dma_ready(struct dma_channel* channel)
{
    return !dma_busy(channel);
}

static void dma_handle_interrupt(struct dma_channel* channel)
{
    unsigned int int_flags = atomic_reg_value(channel->dma_reg->dchint);
    
    if(int_flags & DMA_DCHINT_CHBCIF_MASK) {
        channel->block_transfer_complete(channel);
        atomic_reg_clr(channel->dma_reg->dchint, DMA_DCHINT_CHBCIF_MASK);
    }
}

void __ISR(_DMA_0_VECTOR, IPL7AUTO) dma_interrupt0(void)
{
    static struct dma_channel *channel = &dma_channels[0];
    dma_handle_interrupt(channel);
}

void __ISR(_DMA_1_VECTOR, IPL7AUTO) dma_interrupt1(void)
{
    static struct dma_channel *channel = &dma_channels[1];
    dma_handle_interrupt(channel);
}

void __ISR(_DMA_2_VECTOR, IPL7AUTO) dma_interrupt2(void)
{
    static struct dma_channel *channel = &dma_channels[2];
    dma_handle_interrupt(channel);
}

void __ISR(_DMA_3_VECTOR, IPL7AUTO) dma_interrupt3(void)
{
    static struct dma_channel *channel = &dma_channels[3];
    dma_handle_interrupt(channel);
}