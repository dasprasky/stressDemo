#ifndef _POLYFIT_H_
#define _POLYFIT_H_
#define POLYNOMIAL_MAX_ORDER    (10)

#include <stdint.h>

void polyvals(float x[], float y[], float a[], uint32_t N, uint8_t order);
void polyfit(float x[], float y[], float a[], uint32_t N, uint8_t order);
#endif