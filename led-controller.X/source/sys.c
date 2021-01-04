#include "../include/sys.h"
#include "../include/toolbox.h"
#include <xc.h>

#define SYS_OSCCON_REG                  OSCCON
#define SYS_INTCON_REG                  INTCON
#define SYS_CFGCON_REG                  CFGCON

#define SYS_OSCCON_CLK_LCK_MASK         BIT(7)
#define SYS_OSCCON_CF_MASK              BIT(3)
#define SYS_INTCON_MVEC_MASK            BIT(12)
#define SYS_CFGCON_IOLOCK               BIT(12)

void sys_lock(void)
{
    REG_SET(SYS_CFGCON_REG, SYS_CFGCON_IOLOCK);
    SYSKEY = 0x33333333;
}

void sys_unlock(void)
{
	SYSKEY = 0x33333333;
	SYSKEY = 0xAA996655;
	SYSKEY = 0x556699AA;
    REG_CLR(SYS_CFGCON_REG, SYS_CFGCON_IOLOCK);
}

void sys_enable_global_interrupt()
{
    __asm("ei");
}

void sys_disable_global_interrupt()
{
    __asm("di");
}

void sys_cpu_early_init(void)
{   
    // Wait for valid clock
    while(SYS_OSCCON_REG & SYS_OSCCON_CF_MASK);
    
    // Configure clock
    sys_unlock();
    REG_SET(SYS_OSCCON_REG, SYS_OSCCON_CLK_LCK_MASK); // Lock clock and PLL selections
    sys_lock();
    
    // Configure other stuff
    REG_SET(SYS_INTCON_REG, SYS_INTCON_MVEC_MASK);
}