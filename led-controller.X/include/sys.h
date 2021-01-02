#ifndef SYS_H
#define	SYS_H

#include <stdbool.h>
#include <xc.h>

#if defined(_SYS_CLK) && defined(_PB_DIV)
    #define SYS_PB_CLOCK                (_SYS_CLK / _PB_DIV)
#else
    #error "System peripheral bus clock could not be calculated, please define the _SYS_CLK and _PB_DIV." 
#endif

#define SYS_BONZO_IS_HUNGRY         true

#define sys_goodnight_bonzo()       WDTCONbits.ON = 0
#define sys_wakeup_bonzo()          WDTCONbits.ON = 1
#define sys_feed_bonzo()            WDTCONbits.WDTCLR = 1

void sys_lock(void);
void sys_unlock(void);
void sys_cpu_early_init(void);

#endif	/* SYS_H */