#ifndef SPI_H
#define	SPI_H

#include "../include/toolbox.h"
#include <stdbool.h>

enum spicon_flag
{
    SPI_FRMEN                   = BIT(31),
    SPI_FRMSYNC                 = BIT(30),
    SPI_FRMPOL                  = BIT(29),
    SPI_MSSEN                   = BIT(28),
    SPI_FRMSYPW                 = BIT(27),
    SPI_FRMCNT32                = BIT(26) | BIT(24),
    SPI_FRMCNT16                = BIT(26),
    SPI_FRMCNT8                 = BIT(25) | BIT(24),
    SPI_FRMCNT4                 = BIT(25),
    SPI_FRMCNT2                 = BIT(24),
    SPI_FRMCNT                  = 0,
    SPI_MCLKSEL                 = BIT(23),
    SPI_SPIFE                   = BIT(17),
    SPI_ENHBUF                  = BIT(16),
    SPI_ON                      = BIT(15),
    SPI_SIDL                    = BIT(13),
    SPI_DISSDO                  = BIT(12),
    SPI_MODE32                  = BIT(11),
    SPI_MODE16                  = BIT(10),
    SPI_MODE8                   = 0,
    SPI_SMP                     = BIT(9),
    SPI_CKE                     = BIT(8),
    SPI_SSEN                    = BIT(7),
    SPI_CKP                     = BIT(6),
    SPI_MSTEN                   = BIT(5),
    SPI_DISSDI                  = BIT(4),
    SPI_STXISEL_NOT_FULL        = BIT(3) | BIT(2),
    SPI_STXISEL_EMPTY_ONE_HALF  = BIT(3),
    SPI_STXISEL_EMPTY           = BIT(2),
    SPI_STXISEL_COMPLETE        = 0,
    SPI_SRXISEL_FULL            = BIT(1) | BIT(0),
    SPI_SRXISEL_FULL_ONE_HALF   = BIT(1),
    SPI_SRXISEL_NOT_EMPTY       = BIT(0),
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
bool spi_transmit_mode32(struct spi_module* module, unsigned int* buffer, unsigned int size);
bool spi_transmit_mode8(struct spi_module* module, unsigned char* buffer, unsigned int size);

#endif	/* SPI_H */