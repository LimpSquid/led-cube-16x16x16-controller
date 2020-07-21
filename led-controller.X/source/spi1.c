#include "../include/spi1.h"
#include "../include/kernel_task.h"
#include "../include/assert.h"
#include "../include/sys.h"
#include <xc.h>

#define SPI1_SET_REG(reg, mask) (reg |= mask)
#define SPI1_CLR_REG(reg, mask) (reg &= ~mask)

#define SPI1_BAUDRATE           40000000LU          

#define SPI1_INTERRUPT_REG      IEC1
#define SPI1_CON_REG            SPI1CON
#define SPI1_CON2_REG           SPI1CON2
#define SPI1_BRG_REG            SPI1BRG

#define SPI1_CON_WORD           0x0070
#define SPI1_CON2_WORD          0x0000
#define SPI1_BRG_WORD           ((SYS_PB_CLOCK / (SPI1_BAUDRATE << 1)) - 1)

#define SPI1_INTERRUPT_MASK     0x0038
#define SPI1_ENABLE_MASK        0x8000

static int spi1_rtask_init(void);
static void spi1_rtask_execute(void);
KERN_QUICK_RTASK(spi1, spi1_rtask_init, spi1_rtask_execute)

static int spi1_rtask_init(void)
{
    // Disable SPI1 interrupt and module
    SPI1_CLR_REG(SPI1_CON_REG, SPI1_ENABLE_MASK);
    SPI1_CLR_REG(SPI1_INTERRUPT_REG, SPI1_INTERRUPT_MASK);

    // Set the baudrate generator
    SPI1_BRG_REG = SPI1_BRG_WORD;
    
    // Configure SPI1
    SPI1_CON_REG = SPI1_CON_WORD;
    SPI1_CON2_REG = SPI1_CON2_WORD;
    
    // Enable SPI1 module
    SPI1_SET_REG(SPI1_CON_REG, SPI1_ENABLE_MASK);
}

static void spi1_rtask_execute(void)
{
    
}