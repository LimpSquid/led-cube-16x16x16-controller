#ifndef TOOLBOX_H
#define	TOOLBOX_H

#define MASK(mask, shift)       MASK_SHIFT(mask, shift)
#define MASK_SHIFT(mask, shift) ((unsigned int)mask << shift)

#define BIT(shift)              BIT_SHIFT(shift)
#define BIT_SHIFT(shift)        (1U << shift)

#define REG_SET(reg, mask)      (reg |= mask)
#define REG_CLR(reg, mask)      (reg &= ~mask)
#define REG_INV(reg, mask)      (reg ^= mask)

#endif	/* TOOLBOX_H */