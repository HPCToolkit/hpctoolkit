#ifndef MMAP_H
#define MMAP_H

#include <stdlib.h>

void* hpcrun_mmap_anon(size_t size);
void hpcrun_mmap_init(void);

#endif // MMAP_H
