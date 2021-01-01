#ifndef DMA_H
#define	DMA_H

#include <stdbool.h>

struct dma_channel;
struct dma_irq
{
    unsigned char irq_vector;
    bool enable;
};

struct dma_config
{
    void (*block_transfer_complete)(struct dma_channel*);
    
    const void* src_mem;
    const void* dst_mem;
    unsigned short src_size;
    unsigned short dst_size;
    unsigned short cell_size;
    
    struct dma_irq abort_event;
    struct dma_irq start_event;
};

void dma_init(void);
struct dma_channel* dma_construct(struct dma_config config);
void dma_destruct(struct dma_channel* channel);
void dma_configure(struct dma_channel* channel, struct dma_config config);
void dma_enable_transfer(struct dma_channel *channel);
bool dma_transferring(struct dma_channel *channel);

#endif	/* DMA_H */