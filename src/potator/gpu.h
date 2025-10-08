#ifndef __GPU_H__
#define __GPU_H__

#include "types.h"
#include "supervision.h" // SV_*

void gpu_init(void);
void gpu_reset(void);
void gpu_done(void);
void gpu_set_map_func(SV_MapRGBFunc func);

#endif
