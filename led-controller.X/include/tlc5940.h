#ifndef TLC5940_H
#define	TLC5940_H

#include <stdbool.h>

bool tlc5940_busy(void);
bool tlc5940_ready(void);
bool tlc5940_update(void);

#endif	/* TLC5940_H */