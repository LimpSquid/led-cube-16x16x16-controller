#include "../include/spi.h"
#include "../include/assert.h"
#include "../include/register.h"
#include "../include/sys.h"
#include "../include/dma.h"
#include <xc.h>
#include <stddef.h>

#define SPI_BRG(baudrate)           ((SYS_PB_CLOCK / (baudrate << 1)) - 1)
#define SPI_FIFO_DEPTH_MODE32       4
#define SPI_FIFO_DEPTH_MODE16       8
#define SPI_FIFO_DEPTH_MODE8        16
#define SPI_FIFO_SIZE_MODE32        4 // In bytes
#define SPI_FIFO_SIZE_MODE16        2 // In bytes
#define SPI_FIFO_SIZE_MODE8         1 // In bytes

#define SPI_SPICON_RESET_WORD       0x00000000

#define SPI_SPISTAT_SPITBF_MASK     0x00000002

struct spi_register_map
{
    atomic_reg(spicon);
    atomic_reg(spistat);
    atomic_reg(spibuf);
    atomic_reg(spibrg);
    atomic_reg(spicon2);
};

struct spi_interrupt_map
{
    atomic_reg_ptr(ifs);
    atomic_reg_ptr(iec);
    
    unsigned int fault_mask;
    unsigned int receive_mask;
    unsigned int transfer_mask;
    unsigned char fault_irq;
    unsigned char receive_irq;
    unsigned char transfer_irq;
};

static const struct spi_interrupt_map spi_module_interrupts[] =
{
    [SPI_CHANNEL1] = { 
        .ifs = atomic_reg_ptr_cast(&IFS1),
        .iec = atomic_reg_ptr_cast(&IEC1),
        .fault_mask = 0x00000008,
        .receive_mask = 0x00000010,
        .transfer_mask = 0x00000020,
        .fault_irq = _SPI1_ERR_IRQ,
        .receive_irq = _SPI1_RX_IRQ,
        .transfer_irq = _SPI1_TX_IRQ,
    },
    [SPI_CHANNEL2] = { 
        .ifs = atomic_reg_ptr_cast(&IFS2),
        .iec = atomic_reg_ptr_cast(&IEC2), 
        .fault_mask = 0x00200000,
        .receive_mask = 0x00400000,
        .transfer_mask = 0x00800000,
        .fault_irq = _SPI2_ERR_IRQ,
        .receive_irq = _SPI2_RX_IRQ,
        .transfer_irq = _SPI2_TX_IRQ,
    },
};

struct spi_module
{
    const struct spi_register_map* const spi_reg;
    const struct spi_interrupt_map* const spi_int;
    
    unsigned char fifo_depth;
    unsigned char fifo_size;
    bool assigned;
};

struct spi_module spi_modules[] =
{
    [SPI_CHANNEL1] = {
        .spi_reg = (const struct spi_register_map* const)(&SPI1CON),
        .spi_int = &spi_module_interrupts[SPI_CHANNEL1],
        .fifo_depth = SPI_FIFO_DEPTH_MODE8,
        .fifo_size = SPI_FIFO_SIZE_MODE8,
        .assigned = false,
    }, 
    [SPI_CHANNEL2] = {
        .spi_reg = (const struct spi_register_map* const)(&SPI2CON),
        .spi_int = &spi_module_interrupts[SPI_CHANNEL2],
        .fifo_depth = SPI_FIFO_DEPTH_MODE8,
        .fifo_size = SPI_FIFO_SIZE_MODE8,
        .assigned = false,
    }
};

struct spi_module* spi_construct(enum spi_channel channel, struct spi_config config)
{
    struct spi_module* module = &spi_modules[channel];
    
    // Already assigned
    if(module->assigned)
        return NULL;
    
    // Configure module
    spi_configure(module, config);
    return module;
}

void spi_destruct(struct spi_module* module)
{
    ASSERT(module != NULL);
    
    atomic_reg_clr(module->spi_reg->spicon, SPI_ON);
    module->assigned = false;
}

void spi_configure(struct spi_module* module, struct spi_config config)
{
    ASSERT(module != NULL);
    const struct spi_register_map* const spi_reg = module->spi_reg;
    const struct spi_interrupt_map* const spi_int = module->spi_int;
    
    // Disable module first
    atomic_reg_clr(spi_reg->spicon, SPI_ON);
    
    // Configure interrupts
    // @Todo: for now only disable...
    atomic_reg_ptr_clr(spi_int->iec, spi_int->fault_mask);
    atomic_reg_ptr_clr(spi_int->iec, spi_int->receive_mask);
    atomic_reg_ptr_clr(spi_int->iec, spi_int->transfer_mask);
    atomic_reg_ptr_clr(spi_int->ifs, spi_int->fault_mask);
    atomic_reg_ptr_clr(spi_int->ifs, spi_int->receive_mask);
    atomic_reg_ptr_clr(spi_int->ifs, spi_int->transfer_mask);
    
    // Configure SPI
    atomic_reg_value(spi_reg->spibrg) = SPI_BRG(config.baudrate);
    atomic_reg_value(spi_reg->spicon) = config.spicon_flags;
    module->fifo_depth = SPI_FIFO_DEPTH_MODE8;
    module->fifo_size = SPI_FIFO_SIZE_MODE8;
    
    if(config.spicon_flags & SPI_MODE32) {
        module->fifo_depth = SPI_FIFO_DEPTH_MODE32;
        module->fifo_size = SPI_FIFO_SIZE_MODE32;
    } else if(config.spicon_flags & SPI_MODE16) {
        module->fifo_depth = SPI_FIFO_DEPTH_MODE16;
        module->fifo_size = SPI_FIFO_SIZE_MODE16;
    }
}

void spi_configure_dma_src(struct spi_module* module, struct dma_channel* channel)
{
    ASSERT(module != NULL);
    ASSERT(channel != NULL);
    unsigned int spicon = atomic_reg_value(module->spi_reg->spicon);
    struct dma_irq start_event =
    {
        .enable = true,
        .irq_vector = module->spi_int->receive_irq,
    };
    
    struct dma_irq abort_event =
    {
        .enable = true,
        .irq_vector = module->spi_int->fault_irq,
    };
    
    dma_configure_src(channel, &module->spi_reg, 1); // one fifo_size per transfer
    dma_configure_cell(channel, module->fifo_size);
    dma_configure_start_event(channel, start_event);
    dma_configure_abort_event(channel, abort_event);
}

void spi_configure_dma_dst(struct spi_module* module, struct dma_channel* channel)
{
    ASSERT(module != NULL);
    ASSERT(channel != NULL);
    unsigned int spicon = atomic_reg_value(module->spi_reg->spicon);
    struct dma_irq start_event =
    {
        .enable = true,
        .irq_vector = module->spi_int->transfer_irq,
    };
    
    struct dma_irq abort_event =
    {
        .enable = true,
        .irq_vector = module->spi_int->fault_irq,
    };
    
    dma_configure_dst(channel, &module->spi_reg->spibuf, 1); // one fifo_size per transfer
    dma_configure_cell(channel, module->fifo_size);
    dma_configure_start_event(channel, start_event);
    dma_configure_abort_event(channel, abort_event);
}

void spi_enable(struct spi_module* module)
{
    ASSERT(module != NULL);
    
    atomic_reg_set(module->spi_reg->spicon, SPI_ON);
}

void spi_disable(struct spi_module* module)
{
    ASSERT(module != NULL);
    
    atomic_reg_clr(module->spi_reg->spicon, SPI_ON);
}

bool spi_transmit(struct spi_module* module, unsigned int* buffer, unsigned char size)
{
    ASSERT(module != NULL);
    ASSERT(atomic_reg_value(module->spi_reg->spicon) & SPI_ON);
    const struct spi_register_map* const spi_reg = module->spi_reg;
    bool result = false;
    
    if(size && atomic_reg_value(spi_reg->spicon) & SPI_MSTEN) {
        while(size--) {
            while(atomic_reg_value(spi_reg->spistat) & SPI_SPISTAT_SPITBF_MASK);
            atomic_reg_value(spi_reg->spibuf) = *buffer++;
        }
        result = true;
    }
    return result;
}