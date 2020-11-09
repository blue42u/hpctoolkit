#ifndef MMAP_H
#define MMAP_H

#include <stdlib.h>

__attribute__((visibility("default")))
void* hpcrun_mmap_anon(size_t size);
__attribute__((visibility("default")))
void hpcrun_mmap_init(void);

#endif // MMAP_H
