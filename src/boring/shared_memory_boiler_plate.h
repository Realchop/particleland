#ifndef SHM_BOILER_PLATE_H
#define SHM_BOILER_PLATE_H
#include <unistd.h>

void randname(char *buf);

int create_shm_file(void);

int allocate_shm_file(size_t size);

#endif
