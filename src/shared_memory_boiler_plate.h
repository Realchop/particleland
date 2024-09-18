#ifndef shm_boiler_plate
#define shm_boiler_plate
#include <unistd.h>

void randname(char *buf);

int create_shm_file(void);

int allocate_shm_file(size_t size);

#endif
