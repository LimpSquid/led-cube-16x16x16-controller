#ifndef KERNEL_CONFIG_H
#define	KERNEL_CONFIG_H

#include "sys.h"

#define KERN_TMR_REG        TMR5            // Hardware timer, must be a 16 bit timer
#define KERN_TMR_CFG_REG    T5CON           // Hardware timer configuration register
#define KERN_TMR_CFG_WORD   0xA040          // Hardware timer configuration word
#define KERN_TMR_PRESCALER  0x10            // Hardware timer prescaler
#define KERN_TMR_EN_BIT     (1 << 15)       // Hardware timer enable mask of the configuration word
#define KERN_TMR_CLK_FREQ   SYS_PB_CLOCK    // Hardware timer input frequency (can be calculated with SYS_CLK / PB_DIV)

#endif	/* KERNEL_CONFIG_H */

