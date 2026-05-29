#ifndef __FLOYD_STEINBERG_H__
#define __FLOYD_STEINBERG_H__

#include <stdint.h>

void floyd_steinberg( volatile uint8_t *source,
                      int width,
                      int height,
                      volatile uint8_t *destination,
                      volatile int16_t *error_array );

#endif
