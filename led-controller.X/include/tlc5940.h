#ifndef TLC5940_H
#define	TLC5940_H

#include <stdbool.h>

bool tlc5940_busy(void);
bool tlc5940_ready(void);
bool tlc5940_update(void);
bool tlc5940_set_latch_callback(void (*callback)(void));
void tlc5940_write_grayscale(unsigned int device, unsigned int channel, unsigned short value);

#endif	/* TLC5940_H */