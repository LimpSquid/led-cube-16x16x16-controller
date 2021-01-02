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
void dma_configure_src(struct dma_channel* channel, const void* mem, unsigned short size);
void dma_configure_dst(struct dma_channel* channel, const void* mem, unsigned short size);
void dma_configure_cell(struct dma_channel* channel, unsigned short size);
void dma_configure_start_event(struct dma_channel* channel, struct dma_irq event);
void dma_configure_abort_event(struct dma_channel* channel, struct dma_irq event);
void dma_enable_transfer(struct dma_channel* channel);
bool dma_busy(struct dma_channel* channel);
bool dma_ready(struct dma_channel* channel);

#endif	/* DMA_H */