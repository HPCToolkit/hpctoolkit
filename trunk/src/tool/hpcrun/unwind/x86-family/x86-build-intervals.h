#ifndef x86_build_intervals_h
#define x86_build_intervals_h

#include "x86-unwind-interval.h"

interval_status 
x86_build_intervals(char *ins, unsigned int len, int noisy);

#endif
