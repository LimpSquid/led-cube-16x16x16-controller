#ifndef LAYER_H
#define	LAYER_H

#include <stdbool.h>

bool layer_busy(void);
bool layer_ready(void);
bool layer_receive_frame(void);


#endif	/* LAYER_H */