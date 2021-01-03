#ifndef SPI_H
#define	SPI_H

#include <stdbool.h>

enum spicon_flag
{
    SPI_FRMEN                   = (1 << 31),
    SPI_FRMSYNC                 = (1 << 30),
    SPI_FRMPOL                  = (1 << 29),
    SPI_MSSEN                   = (1 << 28),
    SPI_FRMSYPW                 = (1 << 27),
    SPI_FRMCNT32                = (1 << 26) | (1 << 24),
    SPI_FRMCNT16                = (1 << 26),
    SPI_FRMCNT8                 = (1 << 25) | (1 << 24),
    SPI_FRMCNT4                 = (1 << 25),
    SPI_FRMCNT2                 = (1 << 24),
    SPI_FRMCNT                  = 0,
    SPI_MCLKSEL                 = (1 << 23),
    SPI_SPIFE                   = (1 << 17),
    SPI_ENHBUF                  = (1 << 16),
    SPI_ON                      = (1 << 15),
    SPI_SIDL                    = (1 << 13),
    SPI_DISSDO                  = (1 << 12),
    SPI_MODE32                  = (1 << 11),
    SPI_MODE16                  = (1 << 10),
    SPI_MODE8                   = 0,
    SPI_SMP                     = (1 << 9),
    SPI_CKE                     = (1 << 8),
    SPI_SSEN                    = (1 << 7),
    SPI_CKP                     = (1 << 6),
    SPI_MSTEN                   = (1 << 5),
    SPI_DISSDI                  = (1 << 4),
    SPI_STXISEL_NOT_FULL        = (1 << 3) | (1 << 2),
    SPI_STXISEL_EMPTY_ONE_HALF  = (1 << 3),
    SPI_STXISEL_EMPTY           = (1 << 2),
    SPI_STXISEL_COMPLETE        = 0,
    SPI_SRXISEL_FULL            = (1 << 1) | (1 << 0),
    SPI_SRXISEL_FULL_ONE_HALF   = (1 << 1),
    SPI_SRXISEL_NOT_EMPTY       = (1 << 0),
    SPI_SRXISEL_EMPTY           = 0
};

enum spi_channel
{
    SPI_CHANNEL1 = 0,
    SPI_CHANNEL2,
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