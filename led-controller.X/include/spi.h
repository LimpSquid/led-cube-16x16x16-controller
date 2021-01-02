#ifndef SPI_H
#define	SPI_H

#include <stdbool.h>

enum spicon_flag
{
    SF_FRMEN                    = (1 << 31),
    SF_FRMSYNC                  = (1 << 30),
    SF_FRMPOL                   = (1 << 29),
    SF_MSSEN                    = (1 << 28),
    SF_FRMSYPW                  = (1 << 27),
    SF_FRMCNT32                 = (1 << 26) | (1 << 24),
    SF_FRMCNT16                 = (1 << 26),
    SF_FRMCNT8                  = (1 << 25) | (1 << 24),
    SF_FRMCNT4                  = (1 << 25),
    SF_FRMCNT2                  = (1 << 24),
    SF_FRMCNT                   = 0,
    SF_MCLKSEL                  = (1 << 23),
    SF_SPIFE                    = (1 << 17),
    SF_ENHBUF                   = (1 << 16),
    SF_ON                       = (1 << 15),
    SF_SIDL                     = (1 << 13),
    SF_DISSDO                   = (1 << 12),
    SF_MODE32                   = (1 << 11),
    SF_MODE16                   = (1 << 10),
    SF_MODE8                    = 0,
    SF_SMP                      = (1 << 9),
    SF_CKE                      = (1 << 8),
    SF_SSEN                     = (1 << 7),
    SF_CKP                      = (1 << 6),
    SF_MSTEN                    = (1 << 5),
    SF_DISSDI                   = (1 << 4),
    SF_STXISEL_NOT_FULL         = (1 << 3) | (1 << 2),
    SF_STXISEL_EMPTY_ONE_HALF   = (1 << 3),
    SF_STXISEL_EMPTY            = (1 << 2),
    SF_STXISEL_COMPLETE         = 0,
    SF_SRXISEL_FULL             = (1 << 1) | (1 << 0),
    SF_SRXISEL_FULL_ONE_HALF    = (1 << 1),
    SF_SRXISEL_NOT_EMPTY        = (1 << 0),
    SF_SRXISEL_EMPTY            = 0
};

enum spi_channel
{
    SC_CHANNEL1 = 0,
    SC_CHANNEL2,
}; 

struct dma_channel;
struct spi_module;
struct spi_config
{    
    enum spicon_flag spicon_flags;
    unsigned int baudrate;
};

struct spi_module* spi_construct(enum spi_channel channel, struct spi_config config);
void spi_destruct(struct spi_module* module);
void spi_configure(struct spi_module* module, struct spi_config config);
void spi_configure_dma_src(struct spi_module* module, struct dma_channel* channel);
void spi_configure_dma_dst(struct spi_module* module, struct dma_channel* channel);
void spi_enable(struct spi_module* module);
void spi_disable(struct spi_module* module);
bool spi_transmit(struct spi_module* module, unsigned int* buffer, unsigned char size);

#endif	/* SPI_H */