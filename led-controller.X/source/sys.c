#include <xc.h>

void sys_lock(void)
{
    SYSKEY = 0x33333333;
}

void sys_unlock(void)
{
	SYSKEY = 0x33333333;
	SYSKEY = 0xAA996655;
	SYSKEY = 0x556699AA;
}

void sys_cpu_early_init(void)
{
     sys_unlock();
    OSCCONbits.FRCDIV = 1; // Divided by 2, default
    OSCCONbits.CLKLOCK = 1; // Lock clock and PLL selections
    OSCCONbits.SLPEN = 0; // Enter idle mode upon WAIT instruction
    OSCCONbits.UFRCEN = 0; // Use primary oscillator or USB PLL as USB clock source
    OSCCONbits.SOSCEN = 0; // Disable secondary oscillator
    OSCTUNbits.TUN = 0; // Calibrated frequency
    sys_lock();
    
    while(OSCCONbits.CF); // Clock failure
}